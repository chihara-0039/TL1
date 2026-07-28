#include <algorithm>
#include <cmath>
#include "GameRuntime.h"
#include "EffectPresetStore.h"
#include "../Environment/WeatherPresetManager.h"

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

void GameRuntime::Update() {


    HandleModeChange();


    BeginFrameImGui();


    input->Update();
    UpdateBlenderLevelFileWatch();
    if (input->TriggerKey(DIK_F3)) {
        debugFlags_.showCollisionBoxes = !debugFlags_.showCollisionBoxes;
    }
    UpdateSceneTransition();


    bool isGuiCaptured = IsGuiCapturingMouse();


    Vector3 lightDir = UpdateLightCameraForFrame();
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // 雷・ヒット発光の寿命は表示モードに関係なく毎フレーム減衰させる。
    // 以前はShowcase内だけで減衰していたため、通常ゲームでは次の雷まで残っていた。
    effectShowcaseController_.TickLight(1.0f / 60.0f);

    UpdateHitEffectShortcut();


    UpdateSharedCameraControls(isGuiCaptured);

    camera->Update();

    UpdateBackgroundObjects();


    UpdateParticleDebugVisibility();

    UpdateCurrentMode(lightVP, isGuiCaptured);

    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();


    UpdatePlayerCameraAndTransform(view, proj, lightVP);


    if (IsWindowInactive()) {
        return;
    }


    UpdateDebugAndEffectObjects(view, proj, lightVP);


    UpdateStagePresentation(view, proj, lightVP);
    UpdateRuntimeLevelObjects(view, proj, lightVP);


    UpdateWeatherParticles(view, proj);


    ApplySceneLighting(lightDir);


    UpdateClearColorForFrame();


    UpdateGameplayUserInterface();
}

void GameRuntime::HandleModeChange() {
    if (currentMode_ == prevMode_) {
        return;
    }

    // PostEffect表示で無効化したパーティクル表示を、天候を使うシーンへ持ち越さない。
    if (currentMode_ == AppMode::StageEditor ||
        currentMode_ == AppMode::GamePlay ||
        currentMode_ == AppMode::GamePlay_BlockPlace) {
        debugFlags_.showParticles = true;
    }
    if (currentMode_ == AppMode::StageSelect && particleManager) {
        // 継続型の嵐は寿命の長い雨・風を持つため、停止だけでなく残存粒子も破棄する。
        weatherRuntimeController_.StopStorm(*particleManager);
        particleManager->GetWeatherEmitter().active = false;
        particleManager->GetAmbientCloudEmitter().active = false;
        particleManager->ClearParticles();
    }

    bgmController_.Update(
        currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);

    const bool leftEffectPresentation =
        (prevMode_ == AppMode::EffectPreview || prevMode_ == AppMode::EffectShowcase) &&
        (currentMode_ != AppMode::EffectPreview && currentMode_ != AppMode::EffectShowcase);
    if (leftEffectPresentation && particleManager) {
        particleManager->SetStormActive(false);
    }

    if (currentMode_ == AppMode::SkinningEditor) {
        EnsureSkinningEditorInitialized();
        camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
    } else if (currentMode_ == AppMode::StageEditor) {
        // 旧左パネルを廃止して広がったシーンビューの中央へ、編集対象を再配置する。
        const Vector3 focus = player_ ? player_->GetPosition() : Vector3{ 8.0f, 1.0f, 8.0f };
        camera->ForceReset(focus, 18.0f, { 0.35f, 0.0f, 0.0f });
    } else if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        EnsureTerrainInitialized();
        camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
        effectShowcaseController_.Reset();
    } else if (currentMode_ == AppMode::PostEffectShowcase) {
        EnsurePostProcessInitialized();
        EnsureTerrainInitialized();
        camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 7.0f, { 0.35f, 0.0f, 0.0f });
    }

    prevMode_ = currentMode_;
}

void GameRuntime::BeginFrameImGui() {
    dxCommon->BeginImGui();
#ifndef NDEBUG
    UpdateImGui();
#endif
}

bool GameRuntime::IsGuiCapturingMouse() {
#ifndef NDEBUG
    return ImGui::GetIO().WantCaptureMouse;
#else
    return false;
#endif
}

