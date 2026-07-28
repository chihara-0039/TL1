#pragma once
#include "SkinnedObject.h"
#include "Object3d.h"
#include "Model.h"
#include "Camera.h"
#include "Input.h"
#include "MyMath.h"
#include "LevelDataLoader.h"
#include "../Collision/WorldCollisionBox.h"
#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

// 前方宣言 (循環インクルードを防ぐ)
class Object3dCommon;
class DirectXCommon;
class TextureManager;
class Player;
class ParticleManager;

/// <summary>
/// スキニングエディターモードの制御クラス。
///
/// 役割：
///   - スキャン/切り替え/反映 の3操作 (ScanGltfModels / ChangePreviewModel / ApplyModelToPlayer)
///   - プレビュー用 SkinnedObject の所有と更新・描画
///   - グリッド線 (地面補助グリッド) の所有と更新・描画
///   - スケルトン描画用デバッグ立方体モデルの所有
///   - レイキャストによるジョイントのクリック選択
///   - ImGui の下パネル (タイムライン) と右パネル (設定・ボーン操作) の描画
///
/// MyGame はこのクラスへ委譲することで、スキニング関連のメンバーをすべてここに集約する。
/// </summary>
class SkinningEditorController {
public:
    SkinningEditorController()  = default;
    ~SkinningEditorController() = default;

    // ========== 初期化 ==========

    /// <summary>
    /// 初期化。SkinnedObject・デバッグキューブ・グリッド線・モデルリストを構築する。
    /// </summary>
    /// <param name="object3dCommon">Object3dCommon へのポインタ (非所有)</param>
    /// <param name="dxCommon">DirectXCommon へのポインタ (非所有)</param>
    /// <param name="textureManager">TextureManager へのポインタ (非所有)</param>
    void Initialize(
        Object3dCommon* object3dCommon,
        DirectXCommon*  dxCommon,
        TextureManager* textureManager);

    // ========== 毎フレーム処理 ==========

    /// <summary>
    /// 毎フレームの更新。
    /// レイキャストによるジョイント選択・SkinnedObject の更新・グリッド線の更新を行う。
    /// </summary>
    /// <param name="dxCommon">スキニング計算に使用する DirectXCommon</param>
    /// <param name="input">マウスクリックによるジョイント選択に使用する Input</param>
    /// <param name="camera">ビュー・プロジェクション行列の取得に使用する Camera</param>
    /// <param name="lightVP">影行列 (SkinnedObject の Update に渡す)</param>
    /// <param name="isGuiCaptured">ImGui がマウスをキャプチャしているか (クリック判定のガード用)</param>
    void Update(
        DirectXCommon*       dxCommon,
        Input*               input,
        Camera*              camera,
        const Matrix4x4&     lightVP,
        bool                 isGuiCaptured,
        ParticleManager*     particleManager = nullptr);

    /// <summary>グリッド線・スキニングメッシュ・スケルトンを描画する</summary>
    /// <param name="object3dCommon">スケルトン描画の PreDraw に使用</param>
    /// <param name="camera">スケルトン描画のビュー・プロジェクション行列</param>
    void Draw(Object3dCommon* object3dCommon, Camera* camera);

    /// <summary>シャドウマップへの描画 (影を生成するため)</summary>
    void DrawShadow(const Matrix4x4& lightVP);

    // ========== ImGui 描画 ==========

    /// <summary>
    /// 下パネル (Tools &amp; Controls) 内に描画するタイムライン UI。
    /// キーフレームのビジュアルタイムラインとジョイント別のキーフレームリストを描画する。
    /// </summary>
    void DrawImGuiTimeline();

    /// <summary>
    /// Draws a Unity-like model asset browser in the bottom Tools & Controls panel.
    /// Selecting an asset updates the 3D preview, and dragging/double-clicking can apply it to the player.
    /// </summary>
    void DrawAssetBrowserPanel(Player* player, Model* defaultObjModel);

    /// <summary>
    /// 右パネル (Skinning Editor) に描画するサイドパネル UI。
    /// モデル選択・アニメーション選択・ボーン操作・カメラプリセットを描画する。
    /// </summary>
    /// <param name="camera">カメラプリセットボタンの操作対象</param>
    /// <param name="player">「ゲームに反映」ボタン押下時に更新するプレイヤー</param>
    /// <param name="defaultObjModel">インデックス 1 (OBJ プレイヤー) のモデル</param>
    void DrawImGuiSidePanel(Camera* camera, Player* player, Model* defaultObjModel);

    // ========== ゲッター ==========

    /// <summary>プレビュー用 SkinnedObject へのポインタを返す (非所有)</summary>
    SkinnedObject* GetPreviewObject() const { return skinnedObject_.get(); }

    /// <summary>プレビュー用 SkinnedObject が有効かどうか</summary>
    bool HasPreviewObject() const { return skinnedObject_ != nullptr; }

