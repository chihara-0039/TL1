#pragma once
#include "Object3d.h"
#include "Input.h"
#include "StageMap.h"
#include "SkinnedObject.h"
#include "../Collision/WorldCollisionBox.h"
#include <memory>
#include <string>
#include <vector>

class DirectXCommon;

// ==============================================================
//  Player
//
//  プレイヤーキャラクターの物理・入力・描画を管理するクラス。
//
//  ─── 内部コンポーネント ───────────────────────────────────
//  object_       : 通常 OBJ モデル用の Object3d (静的モデル)
//  skinnedObject_: glTF スキニングモデル用 (アニメーションあり)
//  isSkinned_    : どちらを使っているかのフラグ
//
//  Initialize() か InitializeWithSkinnedGltf() のどちらかで初期化し、
//  後から ApplyModelToPlayer() 経由で切り替えることもできる。
//
//  ─── 物理シミュレーション ────────────────────────────────
//  簡易的な AABB (軸平行バウンディングボックス) を使ったブロック衝突判定。
//  velocity_.y に重力を毎フレーム加算し、ブロックに接触したらゼロにする。
//  ジャンプは velocity_.y = jumpSpeed_ を代入するだけのシンプルな実装。
//
//  ─── 座標系 ───────────────────────────────────────────────
//  position_ : キャラクターの足元 (AABB の下端) を基準とした世界座標。
//  radius_   : AABB の半径 (x,y,z)。モデルのサイズに合わせて調整すること。
//
//  ─── 操作キー ─────────────────────────────────────────────
//  A / D キー    : 左右移動 (カメラ方向 cameraRotY に合わせて向きを変える)
//  SPACE キー    : ジャンプ (長押しで高く飛べる追加加速あり)
//  B キー        : インベントリ開閉 (GamePlay_BlockPlace モードへ移行)
// ==============================================================
class Player {
public:
    Player()  = default;
    ~Player();

    // -------------------------------------------------------
    //  Initialize : OBJ モデルを使って初期化する (スキニングなし)。
    //  軽量でシンプル。プロトタイプや OBJ モデルのテストに使用。
    // -------------------------------------------------------
    void Initialize(Object3dCommon* common, Model* model);

    // -------------------------------------------------------
    //  InitializeWithSkinnedGltf : glTF モデルを使って初期化する。
    //  gltfPath に .gltf/.glb ファイルのパスを渡す。
    //  内部で SkinnedObject を生成し、isSkinned_ = true になる。
    // -------------------------------------------------------
    void InitializeWithSkinnedGltf(
        Object3dCommon*    common,
        DirectXCommon*     dxCommon,
        const std::string& gltfPath,
        TextureManager*    textureManager);

    // -------------------------------------------------------
    //  InitializeWithDefaultSkinned : 組み込みヒューマノイドで初期化する。
    //  Blender モデルが用意できていない段階でのテスト用。
    // -------------------------------------------------------
    void InitializeWithDefaultSkinned(
        Object3dCommon* common,
        DirectXCommon*  dxCommon,
        TextureManager* textureManager);

    // -------------------------------------------------------
    //  Update : 毎フレームの入力処理・物理演算・当たり判定を実行する。
    //
    //  処理順:
    //    1. 入力から速度 (velocity_) を決定
    //    2. velocity_ に重力を加算
    //    3. 速度を position_ に加算して新しい位置を仮決定
    //    4. CheckCollision() でブロックと衝突しているか確認
    //    5. 衝突していたら押し戻し・速度をゼロに
    //    6. UpdateTransform() で Object3d の行列を確定
    //
    //  cameraRotY : カメラの Y 軸回転角 (ラジアン)。
    //               プレイヤーの移動方向をカメラ相対にするために使用。
    // -------------------------------------------------------
    void Update(const Input* input, StageMap& map, float cameraRotY,
                const Matrix4x4& lightVP, DirectXCommon* dxCommon);

    // -------------------------------------------------------
    //  UpdateTransform : Object3d の行列のみを更新する (物理計算なし)。
    //  GamePlay 以外のモード (StageEditor など) で位置だけ反映したい場合に使う。
    // -------------------------------------------------------
    void UpdateTransform(const Matrix4x4& lightVP);

    // -------------------------------------------------------
    //  CrumbleUpdate : 崩れる足場ブロック (Crumble) の更新処理。
    //  プレイヤーが乗っているブロックに崩れるタイマーをセットする。
    // -------------------------------------------------------
    void CrumbleUpdate(StageMap& map);

