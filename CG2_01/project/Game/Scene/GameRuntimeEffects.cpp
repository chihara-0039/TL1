// プリセット保存、エフェクト再生、評価課題用ショーケースを管理する。
#include "GameRuntime.h"
#include "EffectPresetStore.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <random>
#include "externals/imgui/imgui.h"

namespace {
constexpr const char* kEffectPresetPath = "Resources/presets/effect_presets.json";
constexpr const char* kStormPresetPath = "Resources/presets/storm_effect_presets.json";
constexpr const char* kStormShowcaseName = "Tempest Storm";

// ImGuiの固定長入力欄へ、終端を保証してプリセット名を反映する。
void CopyPresetName(std::array<char,64>& buffer,const std::string& name) {
    buffer.fill('\0');
    strncpy_s(buffer.data(),buffer.size(),name.c_str(),_TRUNCATE);
}
} // namespace

void GameRuntime::LoadStormPresetNames() {
    const EffectPresetStore store;
    const auto result = store.LoadStormPresetNames(kStormPresetPath, kStormShowcaseName);
    stormPresetNames_ = result.all;
    stormShowcasePresetNames_ = result.showcase;
    stormPresetSelectedIndex_ = -1;
    if (!result.status.empty()) {
        stormPresetStatus_ = result.status;
    }
}

bool GameRuntime::SaveStormPreset(const std::string& name) {
    if (name.empty() || !particleManager) {
        stormPresetStatus_ = "Storm preset: invalid name";
        return false;
    }

    EffectPresetStore::StormPreset preset;
    preset.settings = particleManager->GetStormSettings();
    preset.includeInShowcase = stormPresetIncludeInShowcase_;

    const EffectPresetStore store;
    if (!store.SaveStormPreset(kStormPresetPath, name, preset, stormPresetStatus_)) {
        return false;
    }

    LoadEffectPresetNames();
    for (int i = 0; i < static_cast<int>(stormPresetNames_.size()); ++i) {
        if (stormPresetNames_[i] == name) {
            stormPresetSelectedIndex_ = i;
            break;
        }
    }
    CopyPresetName(stormPresetNameBuffer_, name);
    return true;
}

bool GameRuntime::LoadStormPreset(const std::string& name) {
    if (!particleManager) { 
        return false;
    }

    EffectPresetStore::StormPreset preset;
    preset.settings = particleManager->GetStormSettings();
    preset.includeInShowcase = stormPresetIncludeInShowcase_;

    const EffectPresetStore store;
    if (!store.LoadStormPreset(kStormPresetPath, kStormShowcaseName, name, preset, stormPresetStatus_)) {
        return false;
    }

    particleManager->GetStormSettings() = preset.settings;
    stormPresetIncludeInShowcase_ = preset.includeInShowcase;
    CopyPresetName(stormPresetNameBuffer_, name);
    return true;
}

void GameRuntime::LoadEffectPresetNames() {
    LoadStormPresetNames();

    const EffectPresetStore store;
    const auto result = store.LoadHitPresetNames(kEffectPresetPath);
    effectPresetNames_ = result.all;
    std::vector<std::string> showcasePresets = stormShowcasePresetNames_;
    showcasePresets.insert(showcasePresets.end(), result.showcase.begin(), result.showcase.end());
    effectShowcaseController_.SetPresets(std::move(showcasePresets), stormPresetNames_);
    effectPresetSelectedIndex_ = -1;
    effectPresetStatus_ = result.status;
}

bool GameRuntime::SaveEffectPreset(const std::string& name) {
    EffectPresetStore::HitPreset preset;
    preset.settings = effectPreviewHitSettings_;
    preset.showGpuSphere = effectPreviewShowGPUParticleSphere_;
    preset.mirrorSlash = effectPreviewMirrorSlash_;
    preset.burstCount = effectPreviewBurstCount_;
    preset.burstRadius = effectPreviewBurstRadius_;
    preset.includeInShowcase = effectPresetIncludeInShowcase_;

    const EffectPresetStore store;
    if (!store.SaveHitPreset(kEffectPresetPath, name, preset, effectPresetStatus_)) {
        return false;
    }

    LoadEffectPresetNames();
    for (int i = 0; i < static_cast<int>(effectPresetNames_.size()); ++i) {
        if (effectPresetNames_[i] == name) {
            effectPresetSelectedIndex_ = i;
            break;
        }
    }
    return true;
}

bool GameRuntime::LoadEffectPreset(const std::string& name) {
    EffectPresetStore::HitPreset preset;
    preset.settings = effectPreviewHitSettings_;
    preset.showGpuSphere = effectPreviewShowGPUParticleSphere_;
    preset.mirrorSlash = effectPreviewMirrorSlash_;
    preset.burstCount = effectPreviewBurstCount_;
    preset.burstRadius = effectPreviewBurstRadius_;
    preset.includeInShowcase = effectPresetIncludeInShowcase_;

    const EffectPresetStore store;
    if (!store.LoadHitPreset(kEffectPresetPath, name, preset, effectPresetStatus_)) {
        return false;
    }

    effectPreviewHitSettings_ = preset.settings;
    effectPreviewShowGPUParticleSphere_ = preset.showGpuSphere;
    effectPreviewMirrorSlash_ = preset.mirrorSlash;
    effectPreviewBurstCount_ = preset.burstCount;
    effectPreviewBurstRadius_ = preset.burstRadius;
    effectPresetIncludeInShowcase_ = preset.includeInShowcase;

    if (currentMode_ == AppMode::EffectPreview) {
        effectPreviewStormMode_ = false;
        if (particleManager) {
            particleManager->SetStormActive(false);
        }
    }

    CopyPresetName(effectPresetNameBuffer_, name);
    return true;
}

