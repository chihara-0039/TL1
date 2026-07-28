// シーンの入退場、切り替え要求、各シーン更新関数への振り分けを管理する。
#include "GameRuntime.h"

void GameRuntime::OnSceneEntered(SceneType sceneType) {
    // 画面効果はシーン固有の状態として扱い、前のシーンから持ち越さない。
    // Dissolveの内部タイマーも同時に止めるため、Controller経由で初期化する。
    EnsurePostProcessInitialized();
    postEffectShowcaseController_.Reset(postProcess_);

    switch (sceneType) {
    case SceneType::StageSelect:
        currentMode_ = AppMode::StageSelect;
        break;
    case SceneType::DebugView:
        currentMode_ = AppMode::DebugView;
        break;
    case SceneType::StageEditor:
        currentMode_ = AppMode::StageEditor;
        break;
    case SceneType::GamePlay:
        currentMode_ = AppMode::GamePlay;
        break;
    case SceneType::GamePlayBlockPlace:
        currentMode_ = AppMode::GamePlay_BlockPlace;
        break;
    case SceneType::SkinningEditor:
        currentMode_ = AppMode::SkinningEditor;
        break;
    case SceneType::EffectPreview:
        currentMode_ = AppMode::EffectPreview;
        break;
    case SceneType::EffectShowcase:
        currentMode_ = AppMode::EffectShowcase;
        break;
    case SceneType::PostEffectShowcase:
        currentMode_ = AppMode::PostEffectShowcase;
        break;
    }

    HandleModeChange();

    // 天候を使わないシーンでは、Emitterを止めるだけでなく既に生成済みの
    // 雨・雪・雲も破棄する。これにより遷移タイミングに依存した残留を防ぐ。
    const bool usesStageWeather =
        sceneType == SceneType::StageEditor ||
        sceneType == SceneType::GamePlay ||
        sceneType == SceneType::GamePlayBlockPlace;
    const bool ownsEffectParticles =
        sceneType == SceneType::EffectPreview ||
        sceneType == SceneType::EffectShowcase;
    if (!usesStageWeather && !ownsEffectParticles && particleManager) {
        weatherRuntimeController_.StopStorm(*particleManager);
        particleManager->GetWeatherEmitter().active = false;
        particleManager->GetAmbientCloudEmitter().active = false;
        particleManager->ClearParticles();
    }
}

void GameRuntime::OnSceneExited(SceneType sceneType) {
    if ((sceneType == SceneType::EffectPreview || sceneType == SceneType::EffectShowcase) && particleManager) {
        particleManager->SetStormActive(false);
    }
}

void GameRuntime::RequestSceneChange(SceneType sceneType) {
    if (sceneType == SceneType::StageSelect) {
        currentMode_ = AppMode::StageSelect;
    } else if (sceneType == SceneType::DebugView) {
        currentMode_ = AppMode::DebugView;
    } else if (sceneType == SceneType::StageEditor) {
        currentMode_ = AppMode::StageEditor;
    } else if (sceneType == SceneType::GamePlay) {
        currentMode_ = AppMode::GamePlay;
    } else if (sceneType == SceneType::GamePlayBlockPlace) {
        currentMode_ = AppMode::GamePlay_BlockPlace;
    } else if (sceneType == SceneType::SkinningEditor) {
        currentMode_ = AppMode::SkinningEditor;
    } else if (sceneType == SceneType::EffectPreview) {
        currentMode_ = AppMode::EffectPreview;
    } else if (sceneType == SceneType::EffectShowcase) {
        currentMode_ = AppMode::EffectShowcase;
    } else if (sceneType == SceneType::PostEffectShowcase) {
        currentMode_ = AppMode::PostEffectShowcase;
    }
}

SceneType GameRuntime::GetCurrentSceneType() const {
    if (currentMode_ == AppMode::StageSelect) {
        return SceneType::StageSelect;
    }
    if (currentMode_ == AppMode::DebugView) {
        return SceneType::DebugView;
    }
    if (currentMode_ == AppMode::StageEditor) {
        return SceneType::StageEditor;
    }
    if (currentMode_ == AppMode::GamePlay) {
        return SceneType::GamePlay;
    }
    if (currentMode_ == AppMode::GamePlay_BlockPlace) {
        return SceneType::GamePlayBlockPlace;
    }
    if (currentMode_ == AppMode::SkinningEditor) {
        return SceneType::SkinningEditor;
    }
    if (currentMode_ == AppMode::EffectPreview) {
        return SceneType::EffectPreview;
    }
    if (currentMode_ == AppMode::EffectShowcase) {
        return SceneType::EffectShowcase;
    }
    if (currentMode_ == AppMode::PostEffectShowcase) {
        return SceneType::PostEffectShowcase;
    }

    return SceneType::DebugView;
}

void GameRuntime::RunStageSelectScene() {
    UpdateStageSelect();
}

void GameRuntime::RunDebugViewScene() {
    UpdateDebugView();
}

void GameRuntime::RunStageEditorScene() {
    stageEditorController_.Update(
        input.get(), stageMap_, stageRenderer_.get(),
        mapCursor_.get(), lightCamera_.get(), player_.get(), camera.get());
}

void GameRuntime::RunGamePlayScene() {
    UpdateGamePlay();
}

void GameRuntime::RunGamePlayBlockPlaceScene() {
    UpdateGamePlayBlockPlace();
}

void GameRuntime::RunSkinningEditorScene(const SceneUpdateContext& context) {
    EnsureSkinningEditorInitialized();
    skinningEditor_.Update(
        dxCommon.get(), input.get(), camera.get(),
        context.lightViewProjection, context.isGuiCaptured, particleManager.get());

    if (skinningEditor_.ConsumePlayRequest()) {
        strncpy_s(blenderLevelPath_.data(), blenderLevelPath_.size(),
            skinningEditor_.GetSceneFilePath(), _TRUNCATE);
        LoadBlenderStage(true);
    }
}

void GameRuntime::RunEffectPreviewScene() {
    UpdateEffectPreview();
}

void GameRuntime::RunEffectShowcaseScene() {
    UpdateEffectShowcase();
    DrawEffectShowcaseImGui();
}

void GameRuntime::RunPostEffectShowcaseScene() {
    UpdatePostEffectShowcase();
    DrawPostEffectShowcaseImGui();
}




