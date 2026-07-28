#include "EffectPresetStore.h"

#include <filesystem>
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

namespace {
    void WriteColor(json& item, const char* key, const Vector4& color) {
        const std::string prefix = key;
        item[prefix + "_r"] = color.x;
        item[prefix + "_g"] = color.y;
        item[prefix + "_b"] = color.z;
        item[prefix + "_a"] = color.w;
    }

    void ReadColor(const json& item, const char* key, Vector4& color, const Vector4& fallback) {
        const std::string prefix = key;
        color.x = item.value(prefix + "_r", fallback.x);
        color.y = item.value(prefix + "_g", fallback.y);
        color.z = item.value(prefix + "_b", fallback.z);
        color.w = item.value(prefix + "_a", fallback.w);
    }

    json LoadPresetArray(const std::string& path) {
        json presets = json::array();
        if (!std::filesystem::exists(path)) {
            return presets;
        }

        std::ifstream input(path);
        if (!input.is_open()) {
            return presets;
        }

        try {
            input >> presets;
            if (!presets.is_array()) {
                presets = json::array();
            }
        } catch (const std::exception&) {
            presets = json::array();
        }
        return presets;
    }

    bool SavePresetArray(const std::string& path, const json& presets) {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream output(path);
        if (!output.is_open()) {
            return false;
        }
        output << presets.dump(4);
        return true;
    }

    void UpsertPreset(json& presets, const std::string& name, const json& saved) {
        for (auto& item : presets) {
            if (item.value("name", "") == name) {
                item = saved;
                return;
            }
        }
        presets.push_back(saved);
    }

    json ToJson(const EffectPresetStore::HitPreset& preset, const std::string& name) {
        const auto& settings = preset.settings;
        json item;
        item["name"] = name;
        item["showGpuSphere"] = preset.showGpuSphere;
        item["mirrorSlash"] = preset.mirrorSlash;
        item["burstCount"] = preset.burstCount;
        item["burstRadius"] = preset.burstRadius;
        item["showcase"] = preset.includeInShowcase;
        item["size"] = settings.size;
        item["brightness"] = settings.brightness;
        item["lifeScale"] = settings.lifeScale;
        item["slashAngle"] = settings.slashAngle;
        item["slashSpread"] = settings.slashSpread;
        item["slashCount"] = settings.slashCount;
        item["sparkCount"] = settings.sparkCount;
        item["sparkSpeed"] = settings.sparkSpeed;
        item["sparkLength"] = settings.sparkLength;
        item["scatterRadius"] = settings.scatterRadius;
        item["blueRatio"] = settings.blueRatio;
        item["ringPower"] = settings.ringPower;
        item["corePower"] = settings.corePower;
        item["crossPower"] = settings.crossPower;
        item["pillarPower"] = settings.pillarPower;
        item["lightningCount"] = settings.lightningCount;
        item["lightningSegments"] = settings.lightningSegments;
        item["lightningLength"] = settings.lightningLength;
        item["lightningSpread"] = settings.lightningSpread;
        item["lightningPower"] = settings.lightningPower;
        item["lightningWidth"] = settings.lightningWidth;
        item["lightningGlowWidth"] = settings.lightningGlowWidth;
        item["lightningGlowOpacity"] = settings.lightningGlowOpacity;
        item["lightningBranchCount"] = settings.lightningBranchCount;
        item["lightningBranchLength"] = settings.lightningBranchLength;
        item["lightningBranchSpread"] = settings.lightningBranchSpread;
        item["lightningBranchWidth"] = settings.lightningBranchWidth;
        item["lightningMode"] = settings.lightningMode;
        item["lightningDirection"] = settings.lightningDirection;
        item["lightningDirectionSpread"] = settings.lightningDirectionSpread;
        item["randomizePosition"] = settings.randomizePosition;
        item["randomizeDirection"] = settings.randomizeDirection;
        item["randomizeAngle"] = settings.randomizeAngle;
        item["angleRandomRange"] = settings.angleRandomRange;
        item["randomizeScale"] = settings.randomizeScale;
        item["randomizeLifetime"] = settings.randomizeLifetime;
        item["randomizeColor"] = settings.randomizeColor;
        WriteColor(item, "coolColor", settings.coolColor);
        WriteColor(item, "warmColor", settings.warmColor);
        WriteColor(item, "coreColor", settings.coreColor);
        WriteColor(item, "slashColor", settings.slashColor);
        WriteColor(item, "sparkColor", settings.sparkColor);
        WriteColor(item, "sparkSecondaryColor", settings.sparkSecondaryColor);
        WriteColor(item, "ringColor", settings.ringColor);
        WriteColor(item, "crossColor", settings.crossColor);
        WriteColor(item, "pillarColor", settings.pillarColor);
        WriteColor(item, "lightningColor", settings.lightningColor);
        WriteColor(item, "lightningGlowColor", settings.lightningGlowColor);
        return item;
    }
}