    // -------------------------------------------------------
    //  Draw / DrawShadow / DrawHighlight
    //  Draw         : 通常描画 (OBJ または SkinnedObject)
    //  DrawShadow   : シャドウマップへの影描画
    //  DrawHighlight: 壁越し表示用シルエット描画
    //                 (Object3dCommon::PreDrawPlayerHighlight() 後に呼ぶ)
    // -------------------------------------------------------
    void Draw();
    void DrawShadow(const Matrix4x4& lightViewProjection);
    void DrawHighlight();

    // ── 座標・回転 ────────────────────────────────────────

    void SetPosition(const Vector3& pos) { position_ = pos; }
    const Vector3& GetPosition() const   { return position_; }

    void SetRotation(const Vector3& rot) { rotation_ = rot; }
    const Vector3& GetRotation() const   { return rotation_; }

    // -------------------------------------------------------
    //  SetCamera : 保持している Object3d / SkinnedObject 両方に
    //  View / Projection 行列を渡す。毎フレーム Update() より前に呼ぶ。
    // -------------------------------------------------------
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        if (object_)        object_->SetCamera(view, projection);
        if (skinnedObject_) skinnedObject_->SetCamera(view, projection);
    }

    // ── コライダー ────────────────────────────────────────

    /// <summary>AABB の半径 (x:左右 / y:上下 / z:奥行き)。衝突判定の大きさ。</summary>
    const Vector3& GetRadius() const { return radius_; }
    /// <summary>
    /// StageMap外に存在するワールドAABB一覧を設定する。
    /// ポインタ先はPlayerより長く生存し、ゲーム実行中は有効である必要がある。
    /// </summary>
    void SetExternalCollisionBoxes(const std::vector<WorldCollisionBox>* boxes) {
        externalCollisionBoxes_ = boxes;
    }

    // ── ドアワープ ────────────────────────────────────────

    /// <summary>ドアブロックに隣接している場合にワープ先にテレポートする</summary>
    void DoorWarp(const StageMap& map);

    // ── リスポーン ────────────────────────────────────────

    /// <summary>リスポーン地点をセット (スタートブロックの位置から計算して渡す)</summary>
    void SetRespawnPosition(const Vector3& pos) { respawnPosition_ = pos; }

    const Vector3& GetRespawnPosition() const { return respawnPosition_; }


    /// <summary>プレイヤーをリスポーン地点に戻し、速度をリセットする</summary>
    void Respawn();

    // ── ゲーム特殊ブロック処理 ────────────────────────────

    /// <summary>P スイッチ (コイン⇔ブロック変換) の更新処理</summary>
    void PSwitchUpdate(StageMap& map);

    void OnOffSwitchUpdate(StageMap& map);
  
    /// <summary>鍵ブロックの取得判定と所持フラグの更新</summary>
    void KeyUpdate(StageMap& map);

    void KeyBlockUIUpdate(StageMap& map);

    // ── 状態フラグゲッター ────────────────────────────────

    /// <summary>ドアの近くにいるか (UI 表示用)</summary>
    bool IsNearDoor() const { return isNearDoor_; }

    // Pスイッチの近くにいるか
    bool IsNearPSwitch() const { return isNearPSwitch_; }
    const Vector3& GetNearPSwitchWorldPos() const { return nearPSwitchWorldPos_; }

    // 鍵の近くにいるか
    bool IsNearKey() const { return isNearKey_; }
    const Vector3& GetNearKeyWorldPos() const { return nearKeyWorldPos_; }

    // 鍵ブロックの近くにいるか
    bool IsNearKeyBlock() const { return isNearKeyBlock_; }
    const Vector3& GetNearKeyBlockWorldPos() const { return nearKeyBlockWorldPos_; }

    /// <summary>ドアのワールド座標 (UI のワールド→スクリーン変換に使用)</summary>
    const Vector3& GetNearDoorWorldPos() const { return nearDoorWorldPos_; }

    /// <summary>はしごに掴まっているか</summary>
    bool IsOnLadder() const { return isOnLadder_; }

    /// <summary>はしごのワールド座標 (UI 表示用)</summary>
    const Vector3& GetLadderWorldPos() const { return ladderWorldPos_; }

    /// <summary>鍵を持っているか (鍵ブロックで扉を開くために必要)</summary>
    bool HasKey() const  { return hasKey_; }
    void SetHasKey(bool hasKey) { hasKey_ = hasKey; }
    void SetGlow(float glow) {
        if (object_) {
            object_->SetEmissive(glow);
        }
        if (skinnedObject_ && skinnedObject_->GetObject3d()) {
            skinnedObject_->GetObject3d()->SetEmissive(glow);
        }
    }
    void SetEnvironmentCoefficient(float coefficient) {
        if (object_) {
            object_->SetEnvironmentCoefficient(coefficient);
        }
        if (skinnedObject_ && skinnedObject_->GetObject3d()) {
            skinnedObject_->GetObject3d()->SetEnvironmentCoefficient(coefficient);
        }
    }

    SkinnedObject* GetSkinnedObject() const { return skinnedObject_.get(); }
    bool IsSkinned() const { return isSkinned_; }

