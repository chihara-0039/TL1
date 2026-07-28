#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "ParticleManager.h"

class Camera;

// Hit Effect編集画面の表示だけを担当するView。
// ゲーム状態は所有せず、Contextで明示的に受け取る。
class HitEffectEditorView {
public:
    struct Context {
        ParticleManager* particles = nullptr;
        Camera* camera = nullptr;
        Vector3& previewPosition;
        bool& autoPlay;
        float& interval;
        bool& showGpuSphere;
        bool& mirrorSlash;
        bool& stormMode;
        bool& includeInShowcase;
        int& burstCount;
        float& burstRadius;
        ParticleManager::HitEffectSettings& settings;
        std::array<char, 64>& presetNameBuffer;
        std::vector<std::string>& presetNames;
        int& selectedPresetIndex;
        std::string& status;
        std::function<void()> drawStormEditor;
        std::function<void()> emitPreviewBurst;
        std::function<void()> reloadPresetNames;
        std::function<bool(const std::string&)> savePreset;
        std::function<bool(const std::string&)> loadPreset;
    };

    void Draw(Context& context);
};