EffectPresetStore::PresetNames EffectPresetStore::LoadHitPresetNames(const std::string& path) const {
    PresetNames result;
    if (!std::filesystem::exists(path)) {
        result.status = "Preset: no saved file";
        return result;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        result.status = "Preset: failed to open list";
        return result;
    }

    try {
        json presets;
        file >> presets;
        if (!presets.is_array()) {
            result.status = "Preset: invalid json";
            return result;
        }

        for (const auto& item : presets) {
            const std::string name = item.value("name", "");
            if (!name.empty()) {
                result.all.push_back(name);
                if (item.value("showcase", true)) {
                    result.showcase.push_back(name);
                }
            }
        }
        result.status = result.all.empty() ? "Preset: empty" : "Preset: list loaded";
    } catch (const std::exception&) {
        result.status = "Preset: parse failed";
    }
    return result;
}

EffectPresetStore::PresetNames EffectPresetStore::LoadStormPresetNames(const std::string& path, const std::string& defaultName) const {
    PresetNames result;
    if (std::filesystem::exists(path)) {
        std::ifstream file(path);
        try {
            json presets;
            file >> presets;
            if (presets.is_array()) {
                for (const auto& item : presets) {
                    const std::string name = item.value("name", "");
                    if (!name.empty()) {
                        result.all.push_back(name);
                        if (item.value("showcase", true)) {
                            result.showcase.push_back(name);
                        }
                    }
                }
            }
        } catch (const std::exception&) {
            result.status = "Storm preset: parse failed";
        }
    }

    if (result.all.empty()) {
        result.all.push_back(defaultName);
        result.showcase.push_back(defaultName);
    }
    return result;
}

bool EffectPresetStore::SaveHitPreset(const std::string& path, const std::string& name, const HitPreset& preset, std::string& status) const {
    if (name.empty()) {
        status = "Preset: name is empty";
        return false;
    }

    json presets = LoadPresetArray(path);
    UpsertPreset(presets, name, ToJson(preset, name));
    if (!SavePresetArray(path, presets)) {
        status = "Preset: save failed";
        return false;
    }

    status = "Preset saved: " + name;
    return true;
}

