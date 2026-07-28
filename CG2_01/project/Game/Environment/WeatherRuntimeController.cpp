#include "WeatherRuntimeController.h"

#include <algorithm>

#include "WeatherPresetManager.h"
#include "ParticleManager.h"
#include "StageMap.h"
#include "TextureManager.h"

bool WeatherRuntimeController::Update(const UpdateContext& context) {
    WeatherPreset* preset = WeatherPresetManager::GetInstance().GetPresetByName(context.presetName);
    if (preset) {
        auto& emitter = context.particles.GetWeatherEmitter();
        auto& cloudEmitter = context.particles.GetAmbientCloudEmitter();
        const bool usesStormPreset = !preset->stormPreset.empty() && !context.suppressPresetStorm;

        if (--cloudHeightRefreshFrames_ <= 0) {
            cachedStageTopSurfaceY_ = FindHighestStageSurfaceY(context.stage);
            cloudHeightRefreshFrames_ = 30;
        }

        const float minimumCloudHeight = cachedStageTopSurfaceY_ + preset->cloudAltitudeOffset;
        cloudEmitter.active = preset->cloudEnabled && !usesStormPreset && !context.suppressPresetStorm;
        cloudEmitter.center = { context.focusPosition.x, 0.0f, context.focusPosition.z };
        cloudEmitter.minimumHeight = minimumCloudHeight;
        cloudEmitter.areaX = (std::max)(18.0f, preset->emitSize.x * 0.5f);
        cloudEmitter.areaZ = (std::max)(18.0f, preset->emitSize.z * 0.5f);
        cloudEmitter.emitRate = preset->cloudDensity;
        cloudEmitter.size = preset->cloudSize;
        cloudEmitter.color = preset->cloudColor;

        emitter.active = preset->particleEnabled && !usesStormPreset;
        if (usesStormPreset) {
            if (cachedStormPreset_ != preset->stormPreset) {
                cachedStormPreset_ = preset->stormPreset;
                if (context.loadStormPreset) {
                    context.loadStormPreset(preset->stormPreset);
                }
                context.particles.SetStormActive(true, { 0.0f, 0.0f, 0.0f });
            }
            context.particles.SetStormCenter({ context.focusPosition.x, 0.0f, context.focusPosition.z });
            context.particles.SetStormMinimumCloudHeight(minimumCloudHeight);
            stormActive_ = true;
        } else if (stormActive_ && !context.suppressPresetStorm) {
            StopStorm(context.particles);
        }

        if (emitter.active) {
            if (cachedPresetName_ != context.presetName || cachedParticleTexturePath_ != preset->particleTexture) {
                cachedPresetName_ = context.presetName;
                cachedParticleTexturePath_ = preset->particleTexture;
                cachedParticleTexture_ = context.textures.LoadTexture(preset->particleTexture);
                if (cachedParticleTexture_ != 0) {
                    context.particles.SetTexture(cachedParticleTexture_);
                }
            }
            emitter.emitRate = preset->emitRate;
            emitter.size = preset->emitSize;
            emitter.velocity = preset->velocity;
            emitter.velocityRandom = preset->velocityRandom;
            emitter.particleSize = preset->particleSize;
            emitter.particleLife = preset->particleLife;
            emitter.color = preset->particleColor;
            emitter.impactEffect = preset->impactEffect == "Rain"
                ? ParticleManager::WeatherImpactEffect::Rain
                : preset->impactEffect == "Snow"
                ? ParticleManager::WeatherImpactEffect::Snow
                : ParticleManager::WeatherImpactEffect::None;
            emitter.center = { 0.0f, 15.0f, 0.0f };
        }
    }

    context.particles.Update(
        1.0f / 60.0f,
        context.view,
        context.projection,
        context.focusPosition,
        &context.stage);
    return context.particles.ConsumeStormLightningFlash();
}

void WeatherRuntimeController::StopStorm(ParticleManager& particles) {
    particles.SetStormActive(false);
    cachedStormPreset_.clear();
    stormActive_ = false;
}

float WeatherRuntimeController::FindHighestStageSurfaceY(const StageMap& stage) const {
    float highestSurface = 0.5f;
    for (int y = stage.GetHeight() - 1; y >= 0; --y) {
        for (int z = 0; z < stage.GetDepth(); ++z) {
            for (int x = 0; x < stage.GetWidth(); ++x) {
                const MapCell* cell = stage.GetCell(x, y, z);
                if (cell && cell->type != BlockType::None) {
                    return static_cast<float>(y) + 0.5f;
                }
            }
        }
    }
    return highestSurface;
}