bool GameRuntime::IsCurrentEffectStorm() const {
    if (currentMode_ == AppMode::EffectPreview) {
        return effectPreviewStormMode_;
    }
    return currentMode_ == AppMode::EffectShowcase && effectShowcaseController_.IsCurrentStorm();
}

void GameRuntime::EmitEffectPreviewBurst() {
    if (!particleManager) {
        return;
    }

    if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        effectShowcaseController_.NotifyImpact();
    }

    ParticleManager::HitEffectSettings settings = effectPreviewHitSettings_;
    if (effectPreviewMirrorSlash_) {
        settings.slashAngle = -settings.slashAngle;
    }

    const int burstCount = effectPreviewBurstCount_ < 1 ? 1 : effectPreviewBurstCount_;
    constexpr float kPi = 3.14159265f;
    static std::mt19937 randomEngine(std::random_device{}());
    for (int i = 0; i < burstCount; ++i) {
        Vector3 burstPosition = effectPreviewPosition_;
        if (burstCount > 1 && effectPreviewBurstRadius_ > 0.0f) {
            const float angle = (static_cast<float>(i) / static_cast<float>(burstCount)) * kPi * 2.0f;
            burstPosition.x += std::cos(angle) * effectPreviewBurstRadius_;
            burstPosition.y += std::sin(angle * 1.7f) * effectPreviewBurstRadius_ * 0.35f;
            burstPosition.z += std::sin(angle) * effectPreviewBurstRadius_;
        }

        ParticleManager::HitEffectSettings burstSettings = settings;
        if (burstSettings.randomizeAngle && burstSettings.angleRandomRange > 0.0f) {
            std::uniform_real_distribution<float> angleOffset(-burstSettings.angleRandomRange, burstSettings.angleRandomRange);
            const float randomAngle = angleOffset(randomEngine);
            burstSettings.slashAngle += randomAngle;
            burstSettings.lightningDirection += randomAngle;
        }
        const float burstT = burstCount <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(burstCount - 1);
        burstSettings.size *= 1.0f - 0.12f * burstT;
        burstSettings.brightness *= 1.0f - 0.10f * burstT;
        burstSettings.slashAngle += (static_cast<float>(i) - static_cast<float>(burstCount - 1) * 0.5f) * 0.22f;
        burstSettings.slashSpread += 0.18f * burstT;
        burstSettings.sparkSpeed *= 1.0f + 0.20f * burstT;
        burstSettings.ringPower *= 1.0f + 0.18f * burstT;
        particleManager->EmitHitEffect(burstPosition, burstSettings);
    }
}

void GameRuntime::UpdateEffectPreview() {
    debugFlags_.showParticles = true;
    if (particleManager) {
        particleManager->SetDrawGPUParticleSphere(effectPreviewStormMode_ ? false : effectPreviewShowGPUParticleSphere_);
        if (effectPreviewStormMode_ && !particleManager->IsStormActive()) {
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        } else if (!effectPreviewStormMode_ && particleManager->IsStormActive()) {
            particleManager->SetStormActive(false);
        }
    }

    if (input->TriggerKey(DIK_SPACE) && particleManager) {
        if (effectPreviewStormMode_) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        } else {
            EmitEffectPreviewBurst();
        }
    }

    if (!effectPreviewStormMode_ && effectPreviewAutoPlay_ && particleManager) {
        constexpr float kDeltaTime = 1.0f / 60.0f;
        effectPreviewTimer_ += kDeltaTime;
        if (effectPreviewTimer_ >= effectPreviewInterval_) {
            effectPreviewTimer_ = 0.0f;
            EmitEffectPreviewBurst();
        }
    } else {
        effectPreviewTimer_ = 0.0f;
    }
}

void GameRuntime::UpdateEffectShowcase() {
    debugFlags_.showParticles = true;
    debugFlags_.showSkybox = false;
    if (particleManager) {
        particleManager->SetDrawGPUParticleSphere(false);
    }

    const bool returnToStageSelect = effectShowcaseController_.Update(
        *input, particleManager.get(), effectPreviewPosition_,
        [this](const std::string& name) { LoadStormPreset(name); },
        [this](const std::string& name) { LoadEffectPreset(name); },
        [this]() { EmitEffectPreviewBurst(); },
        [this]() { LoadEffectPresetNames(); });
    if (returnToStageSelect) {
        stageSelect_->Initialize(object3dCommon.get(), input.get());
        RequestSceneChange(SceneType::StageSelect);
    }
}
void GameRuntime::UpdatePostEffectShowcase() {
    EnsurePostProcessInitialized();
    EnsureTerrainInitialized();
    debugFlags_.showParticles = false;
    debugFlags_.showSkybox = false;

    if (postEffectShowcaseController_.Update(*input, particleManager.get(), postProcess_)) {
        stageSelect_->Initialize(object3dCommon.get(), input.get());
        RequestSceneChange(SceneType::StageSelect);
    }
}
void GameRuntime::DrawEffectShowcaseImGui() {
    effectShowcaseController_.DrawImGui();
}
void GameRuntime::DrawPostEffectShowcaseImGui() {
    postEffectShowcaseController_.DrawImGui(postProcess_);
}
