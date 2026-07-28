#pragma once
#include <vector>
#include <memory>
#include <filesystem>
#include <array>
#include <string>
#include "SceneType.h"
#include "SceneFactory.h"
#include "SceneManager.h"

#include "WinApp.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "Input.h"
#include "Camera.h"
#include "ShadowMap.h"
#include "LightCamera.h"

#include "Object3d.h"
#include "Model.h"
#include "Sprite.h"
#include "Skybox.h"
#include "SkinnedObject.h"

#include "Player.h"
#include "MapCursor.h"
#include "StageMap.h"
#include "StageRenderer.h"
#include"PlayerBasePosition.h"

#include "GameplayUIManager.h"
#include "BlockInventoryUI.h"

#include "StageSelect.h"

#include "GameplayCameraController.h"
#include "StageEditorController.h"
#include "SkinningEditorController.h"
#include "BlenderRuntimeLevel.h"
#include "StormEffectEditorController.h"
#include "HitEffectEditorView.h"

#include "../Block/BlockInventory.h"
#include "../Block/BubblePickupController.h"
#include "../Block/BlockPlacementController.h"

#include "GameBgmController.h"

#include "PostProcessRenderer.h"
#include "PostEffectShowcaseController.h"
#include "EffectShowcaseController.h"

#include "StageRespawnController.h"
#include "../Environment/WeatherRuntimeController.h"

/// ゲーム全体の更新順序を調整するオーケストレーター。
/// 個別機能の実装と状態はControllerへ委譲し、このクラスは接続と実行順序を担当する。
class GameRuntime {
public:
    void Initialize();
    void Update();
    void Draw();
    void Finalize();

    bool IsRunning() { return !winApp->ProcessMessage(); }

    void OnSceneEntered(SceneType sceneType);
    void OnSceneExited(SceneType sceneType);
    void RequestSceneChange(SceneType sceneType);
    SceneType GetCurrentSceneType() const;

    void RunStageSelectScene();
    void RunDebugViewScene();
    void RunStageEditorScene();
    void RunGamePlayScene();
    void RunGamePlayBlockPlaceScene();
    void RunSkinningEditorScene(const SceneUpdateContext& context);
    void RunEffectPreviewScene();
    void RunEffectShowcaseScene();
    void RunPostEffectShowcaseScene();

private:
    // 実行中の画面種別。SceneTypeとゲーム内部の更新分岐を対応付ける。
    enum class AppMode {
        StageSelect,
        DebugView,
        StageEditor,
        GamePlay,
        GamePlay_BlockPlace,
        SkinningEditor,
        EffectPreview,
        EffectShowcase,
        PostEffectShowcase,
    };

    struct DebugDrawFlags {
        bool show3DObjects = true;
        bool showSkybox = true;
        bool showSprite = true;
        bool showParticles = true;
        bool showTerrain = false;
        bool showCollisionBoxes = false;
    };

    // エンジン基盤。生成と破棄はGameRuntimeLifecycle.cppへ集約している。
    std::unique_ptr<WinApp>         winApp;
    std::unique_ptr<DirectXCommon>  dxCommon;
    std::unique_ptr<Input>          input;
    std::unique_ptr<SrvManager>     srvManager;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<SpriteCommon>   spriteCommon;
    std::unique_ptr<Object3dCommon> object3dCommon;
    std::unique_ptr<ParticleManager>particleManager;

    std::vector<std::unique_ptr<Object3d>> objectList;
    std::vector<std::unique_ptr<Model>>    models;

    // Blenderから読み込んだ通常ゲーム用レベルと、その有効状態。
    BlenderRuntimeLevel blenderRuntimeLevel_;
    std::array<char, 260> blenderLevelPath_{ "Resources/Levels/game_level.json" };
    bool blenderStageActive_ = false;
    std::filesystem::file_time_type blenderLevelWriteTime_{};
    std::filesystem::file_time_type pendingBlenderLevelWriteTime_{};
    bool blenderLevelWatchInitialized_ = false;
    bool blenderLevelReloadPending_ = false;
    bool blenderReloadDialogOpen_ = false;
    float blenderLevelChangeStableTime_ = 0.0f;

