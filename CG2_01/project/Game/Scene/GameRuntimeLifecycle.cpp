// アプリケーション資源の生成・破棄と、外部レベルの寿命を管理する。
#include "GameRuntime.h"
#include "../Environment/WeatherPresetManager.h"
#include "ModelManager.h"
#include "externals/imgui/imgui.h"
#include <filesystem>
#include <memory>

void GameRuntime::Initialize() {
    // Storm編集用のプリセット一覧は、UIが開かれる前にControllerへ読み込ませる。
    stormEffectEditor_.Initialize();
    WeatherPresetManager::GetInstance().LoadPresets();
    LoadEffectPresetNames();

    winApp = std::make_unique<WinApp>();
    winApp->Initialize();

    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get());

    input = std::make_unique<Input>();
    input->Initialize(winApp.get());

    srvManager = std::make_unique<SrvManager>();
    srvManager->Initialize(dxCommon.get(), SrvManager::kMaxSRVCount);

    textureManager = std::make_unique<TextureManager>();
    textureManager->Initialize(dxCommon.get(), srvManager.get());

    spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->SetTextureManager(textureManager.get());
    spriteCommon->Initialize(dxCommon.get());

    object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->SetTextureManager(textureManager.get());
    object3dCommon->Initialize(dxCommon.get());

    particleManager = std::make_unique<ParticleManager>();
    particleManager->Initialize(dxCommon.get(), textureManager.get());


    stageSelect_ = std::make_unique<StageSelect>();
    stageSelect_->Initialize(object3dCommon.get(), input.get());

    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/block",  "block.obj",  textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/axis",   "axis.obj",   textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/player", "player.obj", textureManager.get())));

    blenderRuntimeLevel_.Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    const char* runtimeLevelPath = std::filesystem::exists("Resources/Levels/game_level.json")
        ? "Resources/Levels/game_level.json"
        : "Resources/Levels/sample_level.json";
    strncpy_s(
        blenderLevelPath_.data(), blenderLevelPath_.size(),
        runtimeLevelPath, _TRUNCATE);
    blenderRuntimeLevel_.Load(runtimeLevelPath);
    SyncBlenderLevelWriteTime();


    Object3d* debugFloor = CreateObject(models[0].get(), { -25.0f, 0.0f, 0.0f });
    debugFloor->SetScale({ 10.0f, 1.0f, 10.0f });
    debugFloor->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d* debugAxisA = CreateObject(models[1].get(), { -23.0f, 0.0f, 0.0f });
    debugAxisA->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d* debugAxisB = CreateObject(models[1].get(), { -27.0f, 0.0f, 0.0f });
    debugAxisB->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    uint32_t texHandle = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), texHandle);


    bgmController_.Initialize();
    
    
    
    player_ = std::make_unique<Player>();
    const std::string playerWalkGltfPath = "Resources/Models/Work/human/walk.gltf";
    if (std::filesystem::exists(playerWalkGltfPath)) {
        player_->InitializeWithSkinnedGltf(
            object3dCommon.get(),
            dxCommon.get(),
            playerWalkGltfPath,
            textureManager.get());
    } else {
        player_->Initialize(object3dCommon.get(), models[2].get());
    }
    player_->SetPosition({ 0.0f, 1.5f, 0.0f });
    player_->SetExternalCollisionBoxes(nullptr);

    
    camera = std::make_unique<Camera>();
#ifndef NDEBUG
    camera->SetAspectRatio(1280.0f / 720.0f);
#endif


    stageMap_.Initialize(100, 100, 100);