bool EffectPresetStore::LoadHitPreset(const std::string& path, const std::string& name, HitPreset& preset, std::string& status) const {
    if (name.empty() || !std::filesystem::exists(path)) {
        status = "Preset: not found";
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        status = "Preset: load failed";
        return false;
    }

    try {
        json presets;
        file >> presets;
        if (!presets.is_array()) {
            status = "Preset: invalid json";
            return false;
        }

        for (const auto& item : presets) {
            if (item.value("name", "") != name) {
                continue;
            }

            preset.showGpuSphere = item.value("showGpuSphere", true);
            preset.mirrorSlash = item.value("mirrorSlash", false);
            preset.burstCount = item.value("burstCount", 1);
            preset.burstRadius = item.value("burstRadius", 0.0f);
            preset.includeInShowcase = item.value("showcase", true);

            auto& s = preset.settings;
            s.size = item.value("size", s.size);
            s.brightness = item.value("brightness", s.brightness);
            s.lifeScale = item.value("lifeScale", s.lifeScale);
            s.slashAngle = item.value("slashAngle", s.slashAngle);
            s.slashSpread = item.value("slashSpread", s.slashSpread);
            s.slashCount = item.value("slashCount", s.slashCount);
            s.sparkCount = item.value("sparkCount", s.sparkCount);
            s.sparkSpeed = item.value("sparkSpeed", s.sparkSpeed);
            s.sparkLength = item.value("sparkLength", s.sparkLength);
            s.scatterRadius = item.value("scatterRadius", s.scatterRadius);
            s.blueRatio = item.value("blueRatio", s.blueRatio);
            s.ringPower = item.value("ringPower", s.ringPower);
            s.corePower = item.value("corePower", s.corePower);
            s.crossPower = item.value("crossPower", s.crossPower);
            s.pillarPower = item.value("pillarPower", s.pillarPower);
            s.lightningCount = item.value("lightningCount", s.lightningCount);
            s.lightningSegments = item.value("lightningSegments", s.lightningSegments);
            s.lightningLength = item.value("lightningLength", s.lightningLength);
            s.lightningSpread = item.value("lightningSpread", s.lightningSpread);
            s.lightningPower = item.value("lightningPower", s.lightningPower);
            s.lightningWidth = item.value("lightningWidth", s.lightningWidth);
            s.lightningGlowWidth = item.value("lightningGlowWidth", s.lightningGlowWidth);
            s.lightningGlowOpacity = item.value("lightningGlowOpacity", s.lightningGlowOpacity);
            s.lightningBranchCount = item.value("lightningBranchCount", s.lightningBranchCount);
            s.lightningBranchLength = item.value("lightningBranchLength", s.lightningBranchLength);
            s.lightningBranchSpread = item.value("lightningBranchSpread", s.lightningBranchSpread);
            s.lightningBranchWidth = item.value("lightningBranchWidth", s.lightningBranchWidth);
            s.lightningMode = item.value("lightningMode", s.lightningMode);
            s.lightningDirection = item.value("lightningDirection", s.lightningDirection);
            s.lightningDirectionSpread = item.value("lightningDirectionSpread", s.lightningDirectionSpread);
            s.randomizePosition = item.value("randomizePosition", s.randomizePosition);
            s.randomizeDirection = item.value("randomizeDirection", s.randomizeDirection);
            s.randomizeAngle = item.value("randomizeAngle", s.randomizeAngle);
            s.angleRandomRange = item.value("angleRandomRange", s.angleRandomRange);
            s.randomizeScale = item.value("randomizeScale", s.randomizeScale);
            s.randomizeLifetime = item.value("randomizeLifetime", s.randomizeLifetime);
            s.randomizeColor = item.value("randomizeColor", s.randomizeColor);
            ReadColor(item, "coolColor", s.coolColor, s.coolColor);
            ReadColor(item, "warmColor", s.warmColor, s.warmColor);
            ReadColor(item, "coreColor", s.coreColor, s.coolColor);
            ReadColor(item, "slashColor", s.slashColor, s.coolColor);
            ReadColor(item, "sparkColor", s.sparkColor, s.coolColor);
            ReadColor(item, "sparkSecondaryColor", s.sparkSecondaryColor, s.warmColor);
            ReadColor(item, "ringColor", s.ringColor, s.coolColor);
            ReadColor(item, "crossColor", s.crossColor, s.coolColor);
            ReadColor(item, "pillarColor", s.pillarColor, s.coolColor);
            ReadColor(item, "lightningColor", s.lightningColor, s.coolColor);
            ReadColor(item, "lightningGlowColor", s.lightningGlowColor, s.coolColor);

            status = "Preset loaded: " + name;
            return true;
        }
    } catch (const std::exception&) {
        status = "Preset: parse failed";
        return false;
    }

    status = "Preset not found: " + name;
    return false;
}

