#pragma once
#include "SkinnedModel.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include <memory>
#include <vector>

// ==============================================================
//  SkinnedObject
//
//  スキニング (骨格アニメーション) に対応した 3D オブジェクトクラス。
//  Object3d の「静的な見た目」に対し、SkinnedObject は
//  「ボーンで動く見た目」を扱う。
//
//  ─── スキニングとは ───────────────────────────────────────
//  モデルの頂点を「ボーン (骨)」に紐づけておき、
//  ボーンを動かすと頂点もそれに追従して変形する技術。
//  キャラクターの歩き・走り・攻撃アニメーションはこれで実現する。
//
//  ─── Compute Shader スキニングについて ───────────────────
//  このエンジンは Compute Shader でスキニング済み頂点を生成する。
//  変形済み頂点を GPU バッファに残すことで、描画側は通常の頂点として扱える。
//  手順:
//    1. ボーンの行列を階層的に計算 (親→子の順)
//    2. WellForGPU にスケルトン空間行列を転送
//    3. Skinning.CS.hlsl で頂点とウェイトからスキニング済み頂点を生成
//    4. Draw() でスキニング済み頂点バッファを描画
//
//  ─── クラス構成 ───────────────────────────────────────────
//  SkinnedObject : 配置情報 + アニメーション制御 (このクラス)
//  SkinnedModel  : ジョイント定義・アニメーションデータ・スキニング計算
//  Object3d      : GPU への描画コマンド発行 (内部に持つ)
//
//  ─── 2種類の初期化 ────────────────────────────────────────
//  Initialize()         : 組み込みの二足歩行ヒューマノイドを生成
//  InitializeFromGltf() : .gltf/.glb ファイルから読み込み
// ==============================================================
class SkinnedObject {
public:
    SkinnedObject()  = default;
    ~SkinnedObject() = default;

    // -------------------------------------------------------
    //  Initialize : 組み込みヒューマノイドでスキニングオブジェクトを初期化。
    //  Blender などのモデルがなくても動作確認できるデフォルト人型を生成する。
    // -------------------------------------------------------
    void Initialize(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, TextureManager* textureManager);

    // -------------------------------------------------------
    //  InitializeFromGltf : glTF ファイルからスキニングモデルを読み込む。
    //  filePath には .gltf または .glb ファイルのパスを渡す。
    //  (例: "Resources/Models/character/walk.gltf")
    //  glTF は右手座標系なので内部で左手座標系に変換している。
    // -------------------------------------------------------
    void InitializeFromGltf(Object3dCommon* object3dCommon, DirectXCommon* dxCommon,
                            const std::string& filePath, TextureManager* textureManager);

    // -------------------------------------------------------
    //  Update : 毎フレーム呼ぶ。以下の処理を順番に行う:
    //  1. アニメーション時間を進める (playAnimation_ が true の場合)
    //  2. カスタムモーション再生 (playCustomAnimation_ が true の場合)
    //  3. SkinnedModel::Update() でボーン行列と GPU 用パレットを更新
    //  4. Draw() 直前に Compute Shader で頂点をスキニング
    //  5. Object3d::Update() で WVP 行列を定数バッファに書き込む
    // -------------------------------------------------------
    void Update(DirectXCommon* dxCommon, const Matrix4x4& lightVP);

    // -------------------------------------------------------
    //  Draw : スキニングメッシュを描画する。
    //  Object3dCommon::PreDraw() が事前に呼ばれている前提。
    // -------------------------------------------------------
    void Draw();

    // -------------------------------------------------------
    //  DrawShadow : シャドウマップへの影描画。
    //  スキニングアニメーション中でも正しい影が出る。
    // -------------------------------------------------------
    void DrawShadow(const Matrix4x4& lightViewProjection);

    // -------------------------------------------------------
    //  DrawSkeleton : ボーン構造をデバッグ可視化する。
    //  各ジョイント (関節) の位置に cubeModel を小さく描画し、
    //  選択中のジョイントは色を変えて強調表示する。
    //  スキニングエディタのビューポートで常時表示している。
    // -------------------------------------------------------
    void DrawSkeleton(Object3dCommon* object3dCommon, Model* cubeModel,
                      const Matrix4x4& view, const Matrix4x4& projection);