#ifdef DEVELOPMENT
    currentMode_           = AppMode::DebugView;
    debugFlags_.showSkybox = false;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage1.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage1.txt");
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    }
#else
    currentMode_           = AppMode::StageSelect;
    debugFlags_.showSkybox = true;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage01.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage01.txt");
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    }
#endif
    // 起動時はグリッドステージを表示するため、Level Editor用のSpawnで
    // PlayerStartを上書きしない。Blenderステージ開始時だけ適用する。

    if (currentMode_ == AppMode::EffectShowcase) {
        effectShowcaseController_.Reset();
        effectPreviewShowGPUParticleSphere_ = false;
    }

    skydomeModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/skydome", "skydome.obj", textureManager.get()));
    skydomeObject_ = std::make_unique<Object3d>();
    skydomeObject_->Initialize(object3dCommon.get());
    skydomeObject_->SetModel(skydomeModel_.get());
    skydomeObject_->SetEnableLighting(false);
    skydomeObject_->SetScale({ 90.0f, 90.0f, 90.0f });

    skyboxTextureHandle_ = textureManager->LoadTexture("Resources/dds/rostock_laage_airport_4k.dds");
    object3dCommon->SetEnvironmentTextureHandle(skyboxTextureHandle_);
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(object3dCommon.get(), skyboxTextureHandle_);
    skybox_->SetScale({ 50.0f, 50.0f, 50.0f });

    stageRenderer_ = std::make_unique<StageRenderer>();
    stageRenderer_->Initialize(object3dCommon.get());
    stageRenderer_->SetBlockScale({ 1.0f, 1.0f, 1.0f });
    stageRenderer_->BuildFromStageMap(stageMap_);

    mapCursor_ = std::make_unique<MapCursor>();
    mapCursor_->Initialize(object3dCommon.get());
    mapCursor_->SetIndex({ 0, 0, 0 }, stageMap_);
    mapCursor_->SetScale({ 0.9f, 0.9f, 0.9f });

    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon.get(), textureManager.get());

    lightCamera_ = std::make_unique<LightCamera>();
    lightCamera_->Initialize();

    bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);


    gameplayUIManager_ = std::make_unique<GameplayUIManager>();
    gameplayUIManager_->Initialize(dxCommon.get(), textureManager.get(), spriteCommon.get(), object3dCommon.get());

    blockInventory_.Initialize(0);

    blockInventoryUI_ = std::make_unique<BlockInventoryUI>();
    blockInventoryUI_->Initialize(dxCommon.get(), spriteCommon.get(), textureManager.get(), &blockInventory_);


    tutorialSprite_ = std::make_unique<Sprite>();
    tutorialSprite_->Initialize(spriteCommon.get(),
        textureManager->LoadTexture("Resources/UI/tutorial/tutorial.png"));
    tutorialSprite_->SetPosition({ 20, 20 });
    tutorialSprite_->SetSize({ 554, 128 });


    placementTutorialSprite_ = std::make_unique<Sprite>();
    placementTutorialSprite_->Initialize(spriteCommon.get(),
        textureManager->LoadTexture("Resources/UI/tutorial/placement_tutorial.png"));
    placementTutorialSprite_->SetPosition({ 20, 20 });
    placementTutorialSprite_->SetSize({ 682, 185 });

    // ゴール到達後にステージ選択へ戻る操作を案内する。
    clearGuideTexture_ = textureManager->LoadTexture("Resources/UI/clear_guide.png");
    clearGuideSprite_ = std::make_unique<Sprite>();
    clearGuideSprite_->Initialize(spriteCommon.get(), clearGuideTexture_);
    clearGuideSprite_->SetPosition({ 288.0f, 620.0f });
    clearGuideSprite_->SetSize({ 704.0f, 64.0f });


    gameplayCameraController_.Initialize();
    stageEditorController_.Initialize();

    sceneManager_ = SceneManager::GetInstance();
    sceneManager_->Initialize(&sceneFactory_, GetCurrentSceneType(), *this);
}



Object3d* GameRuntime::CreateObject(Model* model, Vector3 initialPosition) {
    auto createdObject = std::make_unique<Object3d>();
    createdObject->Initialize(object3dCommon.get());
    createdObject->SetModel(model);
    createdObject->SetPosition(initialPosition);
    createdObject->SetRotation({ 1.57f, 0.0f, 0.0f });
    Object3d* createdObjectRawPtr = createdObject.get();
    objectList.push_back(std::move(createdObject));
    return createdObjectRawPtr;
}

bool GameRuntime::ApplyRuntimePlayerSpawn() {
    if (!player_ || !blenderRuntimeLevel_.HasPlayerSpawn()) {
        return false;
    }

    const Vector3& spawn = blenderRuntimeLevel_.GetPlayerSpawn();
    player_->SetPosition(spawn);
    player_->SetRespawnPosition(spawn);
    return true;
}