bool EffectPresetStore::SaveStormPreset(const std::string& path, const std::string& name, const StormPreset& preset, std::string& status) const {
    if (name.empty()) {
        status = "Storm preset: invalid name";
        return false;
    }

    const auto& s = preset.settings;
    json item;
    item["name"] = name;
    item["showcase"] = preset.includeInShowcase;
    item["cloudAreaX"] = s.cloudAreaX; item["cloudAreaZ"] = s.cloudAreaZ;
    item["cloudHeight"] = s.cloudHeight; item["cloudEmitRate"] = s.cloudEmitRate;
    item["cloudLife"] = s.cloudLife; item["cloudSize"] = s.cloudSize;
    item["randomizeCloudPosition"] = s.randomizeCloudPosition; item["randomizeCloudSize"] = s.randomizeCloudSize;
    item["rainAreaX"] = s.rainAreaX; item["rainAreaZ"] = s.rainAreaZ;
    item["rainEmitRate"] = s.rainEmitRate; item["rainSpeed"] = s.rainSpeed; item["rainLength"] = s.rainLength;
    item["randomizeRainPosition"] = s.randomizeRainPosition; item["randomizeRainSpeed"] = s.randomizeRainSpeed;
    item["windEmitRate"] = s.windEmitRate; item["windSpeed"] = s.windSpeed; item["windLength"] = s.windLength;
    item["lightningIntervalMin"] = s.lightningIntervalMin; item["lightningIntervalMax"] = s.lightningIntervalMax;
    item["lightningFrequency"] = s.lightningFrequency;
    item["lightningAreaX"] = s.lightningAreaX; item["lightningAreaZ"] = s.lightningAreaZ;
    item["lightningStrikeSize"] = s.lightningStrikeSize;
    item["lightningSizeRandomMin"] = s.lightningSizeRandomMin; item["lightningSizeRandomMax"] = s.lightningSizeRandomMax;
    item["lightningLengthRandomMin"] = s.lightningLengthRandomMin; item["lightningLengthRandomMax"] = s.lightningLengthRandomMax;
    item["lightningWidthRandomMin"] = s.lightningWidthRandomMin; item["lightningWidthRandomMax"] = s.lightningWidthRandomMax;
    item["lightningCoreRandomMin"] = s.lightningCoreRandomMin; item["lightningCoreRandomMax"] = s.lightningCoreRandomMax;
    item["lightningSimultaneousCount"] = s.lightningSimultaneousCount;
    item["lightningSimultaneousSpread"] = s.lightningSimultaneousSpread;
    item["lightningBurstCount"] = s.lightningBurstCount;
    item["lightningBurstInterval"] = s.lightningBurstInterval;
    item["lightningCount"] = s.lightningCount; item["lightningSegments"] = s.lightningSegments;
    item["lightningLength"] = s.lightningLength; item["lightningSpread"] = s.lightningSpread;
    item["lightningPower"] = s.lightningPower; item["lightningWidth"] = s.lightningWidth;
    item["lightningGlowWidth"] = s.lightningGlowWidth; item["lightningGlowOpacity"] = s.lightningGlowOpacity;
    item["lightningBranchCount"] = s.lightningBranchCount;
    item["lightningBranchLength"] = s.lightningBranchLength; item["lightningBranchSpread"] = s.lightningBranchSpread;
    item["lightningBranchWidth"] = s.lightningBranchWidth;
    item["pointLightPower"] = s.pointLightPower;
    item["randomizeLightningPosition"] = s.randomizeLightningPosition;
    item["randomizeLightningInterval"] = s.randomizeLightningInterval;
    item["randomizeLightningDirection"] = s.randomizeLightningDirection;
    item["randomizeLightningSize"] = s.randomizeLightningSize;
    item["randomizeLightningBurstCount"] = s.randomizeLightningBurstCount;
    item["randomizeLightningBranchCount"] = s.randomizeLightningBranchCount;
    WriteColor(item, "cloudColor", s.cloudColor);
    WriteColor(item, "rainColor", s.rainColor);
    WriteColor(item, "windColor", s.windColor);
    WriteColor(item, "lightningColor", s.lightningColor);
    WriteColor(item, "lightningGlowColor", s.lightningGlowColor);

    json presets = LoadPresetArray(path);
    UpsertPreset(presets, name, item);
    if (!SavePresetArray(path, presets)) {
        status = "Storm preset: save failed";
        return false;
    }

    status = "Storm preset saved: " + name;
    return true;
}