    std::unique_ptr<Sprite>       sprite;
    std::unique_ptr<Camera>       camera;
    std::unique_ptr<StageRenderer>stageRenderer_;
    std::unique_ptr<MapCursor>    mapCursor_;
    std::unique_ptr<Player>       player_;

    std::unique_ptr<Model>   terrainModel_;
    std::unique_ptr<Object3d>terrainObject_;
    std::unique_ptr<Object3d>effectShowcaseGround_;
    std::unique_ptr<Model>   skydomeModel_;
    std::unique_ptr<Object3d>skydomeObject_;
    std::unique_ptr<Skybox>  skybox_;
    uint32_t skyboxTextureHandle_ = 0;
    bool     showSkyboxCubemap_ = false;

    std::unique_ptr<ShadowMap>    shadowMap_;
    std::unique_ptr<LightCamera>  lightCamera_;

    // シーン生成と現在シーンの管理。
    SceneFactory sceneFactory_;
    SceneManager* sceneManager_ = nullptr;
    std::unique_ptr<StageSelect>    stageSelect_;

    std::unique_ptr<GameplayUIManager> gameplayUIManager_;
    std::unique_ptr<BlockInventoryUI>  blockInventoryUI_;
    std::unique_ptr<Sprite>            tutorialSprite_;
    std::unique_ptr<Sprite>            placementTutorialSprite_;

    uint32_t objectiveGuideTexture_ = 0;
    uint32_t stageSelectGuideTexture_ = 0;
    uint32_t clearGuideTexture_ = 0;
    std::unique_ptr<Sprite> objectiveGuideSprite_;
    std::unique_ptr<Sprite> stageSelectGuideSprite_;
    std::unique_ptr<Sprite> clearGuideSprite_;

    // 長期的な状態を持つ機能は専用Controllerへ委譲する。
    GameplayCameraController  gameplayCameraController_;
    StageEditorController     stageEditorController_;
    SkinningEditorController  skinningEditor_;
    PostProcessRenderer       postProcess_;
    PostEffectShowcaseController postEffectShowcaseController_;
    EffectShowcaseController effectShowcaseController_;
    bool                      skinningEditorInitialized_ = false;
    bool                      postProcessInitialized_ = false;

    BlockInventory           blockInventory_;
    BubblePickupController   bubblePickupController_;
    BlockPlacementController blockPlacementController_;

    StageMap             stageMap_;
    StageMap             backupMap_;
    StageRespawnController stageRespawnController_;

    PlayerBasePosition playerBasePosition_;


    // BGM音源と再生状態はController側が所有する。
    GameBgmController bgmController_;

    // フレームをまたいで保持するランタイム状態。
    AppMode        currentMode_ = AppMode::DebugView;
    AppMode        prevMode_ = AppMode::DebugView;
    DebugDrawFlags debugFlags_;
    bool           isGoalReached_ = false;
    int            placeableBlockCount_ = 0;
    float          totalTime_ = 0.0f;