Vector3 GameRuntime::UpdateLightCameraForFrame() {
    Vector3 lightDir = stageMap_.GetLightDirection();
    if (lightCamera_) {
        const Vector3 targetPos = player_ ? player_->GetPosition() : camera->GetPosition();
        lightCamera_->Update(lightDir, targetPos);
    }
    return lightDir;
}

void GameRuntime::UpdateHitEffectShortcut() {
    if (currentMode_ == AppMode::PostEffectShowcase) {
        return;
    }
    if (!input->TriggerKey(DIK_H) || !particleManager) {
        return;
    }

    Vector3 effectPos = effectPreviewPosition_;
    if (currentMode_ != AppMode::EffectPreview && currentMode_ != AppMode::EffectShowcase) {
        effectPos = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
        effectPos.y += 0.9f;
    }

    if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        if (IsCurrentEffectStorm()) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        } else {
            EmitEffectPreviewBurst();
        }
    } else {
        particleManager->EmitHitEffect(effectPos);
    }
}

void GameRuntime::UpdateSharedCameraControls(bool isGuiCaptured) {
    if (currentMode_ != AppMode::GamePlay) {
        const bool invertEditorOrbit =
            currentMode_ == AppMode::StageEditor ||
            currentMode_ == AppMode::GamePlay_BlockPlace;
        camera->UpdateBlenderStyle(
            input.get(), isGuiCaptured, winApp->GetHwnd(), invertEditorOrbit);
    }
}

void GameRuntime::UpdateBackgroundObjects() {
    if (skydomeObject_ && debugFlags_.showSkybox && !showSkyboxCubemap_) {
        skydomeObject_->SetPosition(camera->GetPosition());
        skydomeObject_->Update(Math::MakeIdentity4x4());
    }
    if (skybox_ && debugFlags_.showSkybox && showSkyboxCubemap_) {
        skybox_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skybox_->Update(camera->GetPosition());
    }
}

void GameRuntime::UpdateParticleDebugVisibility() {
    if (particleManager &&
        currentMode_ != AppMode::EffectPreview &&
        currentMode_ != AppMode::EffectShowcase &&
        currentMode_ != AppMode::PostEffectShowcase) {
        particleManager->SetDrawGPUParticleSphere(false);
    }
}

void GameRuntime::UpdateCurrentMode(const Matrix4x4& lightVP, bool isGuiCaptured) {
    if (!sceneManager_) {
        return;
    }

    const SceneType requestedScene = GetCurrentSceneType();
    if (sceneManager_->GetCurrentSceneType() != requestedScene) {
        sceneManager_->ChangeScene(requestedScene, *this);
    }

    sceneManager_->Update(*this, SceneUpdateContext{ lightVP, isGuiCaptured });

    const SceneType sceneAfterUpdate = GetCurrentSceneType();
    if (sceneManager_->GetCurrentSceneType() != sceneAfterUpdate) {
        sceneManager_->ChangeScene(sceneAfterUpdate, *this);
    }
}

void GameRuntime::UpdatePlayerCameraAndTransform(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    if (!player_) {
        return;
    }

    player_->SetCamera(view, proj);
    if (currentMode_ == AppMode::DebugView) {
        player_->Update(input.get(), stageMap_, camera->GetRotation().y, lightVP, dxCommon.get());
    } else if (currentMode_ != AppMode::GamePlay) {
        player_->UpdateTransform(lightVP);
    }
}

bool GameRuntime::IsWindowInactive() {
    return GetActiveWindow() != winApp->GetHwnd();
}

void GameRuntime::UpdateDebugAndEffectObjects(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    if (debugFlags_.show3DObjects && currentMode_ == AppMode::DebugView) {
        for (auto& obj : objectList) {
            if (obj) {
                obj->SetCamera(view, proj);
                obj->Update(lightVP);
            }
        }

        if (debugFlags_.showTerrain) {
            EnsureTerrainInitialized();
            terrainObject_->SetCamera(view, proj);
            terrainObject_->Update(lightVP);
        }
    }

    if ((currentMode_ == AppMode::EffectPreview ||
         currentMode_ == AppMode::EffectShowcase ||
         currentMode_ == AppMode::PostEffectShowcase)) {
        EnsureTerrainInitialized();
        terrainObject_->SetCamera(view, proj);
        terrainObject_->Update(lightVP);
    }
}

