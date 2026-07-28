#pragma once

#include <array>
#include <string>
#include <vector>

class ParticleManager;
struct Vector3;

// Stormエフェクトのプリセット一覧、選択状態、保存／読込を所有する。
// 描画ループからファイルI/Oとエディタ状態を分離するためのController。
class StormEffectEditorController {
public:
    void Initialize();
    bool Save(const std::string& name, const ParticleManager& particles);
    bool Load(const std::string& name, ParticleManager& particles);
    void Draw(ParticleManager& particles, const Vector3& previewPosition);

    bool IsStormPreset(const std::string& name) const;
    void Select(int index);

    bool& IncludeInShowcase() { return includeInShowcase_; }
    std::array<char, 64>& NameBuffer() { return nameBuffer_; }
    const std::vector<std::string>& PresetNames() const { return presetNames_; }
    const std::vector<std::string>& ShowcasePresetNames() const { return showcasePresetNames_; }
    int SelectedIndex() const { return selectedIndex_; }
    const std::string& Status() const { return status_; }

private:
    void ReloadNames();
    void SetNameBuffer(const std::string& name);

    bool includeInShowcase_ = true;
    std::array<char, 64> nameBuffer_{ "Tempest Storm" };
    std::vector<std::string> presetNames_;
    std::vector<std::string> showcasePresetNames_;
    int selectedIndex_ = -1;
    std::string status_ = "Storm preset: default";
};
