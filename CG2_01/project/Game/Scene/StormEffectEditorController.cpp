#include "StormEffectEditorController.h"

#include <algorithm>
#include <cstring>

#include "EffectPresetStore.h"
#include "ParticleManager.h"
#include "externals/imgui/imgui.h"

namespace {
constexpr const char* kStormPresetPath = "Resources/presets/storm_effect_presets.json";
constexpr const char* kDefaultStormName = "Tempest Storm";
}

void StormEffectEditorController::Initialize() {
    ReloadNames();
}

void StormEffectEditorController::Draw(ParticleManager& particles, const Vector3& previewPosition) {
    auto& s = particles.GetStormSettings();
    ImGui::TextColored(ImVec4(0.48f, 0.70f, 1.0f, 1.0f), "[ Storm Editor ]");
    if (ImGui::Button("Restart Storm", ImVec2(-1, 26))) {
        particles.SetStormActive(false); particles.ClearParticles();
        particles.SetStormActive(true, { previewPosition.x, 0.0f, previewPosition.z });
    }
    if (ImGui::CollapsingHeader("Dark Clouds", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Cloud Area X", &s.cloudAreaX, 0.0f, 15.0f); ImGui::SliderFloat("Cloud Area Z", &s.cloudAreaZ, 0.0f, 12.0f);
        ImGui::SliderFloat("Cloud Height", &s.cloudHeight, 1.5f, 10.0f); ImGui::SliderFloat("Cloud Emit Rate", &s.cloudEmitRate, 0.5f, 20.0f);
        ImGui::SliderFloat("Cloud Life", &s.cloudLife, 1.0f, 12.0f); ImGui::SliderFloat("Cloud Size", &s.cloudSize, 0.2f, 3.0f);
        ImGui::ColorEdit4("Cloud Color", &s.cloudColor.x); ImGui::Checkbox("Random Cloud Position", &s.randomizeCloudPosition); ImGui::Checkbox("Random Cloud Size", &s.randomizeCloudSize);
    }
    if (ImGui::CollapsingHeader("Rain", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Rain Area X", &s.rainAreaX, 0.0f, 15.0f); ImGui::SliderFloat("Rain Area Z", &s.rainAreaZ, 0.0f, 12.0f);
        ImGui::SliderFloat("Rain Emit Rate", &s.rainEmitRate, 1.0f, 180.0f); ImGui::SliderFloat("Rain Speed", &s.rainSpeed, 0.1f, 3.0f);
        ImGui::SliderFloat("Rain Length", &s.rainLength, 0.2f, 3.0f); ImGui::ColorEdit4("Rain Color", &s.rainColor.x);
        ImGui::Checkbox("Random Rain Position", &s.randomizeRainPosition); ImGui::Checkbox("Random Rain Speed", &s.randomizeRainSpeed);
    }
    if (ImGui::CollapsingHeader("Wind", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Wind Emit Rate", &s.windEmitRate, 0.5f, 40.0f); ImGui::SliderFloat("Wind Speed", &s.windSpeed, 0.1f, 3.0f);
        ImGui::SliderFloat("Wind Length", &s.windLength, 0.2f, 4.0f); ImGui::ColorEdit4("Wind Color", &s.windColor.x);
    }
    if (ImGui::CollapsingHeader("Lightning", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Lightning Area X", &s.lightningAreaX, 0.0f, 12.0f); ImGui::SliderFloat("Lightning Area Z", &s.lightningAreaZ, 0.0f, 10.0f);
        ImGui::SliderFloat("Lightning Frequency", &s.lightningFrequency, 0.1f, 5.0f, "%.2fx"); ImGui::SliderFloat("Interval Min", &s.lightningIntervalMin, 0.15f, 5.0f);
        ImGui::SliderFloat("Interval Max", &s.lightningIntervalMax, 0.2f, 8.0f); ImGui::SliderFloat("Strike Size", &s.lightningStrikeSize, 0.25f, 3.0f, "%.2fx");
        ImGui::Checkbox("Random Strike Size", &s.randomizeLightningSize); ImGui::SliderFloat2("Strike Size Range", &s.lightningSizeRandomMin, 0.1f, 2.5f, "%.2fx");
        ImGui::SliderFloat2("Bolt Length Range", &s.lightningLengthRandomMin, 0.1f, 2.5f, "%.2fx"); ImGui::SliderFloat2("Bolt Width Range", &s.lightningWidthRandomMin, 0.1f, 2.5f, "%.2fx");
        ImGui::SliderFloat2("Center Spark Range", &s.lightningCoreRandomMin, 0.1f, 2.5f, "%.2fx"); ImGui::SliderInt("Simultaneous Strikes", &s.lightningSimultaneousCount, 1, 8);
        ImGui::SliderFloat("Simultaneous Spread", &s.lightningSimultaneousSpread, 0.0f, 8.0f); ImGui::SliderInt("Burst Count", &s.lightningBurstCount, 1, 12);
        ImGui::SliderFloat("Burst Interval", &s.lightningBurstInterval, 0.02f, 0.8f, "%.2f sec"); ImGui::Checkbox("Random Burst Count", &s.randomizeLightningBurstCount);
        ImGui::Checkbox("Random Lightning Position", &s.randomizeLightningPosition); ImGui::Checkbox("Random Lightning Interval", &s.randomizeLightningInterval); ImGui::Checkbox("Random Lightning Direction", &s.randomizeLightningDirection);
        ImGui::SliderInt("Bolt Count", &s.lightningCount, 1, 12); ImGui::SliderInt("Segments", &s.lightningSegments, 2, 16); ImGui::SliderFloat("Bolt Length", &s.lightningLength, 1.0f, 10.0f);
        ImGui::SliderFloat("Bolt Spread", &s.lightningSpread, 0.0f, 3.0f); ImGui::SliderFloat("Bolt Power", &s.lightningPower, 0.1f, 3.0f); ImGui::SliderFloat("Bolt Width", &s.lightningWidth, 0.1f, 4.0f);
        ImGui::SliderFloat("Glow Width", &s.lightningGlowWidth, 1.0f, 8.0f); ImGui::SliderFloat("Glow Opacity", &s.lightningGlowOpacity, 0.0f, 1.0f);
        ImGui::SliderInt("Branch Count", &s.lightningBranchCount, 0, 12); ImGui::Checkbox("Random Branch Count", &s.randomizeLightningBranchCount);
        ImGui::SliderFloat("Branch Length", &s.lightningBranchLength, 0.05f, 1.0f); ImGui::SliderFloat("Branch Spread", &s.lightningBranchSpread, 0.0f, 1.57f); ImGui::SliderFloat("Branch Width", &s.lightningBranchWidth, 0.1f, 1.5f);
        ImGui::ColorEdit4("Lightning Color", &s.lightningColor.x); ImGui::ColorEdit4("Lightning Glow", &s.lightningGlowColor.x); ImGui::SliderFloat("Ground Light Power", &s.pointLightPower, 0.0f, 24.0f);
    }
    if (ImGui::CollapsingHeader("Storm Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Include in Showcase", &includeInShowcase_); ImGui::InputText("Storm Preset Name", nameBuffer_.data(), nameBuffer_.size());
        if (ImGui::Button("Save Storm Preset", ImVec2(-1, 24))) Save(nameBuffer_.data(), particles);
        const char* selected = selectedIndex_ >= 0 ? presetNames_[selectedIndex_].c_str() : "Select storm preset";
        if (ImGui::BeginCombo("Saved Storms", selected)) {
            for (int i = 0; i < static_cast<int>(presetNames_.size()); ++i) { const bool active = i == selectedIndex_; if (ImGui::Selectable(presetNames_[i].c_str(), active)) Select(i); if (active) ImGui::SetItemDefaultFocus(); }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Load Storm Preset", ImVec2(-1, 24)) && selectedIndex_ >= 0) {
            Load(presetNames_[selectedIndex_], particles); particles.SetStormActive(false); particles.ClearParticles(); particles.SetStormActive(true, { previewPosition.x, 0.0f, previewPosition.z });
        }
        if (ImGui::Button("Reset Storm Defaults", ImVec2(-1, 24))) s = ParticleManager::StormEffectSettings{};
        ImGui::TextWrapped("%s", status_.c_str());
    }
}

bool StormEffectEditorController::Save(const std::string& name, const ParticleManager& particles) {
    if (name.empty()) {
        status_ = "Storm preset: invalid name";
        return false;
    }

    EffectPresetStore store;
    EffectPresetStore::StormPreset preset;
    preset.settings = particles.GetStormSettings();
    preset.includeInShowcase = includeInShowcase_;
    if (!store.SaveStormPreset(kStormPresetPath, name, preset, status_)) {
        return false;
    }

    ReloadNames();
    const auto it = std::find(presetNames_.begin(), presetNames_.end(), name);
    selectedIndex_ = it == presetNames_.end() ? -1 : static_cast<int>(std::distance(presetNames_.begin(), it));
    SetNameBuffer(name);
    return true;
}

bool StormEffectEditorController::Load(const std::string& name, ParticleManager& particles) {
    EffectPresetStore store;
    EffectPresetStore::StormPreset preset;
    preset.settings = particles.GetStormSettings();
    preset.includeInShowcase = includeInShowcase_;
    if (!store.LoadStormPreset(kStormPresetPath, kDefaultStormName, name, preset, status_)) {
        return false;
    }

    particles.GetStormSettings() = preset.settings;
    includeInShowcase_ = preset.includeInShowcase;
    SetNameBuffer(name);
    return true;
}

bool StormEffectEditorController::IsStormPreset(const std::string& name) const {
    return std::find(presetNames_.begin(), presetNames_.end(), name) != presetNames_.end();
}

void StormEffectEditorController::Select(int index) {
    selectedIndex_ = index >= 0 && index < static_cast<int>(presetNames_.size()) ? index : -1;
    if (selectedIndex_ >= 0) {
        SetNameBuffer(presetNames_[selectedIndex_]);
    }
}

void StormEffectEditorController::ReloadNames() {
    EffectPresetStore store;
    const auto result = store.LoadStormPresetNames(kStormPresetPath, kDefaultStormName);
    presetNames_ = result.all;
    showcasePresetNames_ = result.showcase;
    selectedIndex_ = -1;
    if (!result.status.empty()) {
        status_ = result.status;
    }
}

void StormEffectEditorController::SetNameBuffer(const std::string& name) {
    nameBuffer_.fill('\0');
    strncpy_s(nameBuffer_.data(), nameBuffer_.size(), name.c_str(), _TRUNCATE);
}