    /// <summary>「保存してプレイ」の要求を一度だけ取得する。</summary>
    bool ConsumePlayRequest();

    /// <summary>エディタとゲームが共有するレベルJSONの保存先を返す。</summary>
    const char* GetSceneFilePath() const { return sceneFilePath_; }

    /// <summary>編集中のBOXをゲームと同じワールドAABBへ変換する。</summary>
    std::vector<WorldCollisionBox> BuildWorldCollisionBoxes() const;

    /// Blenderなど外部ツールが更新したレベルJSONを、開いている編集シーンへ再読込する。
    bool ReloadExternalLevel(const std::string& filePath);

private:
    // ========== 内部処理 ==========

    /// <summary>
    /// Resources/Models 以下の .gltf / .glb / .obj ファイルを再帰スキャンし、
    /// modelPaths_ / modelNames_ に追加する。
    /// インデックス 0 : Default Humanoid (組み込みスキニング人型)
    /// インデックス 1以降 : スキャンした OBJ ファイル
    /// その後      : スキャンした glTF ファイル
    /// </summary>
    void ScanGltfModels();

    /// <summary>
    /// 指定インデックスのモデルをプレビューにロードする。
    /// インデックス 0      : デフォルトスキニング人型 (SkinnedObject)
    /// OBJ インデックス    : Object3d で OBJ を表示 (isObjPreviewMode_ = true)
    /// glTF インデックス  : SkinnedObject で glTF を表示
    /// </summary>
    void ChangePreviewModel(int index);

    /// <summary>
    /// 現在選択中のモデルをプレイヤーに反映する。
    /// インデックス 0: デフォルトスキニング人型
    /// インデックス 1: OBJ プレイヤー
    /// インデックス 2以降: glTF モデル
    /// </summary>
    void ApplyModelToPlayer(Player* player, Model* defaultObjModel);

    /// <summary>手ジョイントの現在位置から評価課題用パーティクルを発生させる。</summary>
    void UpdateHandParticleEmitter(ParticleManager* particleManager);

    /// <summary>
    /// 右側 Inspector に、配置済みシーンオブジェクトの一覧・Transform編集・保存/読込UIを描画する。
    /// Assetsブラウザで選んだモデルを「プレイヤー差し替え」ではなく「独立した配置物」として扱うためのパネル。
    /// </summary>
    void DrawSceneObjectPanel();

    /// <summary>
    /// 現在 Assets で選択中のモデルをシーンへ配置する。
    /// 実処理は PlaceAssetInScene に委譲し、選択インデックスだけを渡す。
    /// </summary>
    bool PlaceSelectedAssetInScene();

    /// <summary>
    /// 指定した Assets インデックスの OBJ モデルを、配置済みシーンオブジェクトとして生成する。
    /// 現段階では静的OBJのみ対応。glTF/SkinnedObject は更新・寿命管理が別系統なので次段階に分ける。
    /// </summary>
    bool PlaceAssetInScene(int assetIndex);

    /// <summary>
    /// 配置済みシーンオブジェクトの assetPath / position / rotation / scale を JSON に保存する。
    /// Model や Object3d 本体は保存せず、次回 Load 時に assetPath から再生成する。
    /// </summary>
    bool SaveSceneObjects(const std::string& filePath);

    /// <summary>
    /// SaveSceneObjects で保存した JSON を読み込み、配置済みシーンオブジェクトを再構築する。
    /// 読み込み時に各 assetPath から Model と Object3d を作り直す。
    /// </summary>
    bool LoadSceneObjects(const std::string& filePath);

    /// <summary>配置済みシーンオブジェクトを全削除し、選択状態も解除する。</summary>
    void ClearSceneObjects();

    // Loads a Blender-style level JSON and converts its MESH nodes into SceneObject entries.
    bool LoadLevelDataIntoScene(const std::string& filePath);
    bool AppendLevelObjectRecursive(const LevelObjectData& objectData);

private:
    /// <summary>
    /// SkinningEditor 内に配置された静的モデル1個分のデータ。
    ///
    /// name / assetPath / transform は保存対象。
    /// model / object は実行時に描画するためのリソースで、JSON には保存しない。
    /// Object3d は Model の所有権を持たないため、Model を同じ構造体内で保持して寿命を揃える。
    /// </summary>
    struct SceneObject {
        std::string name;      ///< Inspector に表示する名前。
        std::string assetPath; ///< 再読み込み時に Model を復元するための元アセットパス。
        Transform transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
        bool disabled = false; ///< trueの場合、ゲーム側へ出さない配置物として扱う。
        LevelColliderData collider; ///< Blenderレベルエディタから読み込んだ当たり判定情報。
        LevelSpawnPointData spawnPoint; ///< Blender側で指定された出現地点情報。
        std::unique_ptr<Model> model;     ///< OBJ から生成したモデル。Object3d より長く生存させる。
        std::unique_ptr<Object3d> object; ///< 実際に描画・Transform更新を行うインスタンス。
    };