bool GameRuntime::LoadBlenderStage(bool beginPlay) {
    const std::string levelPath = blenderLevelPath_.data();
    if (!blenderRuntimeLevel_.Load(levelPath)) {
        return false;
    }
    SyncBlenderLevelWriteTime();

    if (beginPlay) {
        blenderStageActive_ = true;

        stageMap_.Clear();
        backupMap_ = stageMap_;
        if (stageRenderer_) {
            stageRenderer_->BuildFromStageMap(stageMap_);
        }

        player_->SetExternalCollisionBoxes(&blenderRuntimeLevel_.GetCollisionBoxes());
        ApplyRuntimePlayerSpawn();
        RequestSceneChange(SceneType::GamePlay);
    } else if (currentMode_ == AppMode::StageEditor) {
        // 外部ツールで作成したレベルを、シーン遷移せずステージエディター内へ表示する。
        // 読み込んだBOXコライダーとスポーン地点も同時に反映し、見た目だけでなく
        // 実際のゲーム用レベルとして確認できる状態にする。
        blenderStageActive_ = true;
        player_->SetExternalCollisionBoxes(&blenderRuntimeLevel_.GetCollisionBoxes());
        ApplyRuntimePlayerSpawn();
        camera->ForceReset(player_->GetPosition(), 18.0f, { 0.35f, 0.0f, 0.0f });
    } else if (blenderStageActive_) {
        player_->SetExternalCollisionBoxes(&blenderRuntimeLevel_.GetCollisionBoxes());
    }
    return true;
}

void GameRuntime::SyncBlenderLevelWriteTime() {
    std::error_code error;
    const std::filesystem::path levelPath(blenderLevelPath_.data());
    if (!std::filesystem::exists(levelPath, error) || error) {
        blenderLevelWatchInitialized_ = false;
        return;
    }

    blenderLevelWriteTime_ = std::filesystem::last_write_time(levelPath, error);
    if (!error) {
        pendingBlenderLevelWriteTime_ = blenderLevelWriteTime_;
        blenderLevelWatchInitialized_ = true;
        blenderLevelReloadPending_ = false;
        blenderLevelChangeStableTime_ = 0.0f;
    }
}