    // ── モデル参照 ───────────────────────────────────────

    /// <summary>内部のスキニングモデルへのポインタ (ジョイント操作・モーション操作用)</summary>
    SkinnedModel* GetModel() const { return skinnedModel_.get(); }

    /// <summary>内部の Object3d へのポインタ (描画設定の詳細変更用)</summary>
    Object3d* GetObject3d() const { return object3d_.get(); }

    // ── トランスフォーム ──────────────────────────────────

    void SetPosition(const Vector3& pos) { position_ = pos; }
    const Vector3& GetPosition() const   { return position_; }

    void SetRotation(const Vector3& rot) { rotation_ = rot; }
    const Vector3& GetRotation() const   { return rotation_; }

    void SetScale(const Vector3& scale)  { scale_ = scale; }
    const Vector3& GetScale() const      { return scale_; }

    // -------------------------------------------------------
    //  SetCamera : ビュー/プロジェクション行列を内部に保存する。
    //  Update() より前に毎フレーム呼ぶこと。
    // -------------------------------------------------------
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_       = view;
        projectionMatrix_ = projection;
    }

    // ── テストアニメーション制御 ──────────────────────────
    // 組み込みの「ゆらゆら動く」テスト用アニメーション。
    // glTF アニメーション再生中は false にして排他制御する。

    /// <summary>テストアニメーションの再生/停止</summary>
    void SetPlayAnimation(bool play) { playAnimation_ = play; }
    bool IsPlayAnimation() const     { return playAnimation_; }

    /// <summary>アニメーション再生速度の倍率 (1.0 = 等速 / 2.0 = 2倍速)</summary>
    void SetAnimationSpeed(float speed) { animationSpeed_ = speed; }
    float GetAnimationSpeed() const     { return animationSpeed_; }

    // ── スケルトン表示 ────────────────────────────────────

    /// <summary>骨格の可視化 On/Off。スキニングエディタで使用。</summary>
    void SetShowSkeleton(bool show) { showSkeleton_ = show; }
    bool IsShowSkeleton() const     { return showSkeleton_; }

    /// <summary>選択中ジョイントのローカル軸表示 On/Off。</summary>
    void SetShowJointAxes(bool show) { showJointAxes_ = show; }
    bool IsShowJointAxes() const     { return showJointAxes_; }

    // ── ジョイント選択 (スキニングエディタで使用) ─────────
    // レイキャストで選択されたジョイントのインデックスを保持する。
    // DrawSkeleton() で選択中ジョイントの色を変えて強調表示する。

    void SetSelectedJointIndex(int index) { selectedJointIndex_ = index; }
    int  GetSelectedJointIndex() const    { return selectedJointIndex_; }

    /// <summary>複数の名前候補から最初に一致したジョイントのインデックスを返す。</summary>
    int FindJointIndexByNameHints(const std::vector<std::string>& nameHints) const;

    /// <summary>指定ジョイントの現在のワールド座標を取得する。</summary>
    bool TryGetJointWorldPosition(int jointIndex, Vector3& outPosition) const;

    /// <summary>名前候補でジョイントを探し、そのワールド座標を取得する。</summary>
    bool TryGetJointWorldPosition(const std::vector<std::string>& nameHints, Vector3& outPosition) const;

    // ── カスタムモーション操作 ────────────────────────────
    // キーフレームを手動で登録・再生するカスタムモーション機能。
    // SkinnedModel に処理を委譲している。

    /// <summary>現在の各ジョイントの姿勢を時刻 time にキーフレームとして登録</summary>
    void AddKeyframe(float time)  { skinnedModel_->AddKeyframe(time); }

    /// <summary>登録された全キーフレームを削除し T ポーズに戻す</summary>
    void ClearKeyframes() { skinnedModel_->ClearKeyframes(); }

    /// <summary>モーションデータをテキストファイルに保存する</summary>
    bool SaveMotion(const std::string& filePath) { return skinnedModel_->SaveMotion(filePath); }

    /// <summary>テキストファイルからモーションデータを読み込む</summary>
    bool LoadMotion(const std::string& filePath) { return skinnedModel_->LoadMotion(filePath); }

    /// <summary>指定時刻のキーフレームを補間して各ジョイントに適用する</summary>
    void ApplyMotion(float time) { skinnedModel_->ApplyMotion(time); }

    /// <summary>2つのモーションを blendRate(0..1) で補間して適用する</summary>
    void ApplyMotionBlend(int fromMotionIndex, int toMotionIndex, float time, float blendRate) {
        skinnedModel_->ApplyMotionBlend(fromMotionIndex, toMotionIndex, time, blendRate);
    }

    /// <summary>歩行モーションのプリセットキーフレームを自動生成する</summary>
    void GenerateWalkPreset() { skinnedModel_->GenerateWalkPreset(); }

    /// <summary>走りモーションのプリセットキーフレームを自動生成する</summary>
    void GenerateRunPreset()  { skinnedModel_->GenerateRunPreset(); }

    // -------------------------------------------------------
    //  カスタムアニメーション再生制御
    //  playCustomAnimation_ が true のとき、
    //  Update() 内で currentKeyframeTime_ を毎フレーム進め
    //  ApplyMotion() でキーフレーム補間を適用する。
    // -------------------------------------------------------
    void  SetPlayCustomAnimation(bool play) { playCustomAnimation_ = play; }
    bool  IsPlayCustomAnimation() const     { return playCustomAnimation_; }

    float GetCurrentKeyframeTime() const       { return currentKeyframeTime_; }
    void  SetCurrentKeyframeTime(float time)   { currentKeyframeTime_ = time; animationTime_ = time; }

    /// <summary>現在のモーションから targetMotionIndex へ指定秒数でブレンドする</summary>
    void StartMotionBlend(int targetMotionIndex, float duration);
    bool IsMotionBlending() const { return playBlendAnimation_; }
    int GetBlendTargetMotionIndex() const { return blendTargetMotionIndex_; }
    float GetBlendRate() const { return blendRate_; }

