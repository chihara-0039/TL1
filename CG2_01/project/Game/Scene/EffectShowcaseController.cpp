#include "EffectShowcaseController.h"

#include <algorithm>

#include "Input.h"
#include "ParticleManager.h"
#include "externals/imgui/imgui.h"

void EffectShowcaseController::SetPresets(
    std::vector<std::string> presets, std::vector<std::string> stormPresets) {
    presetNames_ = std::move(presets);
    stormPresetNames_ = std::move(stormPresets);
    if (presetNames_.empty()) {
        selectedIndex_ = 0;
    } else {
        selectedIndex_ = std::clamp(selectedIndex_, 0, static_cast<int>(presetNames_.size()) - 1);
    }
}

void EffectShowcaseController::Reset() {
    selectedIndex_ = 0;
    timer_ = 0.0f;
    firstPlay_ = true;
}

bool EffectShowcaseController::IsStormIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(presetNames_.size())) {
        return false;
    }
    return std::find(stormPresetNames_.begin(), stormPresetNames_.end(), presetNames_[index]) !=
        stormPresetNames_.end();
}

bool EffectShowcaseController::IsCurrentStorm() const {
    return IsStormIndex(selectedIndex_);
}

void EffectShowcaseController::SelectPreset(
    int index, bool play, ParticleManager* particles, const Vector3& position,
    const LoadPreset& loadStorm, const LoadPreset& loadHit, const SimpleAction& emitHit) {
    if (presetNames_.empty()) {
        return;
    }
    const int count = static_cast<int>(presetNames_.size());
    selectedIndex_ = (index % count + count) % count;
    timer_ = 0.0f;
    if (particles) {
        particles->SetStormActive(false);
        particles->ClearParticles();
    }

    if (IsCurrentStorm()) {
        loadStorm(presetNames_[selectedIndex_]);
        if (particles) {
            particles->SetStormActive(true, {position.x, 0.0f, position.z});
        }
    } else {
        loadHit(presetNames_[selectedIndex_]);
        if (play) {
            emitHit();
        }
    }
}

bool EffectShowcaseController::Update(
    Input& input, ParticleManager* particles, const Vector3& position,
    const LoadPreset& loadStorm, const LoadPreset& loadHit,
    const SimpleAction& emitHit, const SimpleAction& refreshPresets) {
    if (firstPlay_) {
        firstPlay_ = false;
        SelectPreset(selectedIndex_, true, particles, position, loadStorm, loadHit, emitHit);
    }
    if (input.TriggerKey(DIK_LEFT)) SelectPreset(selectedIndex_ - 1, true, particles, position, loadStorm, loadHit, emitHit);
    if (input.TriggerKey(DIK_RIGHT)) SelectPreset(selectedIndex_ + 1, true, particles, position, loadStorm, loadHit, emitHit);
    if (input.TriggerKey(DIK_SPACE)) SelectPreset(selectedIndex_, true, particles, position, loadStorm, loadHit, emitHit);
    if (input.TriggerKey(DIK_A)) { autoPlay_ = !autoPlay_; timer_ = 0.0f; }
    if (input.TriggerKey(DIK_R)) { refreshPresets(); Reset(); return false; }
    if (input.TriggerKey(DIK_TAB)) {
        if (particles) { particles->SetStormActive(false); particles->ClearParticles(); }
        return true;
    }

    if (autoPlay_ && !presetNames_.empty()) {
        timer_ += 1.0f / 60.0f;
        const float displayInterval = IsCurrentStorm() ? 10.0f : interval_;
        if (timer_ >= displayInterval) {
            SelectPreset(selectedIndex_ + 1, true, particles, position, loadStorm, loadHit, emitHit);
        }
    }
    return false;
}

void EffectShowcaseController::NotifyImpact() { lightTimer_ = kLightDuration; }
void EffectShowcaseController::TickLight(float deltaTime) { lightTimer_ = (std::max)(0.0f, lightTimer_ - deltaTime); }
float EffectShowcaseController::GetLightRatio() const { return std::clamp(lightTimer_ / kLightDuration, 0.0f, 1.0f); }

void EffectShowcaseController::DrawImGui() const {
    const ImGuiIO& io = ImGui::GetIO();
    const int count = static_cast<int>(presetNames_.size());
    const int safeIndex = count > 0 ? std::clamp(selectedIndex_, 0, count - 1) : 0;
    const char* name = count > 0 ? presetNames_[safeIndex].c_str() : "No showcase presets";
#ifdef NDEBUG
    const float headerX = 24.0f, headerWidth = io.DisplaySize.x - 48.0f;
    const float headerY = 20.0f;
#else
    const float hierarchyWidth =
        std::clamp(io.DisplaySize.x * 0.15f, 240.0f, 300.0f);
    const float inspectorWidth =
        std::clamp(io.DisplaySize.x * 0.20f, 340.0f, 400.0f);
    const float headerX = hierarchyWidth + 24.0f;
    const float headerWidth =
        io.DisplaySize.x - hierarchyWidth - inspectorWidth - 48.0f;
    const float headerY = 58.0f;
#endif
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    ImGui::SetNextWindowPos(
        ImVec2(headerX, headerY),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(headerWidth, 94.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("Effect Showcase Header", nullptr, flags);
    ImGui::SetWindowFontScale(1.45f);
    ImGui::TextColored(ImVec4(0.42f, 0.86f, 1.0f, 1.0f), "EFFECT SHOWCASE");
    ImGui::SetWindowFontScale(1.15f);
    ImGui::Text("%02d / %02d    %s", count > 0 ? safeIndex + 1 : 0, count, name);
    ImGui::End();
#ifdef NDEBUG
    ImGui::SetNextWindowPos(ImVec2(24.0f, io.DisplaySize.y - 120.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 48.0f, 96.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("Effect Showcase Controls", nullptr, flags);
    ImGui::Text("LEFT / RIGHT : Select    SPACE : Replay    A : Auto [%s]    TAB : Back", autoPlay_ ? "ON" : "OFF");
    ImGui::TextUnformatted("MMB : Orbit    Shift + MMB : Pan    Wheel : Zoom");
    ImGui::End();
#endif
}