void GameRuntime::UpdateStagePresentation(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    // ステージエディターで外部レベルを確認している間は、グリッドステージを重ねない。
    const bool showNativeStage =
        !(currentMode_ == AppMode::StageEditor && blenderStageActive_);

    if (stageRenderer_ && showNativeStage) {
        stageRenderer_->SetIsEditorMode(currentMode_ == AppMode::StageEditor);
        stageRenderer_->SetCamera(view, proj);
        stageRenderer_->Update(stageMap_, lightVP);
    }

    if (stageRenderer_ && player_ && showNativeStage) {
        stageRenderer_->UpdateCloudTransparency(
            camera->GetPosition(),
            player_->GetPosition()
        );
    }

    if (mapCursor_ && showNativeStage &&
        (currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        mapCursor_->SetCamera(view, proj);
        mapCursor_->Update(lightVP);
    }
}

void GameRuntime::UpdateWeatherParticles(const Matrix4x4& view, const Matrix4x4& proj) {
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        sprite->Update();
    }
    if (!debugFlags_.showParticles || !particleManager || !textureManager) {
        return;
    }

    const bool usesStageWeather =
        currentMode_ == AppMode::StageEditor ||
        currentMode_ == AppMode::GamePlay ||
        currentMode_ == AppMode::GamePlay_BlockPlace;

    bool lightningFlashed = false;
    if (usesStageWeather) {
        if (WeatherPreset* preset =
            WeatherPresetManager::GetInstance().GetPresetByName(stageMap_.GetWeatherPresetName())) {
            // プリセット編集内容をゲーム内の環境光と背景色へ即時反映する。
            stageMap_.SetClearColor(preset->clearColor);
            stageMap_.SetLightIntensity(preset->lightIntensity);
            stageMap_.SetLightColor(preset->lightColor);
            stageMap_.SetLightDirection(preset->lightDirection);
        }
        const Vector3 focusPosition = player_ ? player_->GetPosition() : Vector3{0.0f, 0.0f, 0.0f};
        WeatherRuntimeController::UpdateContext context{
            *particleManager,
            *textureManager,
            stageMap_,
            view,
            proj,
            focusPosition,
            stageMap_.GetWeatherPresetName(),
            false,
            [this](const std::string& name) { return LoadStormPreset(name); }
        };
        lightningFlashed = weatherRuntimeController_.Update(context);
    } else {
        // エフェクト編集系ではステージ天候を上書きせず、発生済みエフェクトだけ更新する。
        particleManager->GetWeatherEmitter().active = false;
        particleManager->GetAmbientCloudEmitter().active = false;
        particleManager->Update(
            1.0f / 60.0f, view, proj,
            player_ ? player_->GetPosition() : Vector3{0.0f, 0.0f, 0.0f},
            &stageMap_);
        lightningFlashed = particleManager->ConsumeStormLightningFlash();
    }

    if (lightningFlashed) {
        effectShowcaseController_.NotifyImpact();
    }
}
void GameRuntime::ApplySceneLighting(const Vector3& lightDir) {
    object3dCommon->SetLightDirection(lightDir);
    object3dCommon->SetLightColor(Vector4(
        stageMap_.GetLightColor().x,
        stageMap_.GetLightColor().y,
        stageMap_.GetLightColor().z, 1.0f));
    const bool isEffectPresentation = currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase;
    object3dCommon->SetLightIntensity(isEffectPresentation ? 0.18f : stageMap_.GetLightIntensity());
    object3dCommon->SetCameraPosition(camera->GetPosition());
    object3dCommon->ClearPointLights();

    const bool isPlayerScene =
        currentMode_ == AppMode::StageEditor ||
        currentMode_ == AppMode::GamePlay ||
        currentMode_ == AppMode::GamePlay_BlockPlace;
    if (player_ && isPlayerScene) {
        // プレイヤー本体の発光と、周囲を照らすライトを常時維持する。
        player_->SetGlow(playerGlow_);
        Vector3 playerLightPosition = player_->GetPosition();
        playerLightPosition.y += 0.8f;
        object3dCommon->AddPointLight(
            playerLightPosition, playerLightIntensity_, playerLightColor_, 12.0f);
    }

    if (isEffectPresentation) {
        const bool isStorm = IsCurrentEffectStorm();
        const float remaining = effectShowcaseController_.GetLightRatio();
        const float lightEnvelope = remaining * remaining;
        const Vector4 sourceColor = isStorm && particleManager
            ? particleManager->GetStormSettings().lightningColor
            : effectPreviewHitSettings_.lightningCount > 0
            ? effectPreviewHitSettings_.lightningColor
            : effectPreviewHitSettings_.coreColor;
        const Vector4 lightColor = {
            std::clamp(sourceColor.x, 0.0f, 1.0f),
            std::clamp(sourceColor.y, 0.0f, 1.0f),
            std::clamp(sourceColor.z, 0.0f, 1.0f),
            1.0f
        };
        const float lightIntensity = isStorm && particleManager
            ? particleManager->GetStormSettings().pointLightPower * lightEnvelope
            : (1.8f + effectPreviewHitSettings_.brightness * 2.8f) * lightEnvelope;
        const Vector3 lightPosition = isStorm && particleManager
            ? particleManager->GetStormLightningPosition()
            : effectPreviewPosition_;
        object3dCommon->AddPointLight(lightPosition, lightIntensity, lightColor, 18.0f);
    } else if (weatherRuntimeController_.IsStormActive() && particleManager) {
        // 嵐の雷光はプレイヤーライトを消さず、追加ライトとして重ねる。
        const float lightningEnvelope = effectShowcaseController_.GetLightRatio();
        if (lightningEnvelope > 0.0f) {
            const auto& storm = particleManager->GetStormSettings();
            object3dCommon->AddPointLight(
                particleManager->GetStormLightningPosition(),
                storm.pointLightPower * lightningEnvelope * lightningEnvelope,
                storm.lightningColor,
                24.0f);
        }
    }
}