private:
    // ── 主要コンポーネント ────────────────────────────────
    std::unique_ptr<SkinnedModel> skinnedModel_; // スキニング計算・アニメーションデータ
    std::unique_ptr<Object3d>     object3d_;     // GPU 描画コマンド発行 (頂点バッファはここが持つ)

    // ── ワールドトランスフォーム ──────────────────────────
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 scale_    = { 1.0f, 1.0f, 1.0f };

    // ── カメラ行列 (毎フレーム SetCamera() で更新) ──────────
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    // ── テストアニメーション ─────────────────────────────
    bool  playAnimation_  = false; // 再生フラグ
    float animationTime_  = 0.0f;  // 再生時間 (秒)
    float animationSpeed_ = 1.0f;  // 再生速度倍率

    // ── カスタムモーション ────────────────────────────────
    bool  playCustomAnimation_  = false; // カスタムモーション再生フラグ
    float currentKeyframeTime_  = 0.0f;  // タイムラインの現在時刻
    bool  playBlendAnimation_   = false; // true: 2つのモーションを補間再生中
    int   blendFromMotionIndex_ = -1;    // 補間元モーション
    int   blendTargetMotionIndex_ = -1;  // 補間先モーション
    float blendDuration_ = 0.35f;        // 補間にかける秒数
    float blendElapsed_  = 0.0f;         // 補間開始からの経過秒
    float blendRate_     = 0.0f;         // 現在の補間率 (0..1)

    // ── スケルトン可視化 ──────────────────────────────────
    bool showSkeleton_      = true;  // true: ボーンを描画する
    bool showJointAxes_     = true;  // true: 選択中ジョイントのローカル軸を描画する
    int  selectedJointIndex_ = -1;   // 選択中ジョイントのインデックス (-1: 未選択)

    // DrawSkeleton() で使うジョイント・ボーンのビジュアル用 Object3d 群。
    // ジョイントごとに立方体 (jointVisuals_) とつなぎ棒 (boneVisuals_) を持つ。
    std::vector<std::unique_ptr<Object3d>> jointVisuals_; // 各関節の位置を示すキューブ
    std::vector<std::unique_ptr<Object3d>> boneVisuals_;  // 親子関節をつなぐ棒
    std::vector<std::unique_ptr<Object3d>> axisVisuals_;  // 選択中関節のローカル軸
};


