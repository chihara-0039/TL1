#pragma once

#include <functional>
#include <string>
#include <vector>

#include "MyMath.h"

class Input;
class ParticleManager;

/// パーティクル／ストームのショーケース再生順序とUI状態を所有する。
class EffectShowcaseController {
public:
    using LoadPreset = std::function<void(const std::string&)>;
    using SimpleAction = std::function<void()>;

    /// 表示対象と、そのうちストームに該当する名前を更新する。
    void SetPresets(std::vector<std::string> presets, std::vector<std::string> stormPresets);
    void Reset();

    /// 入力と自動再生を更新する。ステージ選択へ戻る場合はtrueを返す。
    bool Update(Input& input, ParticleManager* particles, const Vector3& position,
        const LoadPreset& loadStorm, const LoadPreset& loadHit,
        const SimpleAction& emitHit, const SimpleAction& refreshPresets);

    /// ヒット発生時の一時ライトを開始し、毎フレーム減衰させる。
    void NotifyImpact();
    void TickLight(float deltaTime);
    float GetLightRatio() const;

    bool IsCurrentStorm() const;
    const std::vector<std::string>& GetPresetNames() const { return presetNames_; }
    int GetSelectedIndex() const { return selectedIndex_; }
    bool IsAutoPlayEnabled() const { return autoPlay_; }

    void DrawImGui() const;

private:
    bool IsStormIndex(int index) const;
    void SelectPreset(int index, bool play, ParticleManager* particles, const Vector3& position,
        const LoadPreset& loadStorm, const LoadPreset& loadHit, const SimpleAction& emitHit);

    std::vector<std::string> presetNames_;
    std::vector<std::string> stormPresetNames_;
    int selectedIndex_ = 0;
    bool autoPlay_ = true;
    bool firstPlay_ = true;
    float timer_ = 0.0f;
    float interval_ = 2.5f;
    float lightTimer_ = 0.0f;
    static constexpr float kLightDuration = 0.7f;
};