void GameRuntime::UpdateBlenderLevelFileWatch() {
    // エディターから明示的に読み込んだ外部レベルだけを監視する。
    // 初期パスを常時監視すると、F5操作や別ステージ編集中に意図せず混在してしまう。
    if (!blenderStageActive_) {
        return;
    }

    const std::filesystem::path levelPath(blenderLevelPath_.data());
    std::error_code error;
    if (!std::filesystem::exists(levelPath, error) || error) {
        return;
    }

    const auto currentWriteTime = std::filesystem::last_write_time(levelPath, error);
    if (error) {
        return;
    }

    if (!blenderLevelWatchInitialized_) {
        blenderLevelWriteTime_ = currentWriteTime;
        pendingBlenderLevelWriteTime_ = currentWriteTime;
        blenderLevelWatchInitialized_ = true;
        return;
    }

    if (currentWriteTime != blenderLevelWriteTime_) {
        if (!blenderLevelReloadPending_ || currentWriteTime != pendingBlenderLevelWriteTime_) {
            pendingBlenderLevelWriteTime_ = currentWriteTime;
            blenderLevelChangeStableTime_ = 0.0f;
        }
        blenderLevelReloadPending_ = true;
        blenderLevelChangeStableTime_ += 1.0f / 60.0f;
    }

    // BlenderがJSONを書き終える前に開かないよう、更新時刻が安定してから確認を出す。
    if (blenderLevelReloadPending_ &&
        blenderLevelChangeStableTime_ >= 0.5f &&
        !blenderReloadDialogOpen_) {
        ImGui::OpenPopup("Blenderレベルの外部変更");
        blenderReloadDialogOpen_ = true;
    }

    bool keepDialogOpen = true;
    if (ImGui::BeginPopupModal(
        "Blenderレベルの外部変更", &keepDialogOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Blenderレベルファイルの変更を検出しました。");
        ImGui::Text("File: %s", blenderLevelPath_.data());
        ImGui::Separator();
        ImGui::TextUnformatted("変更内容をエンジンへ再読み込みしますか？");

        if (ImGui::Button("再読み込み", ImVec2(150.0f, 0.0f))) {
            const bool runtimeReloaded = LoadBlenderStage(false);
            const bool editorReloaded =
                currentMode_ != AppMode::SkinningEditor ||
                skinningEditor_.ReloadExternalLevel(blenderLevelPath_.data());
            if (runtimeReloaded && editorReloaded) {
                blenderLevelReloadPending_ = false;
            }
            blenderReloadDialogOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("今回は無視", ImVec2(150.0f, 0.0f))) {
            blenderLevelWriteTime_ = pendingBlenderLevelWriteTime_;
            blenderLevelReloadPending_ = false;
            blenderLevelChangeStableTime_ = 0.0f;
            blenderReloadDialogOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!keepDialogOpen) {
        blenderLevelWriteTime_ = pendingBlenderLevelWriteTime_;
        blenderLevelReloadPending_ = false;
        blenderLevelChangeStableTime_ = 0.0f;
        blenderReloadDialogOpen_ = false;
    }
}

bool GameRuntime::IsRuntimeLevelVisible() const {
    return blenderStageActive_ && (currentMode_ == AppMode::StageEditor ||
        currentMode_ == AppMode::GamePlay ||
        currentMode_ == AppMode::GamePlay_BlockPlace);
}

void GameRuntime::UpdateRuntimeLevelObjects(
    const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    if (!IsRuntimeLevelVisible()) {
        return;
    }
    blenderRuntimeLevel_.Update(view, proj, lightVP);
}

void GameRuntime::DrawRuntimeLevelObjects() {
    if (!IsRuntimeLevelVisible()) {
        return;
    }
    blenderRuntimeLevel_.Draw();
}

void GameRuntime::DrawRuntimeLevelShadows(const Matrix4x4& lightVP) {
    if (!IsRuntimeLevelVisible()) {
        return;
    }
    blenderRuntimeLevel_.DrawShadow(lightVP);
}

void GameRuntime::EnsureSkinningEditorInitialized() {
    if (skinningEditorInitialized_) {
        return;
    }
    skinningEditor_.Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    skinningEditorInitialized_ = true;
}

void GameRuntime::EnsureTerrainInitialized() {
    if (terrainObject_) {
        return;
    }

    terrainModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/terrain", "terrain.obj", textureManager.get()));
    terrainObject_ = std::make_unique<Object3d>();
    terrainObject_->Initialize(object3dCommon.get());
    terrainObject_->SetModel(terrainModel_.get());
    terrainObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
    terrainObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    terrainObject_->SetRotation({ 0.0f, 0.0f, 0.0f });
    terrainObject_->SetEnvironmentCoefficient(terrainEnvironmentCoefficient_);
    terrainObject_->SetShininess(0.38f);
    terrainObject_->SetMetallic(0.08f);
}

void GameRuntime::EnsurePostProcessInitialized() {
    if (postProcessInitialized_) {
        return;
    }
    postProcess_.Initialize(dxCommon.get(), stageMap_.GetClearColor());
    postProcessInitialized_ = true;
}


void GameRuntime::Finalize() {

    if (dxCommon) {
        dxCommon->WaitForGpu();
    }

    if (sceneManager_) {
        sceneManager_->Finalize(*this);
        sceneManager_ = nullptr;
    }

    bgmController_.Finalize();

    dxCommon->FinalizeImGui();


    ModelManager::Finalize();
    objectList.clear();
    models.clear();

    if (gameplayUIManager_) {
        gameplayUIManager_->Finalize();
    }
    gameplayUIManager_.reset();
    if (blockInventoryUI_) {
        blockInventoryUI_->Finalize();
    }
    blockInventoryUI_.reset();

    player_.reset();
    terrainObject_.reset();
    terrainModel_.reset();
    skydomeObject_.reset();
    skydomeModel_.reset();
    sprite.reset();
    stageRenderer_.reset();
    mapCursor_.reset();
    camera.reset();
    shadowMap_.reset();
    lightCamera_.reset();


    particleManager.reset();
    object3dCommon.reset();
    spriteCommon.reset();
    textureManager.reset();
    input.reset();
    dxCommon.reset();
    winApp.reset();
}