private:
    enum class AnimationState {
        Idle,
        Walk,
        Run,
        Jump,
        Ladder,
    };

    // -------------------------------------------------------
    //  CheckCollision : pos でブロックと衝突しているか判定する。
    //  radius_ の AABB と StageMap の各ブロックを比較する。
    //  isSolid フラグが true のブロックと衝突とみなす。
    // -------------------------------------------------------
    bool CheckCollision(const Vector3& pos, StageMap& map);
    /// <summary>指定位置のプレイヤーAABBが外部ワールドAABBと重なるか判定する。</summary>
    bool CheckExternalCollision(const Vector3& pos) const;
    bool IsJumpTriggered() const;
    bool IsInteractTriggered() const;
    bool IsRunInputActive(const Vector3& inputDir) const;
    AnimationState ResolveAnimationState(bool hasMoveInput, bool isRunInput) const;
    void ApplySkinnedAnimation(AnimationState state, bool isMoving);

private:
    // ── 描画コンポーネント ────────────────────────────────
    std::unique_ptr<Object3d>      object_;        // OBJ モデル用 (通常)
    std::unique_ptr<SkinnedObject> skinnedObject_; // glTF スキニング用
    bool isSkinned_ = false;                       // スキニングモードかどうか
    AnimationState animationState_ = AnimationState::Idle; // 現在のプレイヤーアニメーション状態

    // ── トランスフォーム ──────────────────────────────────
    Vector3 position_ = { 0, 0, 0 }; // 足元基準のワールド座標
    Vector3 rotation_ = { 0, 0, 0 }; // 回転 (主に Y 軸: 向き)
    Vector3 velocity_ = { 0, 0, 0 }; // 速度 (重力・ジャンプ・移動の合算)
    const std::vector<WorldCollisionBox>* externalCollisionBoxes_ = nullptr; ///< Blender配置物などの外部当たり判定。所有しない。
    Vector3 respawnPosition_ = { 0.0f, 1.5f, 0.0f }; // リスポーン地点

    // ── コライダー ────────────────────────────────────────
    // モデルのサイズに合わせて調整すること
    Vector3 radius_ = { 0.35f, 0.8f, 0.35f }; // AABB 半径 (左右 / 上下 / 奥行き)

    // ── 物理パラメータ ────────────────────────────────────
    float walkSpeed_ = 0.12f;  // 1フレームあたりの移動量 (メートル)
    float gravity_   = -0.015f;// 1フレームあたりの重力加速度
    float jumpSpeed_ = 0.3f;   // ジャンプ時の初速度
    bool  isGrounded_ = false; // 地面 or ブロックに接地しているか

    // ── ドア・はしご状態 ──────────────────────────────────
    Vector3 nearDoorWorldPos_ = { 0,0,0 }; // 近くのドアのワールド座標 (UI 追従用)
    bool    isNearDoor_       = false;     // ドアの近くにいるか

    // PスイッチUI
    Vector3 nearPSwitchWorldPos_ = { 0,0,0 };
    bool isNearPSwitch_ = false;

    // 鍵UI
    Vector3 nearKeyWorldPos_ = { 0,0,0 };
    bool isNearKey_ = false;

    // 鍵ブロックUI
    Vector3 nearKeyBlockWorldPos_ = { 0,0,0 };
    bool isNearKeyBlock_ = false;

    bool    isOnLadder_       = false;     // はしごに掴まっているか
    Vector3 ladderWorldPos_   = { 0,0,0 }; // はしごのワールド座標 (UI 追従用)

    // ── アイテム所持 ──────────────────────────────────────
    bool hasKey_          = false; // 鍵を持っているか (鍵→扉を開く)
    bool hasJustWarped_   = false; // ドアワープ直後フラグ (連続ワープ防止)

    const Input* input_ = nullptr; // 入力ポインタ (Update() で毎フレーム受け取る)
};