    // ========== 所有リソース ==========

    std::unique_ptr<SkinnedObject>           skinnedObject_;  ///< スキニングプレビュー用オブジェクト (glTF)
    std::unique_ptr<Model>                   debugCubeModel_; ///< スケルトン描画用のデバッグ立方体モデル
    std::vector<std::unique_ptr<Object3d>>   gridLines_;      ///< 地面補助グリッド線のオブジェクト群

    // OBJ モデルをプレビューするための Object3d と Model
    // isObjPreviewMode_ が true のとき skinnedObject_ の代わりにこちらを使う
    std::unique_ptr<Object3d> objPreviewObject_;  ///< OBJ プレビュー用の Object3d
    std::unique_ptr<Model>    objPreviewModel_;   ///< OBJ プレビュー用の Model
    std::unique_ptr<Model>    appliedObjModel_;   ///< Player に適用した OBJ Model の寿命を保持する
    bool                      isObjPreviewMode_ = false; ///< OBJ モード中かどうか

    // ========== モデルリスト ==========

    std::vector<std::string> modelPaths_; ///< ファイルパス (OBJ/glTF は実際のパス, Default は識別子)
    std::vector<std::string> modelNames_; ///< UI 表示用のモデル名
    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> assetThumbnailHandles_; ///< ImGui 用ヒープにコピー済みのサムネイルハンドル
    std::vector<bool> assetHasThumbnail_; ///< サムネイルが実在するかどうか
    std::unordered_map<uint32_t, D3D12_GPU_DESCRIPTOR_HANDLE> assetThumbnailCache_;
    // OBJ と glTF の境界インデックスを記録しておく (ChangePreviewModel での分岐に使用)
    int objStartIndex_  = 1;  ///< OBJ ファイルが始まるインデックス (通常 1)
    int gltfStartIndex_ = 0;  ///< glTF ファイルが始まるインデックス (スキャン後に確定)
    int selectedModelIndex_   = 0; ///< 現在プレビュー中のモデルインデックス
    int activeGameModelIndex_ = 0; ///< ゲームに反映済みのモデルインデックス
    char motionName_[128] = "CustomMotion";
    char motionPath_[256] = "Resources/Animations/custom_motion.json";
    std::string motionStatus_;
    bool hasCustomMotionFile_ = false;
    int blendTargetMotionIndex_ = 0;  ///< Animation blend target selected by ImGui.
    float blendDuration_ = 0.35f;     ///< Seconds used to interpolate into the target motion.
    bool assetBrowserGridView_ = true; ///< True when the asset browser uses Unity-like tiles.
    int assetTileSize_ = 82;           ///< Pixel size used by model asset tiles.
    std::string assetBrowserStatus_;   ///< Short feedback text shown after browser actions.
    // 手ジョイント連動パーティクルの状態。
    bool emitHandParticles_ = false;
    float handParticleTimer_ = 0.0f;
    int handParticleJointIndex_ = -1;

    // Assets から配置した静的OBJの一覧。SkinningEditor中だけで編集・保存する簡易シーンデータ。
    std::vector<SceneObject> sceneObjects_;
    int selectedSceneObjectIndex_ = -1; ///< Scene Objects リストで選択中の配置物。-1 は未選択。
    char sceneFilePath_[256] = "Resources/Levels/game_level.json"; ///< C++エディタとゲームが共有するJSON。
    bool playRequest_ = false; ///< 保存成功後にMyGameへプレイ開始を通知する。
    char levelDataPath_[256] = "Resources/Levels/sample_level.json"; ///< Blender-style level JSON path.
    std::string loadedLevelName_; ///< Most recently imported Blender level name.
    int levelLoadTotalNodes_ = 0; ///< Number of JSON nodes visited during the latest level import.
    int levelLoadMeshNodes_ = 0; ///< Number of MESH nodes found during the latest level import.
    int levelLoadPlacedObjects_ = 0; ///< Number of MESH nodes successfully converted into SceneObject entries.
    int levelLoadFailedObjects_ = 0; ///< Number of MESH nodes that failed to load or had invalid paths.
    int levelLoadSkippedObjects_ = 0; ///< Number of non-MESH nodes skipped during traversal.
    std::vector<std::string> levelLoadMessages_; ///< Detailed import messages shown in the Inspector report.
    std::string sceneEditorStatus_; ///< 保存/読込/配置などの結果を Inspector に表示する短いメッセージ。

    // ========== 非所有ポインタ (依存参照) ==========

    Object3dCommon* object3dCommon_  = nullptr; ///< Object3dCommon への参照 (所有しない)
    DirectXCommon*  dxCommon_        = nullptr; ///< DirectXCommon への参照 (所有しない)
    TextureManager* textureManager_  = nullptr; ///< TextureManager への参照 (所有しない)
};