bool EffectPresetStore::LoadStormPreset(const std::string& path, const std::string& defaultName, const std::string& name, StormPreset& preset, std::string& status) const {
    if (!std::filesystem::exists(path)) {
        if (name == defaultName) {
            preset.settings = ParticleManager::StormEffectSettings{};
            return true;
        }
        return false;
    }

    std::ifstream file(path);
    try {
        json presets;
        file >> presets;
        for (const auto& item : presets) {
            if (item.value("name", "") != name) continue;

            auto& s = preset.settings;
            const float legacyAreaX = item.value("areaX", s.cloudAreaX);
            const float legacyAreaZ = item.value("areaZ", s.cloudAreaZ);
            s.cloudAreaX = item.value("cloudAreaX", legacyAreaX); s.cloudAreaZ = item.value("cloudAreaZ", legacyAreaZ);
            s.cloudHeight = item.value("cloudHeight", s.cloudHeight); s.cloudEmitRate = item.value("cloudEmitRate", s.cloudEmitRate);
            s.cloudLife = item.value("cloudLife", s.cloudLife); s.cloudSize = item.value("cloudSize", s.cloudSize);
            s.randomizeCloudPosition = item.value("randomizeCloudPosition", s.randomizeCloudPosition);
            s.randomizeCloudSize = item.value("randomizeCloudSize", s.randomizeCloudSize);
            s.rainAreaX = item.value("rainAreaX", legacyAreaX); s.rainAreaZ = item.value("rainAreaZ", legacyAreaZ);
            s.rainEmitRate = item.value("rainEmitRate", s.rainEmitRate); s.rainSpeed = item.value("rainSpeed", s.rainSpeed); s.rainLength = item.value("rainLength", s.rainLength);
            s.randomizeRainPosition = item.value("randomizeRainPosition", s.randomizeRainPosition);
            s.randomizeRainSpeed = item.value("randomizeRainSpeed", s.randomizeRainSpeed);
            s.windEmitRate = item.value("windEmitRate", s.windEmitRate); s.windSpeed = item.value("windSpeed", s.windSpeed); s.windLength = item.value("windLength", s.windLength);
            s.lightningIntervalMin = item.value("lightningIntervalMin", s.lightningIntervalMin); s.lightningIntervalMax = item.value("lightningIntervalMax", s.lightningIntervalMax);
            s.lightningFrequency = item.value("lightningFrequency", s.lightningFrequency);
            s.lightningAreaX = item.value("lightningAreaX", legacyAreaX * 0.5f);
            s.lightningAreaZ = item.value("lightningAreaZ", legacyAreaZ * 0.4f);
            s.lightningStrikeSize = item.value("lightningStrikeSize", s.lightningStrikeSize);
            s.lightningSizeRandomMin = item.value("lightningSizeRandomMin", s.lightningSizeRandomMin); s.lightningSizeRandomMax = item.value("lightningSizeRandomMax", s.lightningSizeRandomMax);
            s.lightningLengthRandomMin = item.value("lightningLengthRandomMin", s.lightningLengthRandomMin); s.lightningLengthRandomMax = item.value("lightningLengthRandomMax", s.lightningLengthRandomMax);
            s.lightningWidthRandomMin = item.value("lightningWidthRandomMin", s.lightningWidthRandomMin); s.lightningWidthRandomMax = item.value("lightningWidthRandomMax", s.lightningWidthRandomMax);
            s.lightningCoreRandomMin = item.value("lightningCoreRandomMin", s.lightningCoreRandomMin); s.lightningCoreRandomMax = item.value("lightningCoreRandomMax", s.lightningCoreRandomMax);
            s.lightningSimultaneousCount = item.value("lightningSimultaneousCount", s.lightningSimultaneousCount);
            s.lightningSimultaneousSpread = item.value("lightningSimultaneousSpread", s.lightningSimultaneousSpread);
            s.lightningBurstCount = item.value("lightningBurstCount", s.lightningBurstCount);
            s.lightningBurstInterval = item.value("lightningBurstInterval", s.lightningBurstInterval);
            s.lightningCount = item.value("lightningCount", s.lightningCount); s.lightningSegments = item.value("lightningSegments", s.lightningSegments);
            s.lightningLength = item.value("lightningLength", s.lightningLength); s.lightningSpread = item.value("lightningSpread", s.lightningSpread);
            s.lightningPower = item.value("lightningPower", s.lightningPower); s.lightningWidth = item.value("lightningWidth", s.lightningWidth);
            s.lightningGlowWidth = item.value("lightningGlowWidth", s.lightningGlowWidth); s.lightningGlowOpacity = item.value("lightningGlowOpacity", s.lightningGlowOpacity);
            s.lightningBranchCount = item.value("lightningBranchCount", s.lightningBranchCount);
            s.lightningBranchLength = item.value("lightningBranchLength", s.lightningBranchLength); s.lightningBranchSpread = item.value("lightningBranchSpread", s.lightningBranchSpread);
            s.lightningBranchWidth = item.value("lightningBranchWidth", s.lightningBranchWidth);
            s.pointLightPower = item.value("pointLightPower", s.pointLightPower);
            s.randomizeLightningPosition = item.value("randomizeLightningPosition", s.randomizeLightningPosition);
            s.randomizeLightningInterval = item.value("randomizeLightningInterval", s.randomizeLightningInterval);
            s.randomizeLightningDirection = item.value("randomizeLightningDirection", s.randomizeLightningDirection);
            s.randomizeLightningSize = item.value("randomizeLightningSize", s.randomizeLightningSize);
            s.randomizeLightningBurstCount = item.value("randomizeLightningBurstCount", s.randomizeLightningBurstCount);
            s.randomizeLightningBranchCount = item.value("randomizeLightningBranchCount", s.randomizeLightningBranchCount);
            ReadColor(item, "cloudColor", s.cloudColor, s.cloudColor);
            ReadColor(item, "rainColor", s.rainColor, s.rainColor);
            ReadColor(item, "windColor", s.windColor, s.windColor);
            ReadColor(item, "lightningColor", s.lightningColor, s.lightningColor);
            ReadColor(item, "lightningGlowColor", s.lightningGlowColor, s.lightningGlowColor);
            preset.includeInShowcase = item.value("showcase", true);
            status = "Storm preset loaded: " + name;
            return true;
        }
    } catch (const std::exception&) {
        status = "Storm preset: parse failed";
    }
    return false;
}