    bool  useFirstPersonCamera_ = false;
    float fpsCameraYaw_ = 0.0f;
    float fpsCameraPitch_ = 0.0f;
    float fpsCameraFov_ = 0.9f;
    float placeRotationY_ = 0.0f;
    float playerGlow_ = 1.0f;
    float playerLightIntensity_ = 4.0f;
    Vector4 playerLightColor_ = { 0.68f, 0.84f, 1.0f, 1.0f };
    float debugObjectEnvironmentCoefficient_ = 0.25f;
    float terrainEnvironmentCoefficient_ = 0.0f;
    float playerEnvironmentCoefficient_ = 0.0f;
    Vector3 effectPreviewPosition_ = { 0.0f, 1.0f, 0.0f };
    float effectPreviewTimer_ = 0.0f;
    float effectPreviewInterval_ = 1.0f;
    bool effectPreviewAutoPlay_ = false;
    bool effectPreviewShowGPUParticleSphere_ = true;
    bool effectPreviewMirrorSlash_ = false;
    bool effectPreviewStormMode_ = false;
    bool effectPresetIncludeInShowcase_ = true;
    bool stormPresetIncludeInShowcase_ = true;
    int effectPreviewBurstCount_ = 1;
    float effectPreviewBurstRadius_ = 0.0f;
    ParticleManager::HitEffectSettings effectPreviewHitSettings_{};
    std::array<char, 64> effectPresetNameBuffer_{ "CinematicFinisher" };
    std::vector<std::string> effectPresetNames_;
    int effectPresetSelectedIndex_ = -1;
    std::string effectPresetStatus_ = "Preset: not loaded";
    std::array<char, 64> stormPresetNameBuffer_{ "Tempest Storm" };
    std::vector<std::string> stormPresetNames_;
    std::vector<std::string> stormShowcasePresetNames_;
    int stormPresetSelectedIndex_ = -1;
    std::string stormPresetStatus_ = "Storm preset: default";
    std::string cachedWeatherPresetName_;
    std::string cachedWeatherParticleTexturePath_;
    uint32_t cachedWeatherParticleTexture_ = 0;
    StormEffectEditorController stormEffectEditor_;
    HitEffectEditorView hitEffectEditorView_;
    WeatherRuntimeController weatherRuntimeController_;


    // 以下は責務別cppから呼ばれる内部処理。公開APIには露出させない。
    void UpdateImGui();
    void UpdateDebugView();
    void UpdateEffectPreview();
    void UpdateEffectShowcase();
    void UpdatePostEffectShowcase();
    void UpdateGameplayPostEffects();
    void EmitEffectPreviewBurst();
    void UpdateGamePlay();
    void UpdateGamePlayBlockPlace();
    void UpdateTitle();
    void UpdateStageSelect();
    void UpdateSceneTransition();
    void HandleModeChange();
    void EnsureSkinningEditorInitialized();
    void EnsureTerrainInitialized();
    void EnsurePostProcessInitialized();
    void BeginFrameImGui();
    bool IsGuiCapturingMouse();
    Vector3 UpdateLightCameraForFrame();
    void UpdateHitEffectShortcut();
    void UpdateSharedCameraControls(bool isGuiCaptured);
    void UpdateBackgroundObjects();
    void UpdateParticleDebugVisibility();
    void UpdateCurrentMode(const Matrix4x4& lightVP, bool isGuiCaptured);
    void UpdatePlayerCameraAndTransform(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP);
    bool IsWindowInactive();
    void UpdateDebugAndEffectObjects(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP);
    void UpdateStagePresentation(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP);
    bool ApplyRuntimePlayerSpawn();
    bool LoadBlenderStage(bool beginPlay);
    void UpdateBlenderLevelFileWatch();
    void SyncBlenderLevelWriteTime();
    void UpdateRuntimeLevelObjects(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP);
    void DrawRuntimeLevelObjects();
    void DrawRuntimeLevelShadows(const Matrix4x4& lightVP);
    bool IsRuntimeLevelVisible() const;
    void UpdateWeatherParticles(const Matrix4x4& view, const Matrix4x4& proj);
    void ApplySceneLighting(const Vector3& lightDir);
    void UpdateClearColorForFrame();
    void UpdateGameplayUserInterface();
    void RenderScene();
    void DrawCollisionDebugBoxes();
    void DrawSkyboxForFrame();
    bool IsPlayerHiddenByWall() const;

    void LoadEffectPresetNames();
    bool SaveEffectPreset(const std::string& name);
    bool LoadEffectPreset(const std::string& name);
    void DrawEffectPreviewEditorImGui();
    void DrawStormEffectEditorImGui();
    void DrawEffectShowcaseImGui();
    void DrawPostEffectShowcaseImGui();
    void LoadStormPresetNames();
    bool SaveStormPreset(const std::string& name);
    bool LoadStormPreset(const std::string& name);
    bool IsCurrentEffectStorm() const;

    Object3d* CreateObject(Model* model, Vector3 initialPosition);


};
