#pragma once

#include <functional>
#include <string>

#include "MyMath.h"

class ParticleManager;
class StageMap;
class TextureManager;

// 天候プリセットを実際のパーティクル／嵐へ反映するランタイム担当。
// MyGame はシーン種別と依存オブジェクトを渡すだけに留める。
class WeatherRuntimeController {
public:
    struct UpdateContext {
        ParticleManager& particles;
        TextureManager& textures;
        StageMap& stage;
        const Matrix4x4& view;
        const Matrix4x4& projection;
        Vector3 focusPosition{};
        std::string presetName;
        bool suppressPresetStorm = false;
        std::function<bool(const std::string&)> loadStormPreset;
    };

    // 雷がこのフレームに発生した場合 true を返す。
    bool Update(const UpdateContext& context);
    void StopStorm(ParticleManager& particles);

    bool IsStormActive() const { return stormActive_; }
    float GetCachedStageTopSurfaceY() const { return cachedStageTopSurfaceY_; }

private:
    float FindHighestStageSurfaceY(const StageMap& stage) const;

    std::string cachedPresetName_;
    std::string cachedParticleTexturePath_;
    uint32_t cachedParticleTexture_ = 0;
    std::string cachedStormPreset_;
    bool stormActive_ = false;
    float cachedStageTopSurfaceY_ = 0.5f;
    int cloudHeightRefreshFrames_ = 0;
};