void GameRuntime::UpdateClearColorForFrame() {
    if (postProcessInitialized_) {
        postProcess_.SetClearColor(stageMap_.GetClearColor());
    }
    const bool stormBackdrop = IsCurrentEffectStorm();
    if (stormBackdrop) {
        dxCommon->SetClearColor(0.012f, 0.018f, 0.045f, 1.0f);
    } else {
        const Vector4& clear = stageMap_.GetClearColor();
        dxCommon->SetClearColor(clear.x, clear.y, clear.z, clear.w);
    }
}

void GameRuntime::UpdateGameplayUserInterface() {
    if (gameplayUIManager_) {
        gameplayUIManager_->Update(
            currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace,
            player_.get(), camera.get(), lightCamera_.get());
    }

    if (blockInventoryUI_) {
        bool isPlayOrPlace = (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);
        blockInventoryUI_->Update(input.get(), winApp.get(), isPlayOrPlace, &stageMap_);

        if (blockInventoryUI_->ConsumeUseRequest()) {
            RequestSceneChange(SceneType::GamePlayBlockPlace);
            Vector3 pPos = player_ ? player_->GetPosition() : Vector3{ 0,0,0 };
            mapCursor_->SetIndex({
                static_cast<int>(std::floor(pPos.x + 0.5f)),
                static_cast<int>(std::floor(pPos.y)),
                static_cast<int>(std::floor(pPos.z + 0.5f))
            }, stageMap_);
            blockPlacementController_.SetPlaceBlockType(blockInventoryUI_->GetSelectedBlockType());
            blockPlacementController_.SetPlaceCustomId(blockInventoryUI_->GetSelectedCustomId());
        }
    }
}



void GameRuntime::UpdateDebugView() {
    if (input->TriggerKey(DIK_SPACE)) {
        particleManager->Emit({ 0, 0, 0 }, 10);
    }

    stageEditorController_.HandleCursorInput(
        input.get(), stageMap_, mapCursor_.get(), lightCamera_.get(), camera.get());
}
