#include "PostEffectShowcaseController.h"

#include "Input.h"
#include "ParticleManager.h"
#include "PostProcessRenderer.h"
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <iterator>

namespace {
/// 入力キー、PostProcess内部番号、画面表示名の対応表。
struct PostEffectBinding {
    BYTE key;
    int mode;
    const char* keyLabel;
    const char* effectName;
};

constexpr PostEffectBinding kBindings[] = {
    {DIK_1, 1, "1", "Grayscale"},
    {DIK_2, 3, "2", "Vignetting"},
    {DIK_3, 6, "3", "GaussianFilter / Smoothing"},
    {DIK_4, 4, "4", "BoxFilter 3x3"},
    {DIK_5, 5, "5", "BoxFilter 5x5"},
    {DIK_6, 7, "6", "LuminanceBasedOutline"},
    {DIK_7, 8, "7", "DepthBasedOutline"},
    {DIK_8, 9, "8", "RadialBlur"},
    {DIK_9, 10, "9", "Dissolve"},
    {DIK_0, 11, "0", "Random"},
};

const char* GetEffectName(int mode) {
    for (const PostEffectBinding& binding : kBindings) {
        if (binding.mode == mode) {
            return binding.effectName;
        }
    }
    return mode == 2 ? "Sepia" : "Normal";
}
} // namespace

void PostEffectShowcaseController::Reset(PostProcessRenderer& postProcess) {
    ReturnToNormal(postProcess);
}

bool PostEffectShowcaseController::Update(
    Input& input, ParticleManager* particleManager, PostProcessRenderer& postProcess) {
    postProcess.SetEnabled(true);

    // 画面空間エフェクトを評価しやすくするため、パーティクル演出を停止する。
    if (particleManager) {
        particleManager->SetStormActive(false);
        particleManager->SetDrawGPUParticleSphere(false);
        particleManager->ClearParticles();
    }

    if (input.TriggerKey(DIK_N)) {
        ReturnToNormal(postProcess);
    }
    for (const PostEffectBinding& binding : kBindings) {
        if (!input.TriggerKey(binding.key)) {
            continue;
        }
        if (binding.mode == 10) {
            StartDissolve(postProcess);
        } else if (binding.mode == 11) {
            dissolvePlaying_ = false;
            returnToNormalNextFrame_ = false;
            postProcess.SetPostEffectMode(binding.mode);
            postProcess.SetRandomMode(0);
            postProcess.SetRandomStrength(0.55f);
        } else {
            dissolvePlaying_ = false;
            returnToNormalNextFrame_ = false;
            postProcess.SetPostEffectMode(binding.mode);
        }
    }
    UpdateDissolve(postProcess);
    return input.TriggerKey(DIK_TAB);
}

void PostEffectShowcaseController::UpdateGameplay(
    Input& input, PostProcessRenderer& postProcess) {
    postProcess.SetEnabled(true);
    if (input.TriggerKey(DIK_N)) {
        ReturnToNormal(postProcess);
    }
    for (const PostEffectBinding& binding : kBindings) {
        if (!input.TriggerKey(binding.key)) {
            continue;
        }
        if (binding.mode == 10) {
            StartDissolve(postProcess);
        } else if (binding.mode == 11) {
            dissolvePlaying_ = false;
            returnToNormalNextFrame_ = false;
            postProcess.SetPostEffectMode(binding.mode);
            postProcess.SetRandomMode(0);
            postProcess.SetRandomStrength(0.55f);
        } else {
            dissolvePlaying_ = false;
            returnToNormalNextFrame_ = false;
            postProcess.SetPostEffectMode(binding.mode);
        }
    }
    UpdateDissolve(postProcess);
}

void PostEffectShowcaseController::StartDissolve(PostProcessRenderer& postProcess) {
    dissolvePlaying_ = true;
    returnToNormalNextFrame_ = false;
    dissolveThreshold_ = 0.0f;
    postProcess.SetDissolveThreshold(0.0f);
    postProcess.SetPostEffectMode(10);
}

void PostEffectShowcaseController::UpdateDissolve(PostProcessRenderer& postProcess) {
    if (returnToNormalNextFrame_) {
        ReturnToNormal(postProcess);
        return;
    }
    if (!dissolvePlaying_) {
        return;
    }
    dissolveThreshold_ += (1.0f / 60.0f) / kDissolveDurationSeconds;
    if (dissolveThreshold_ >= 1.0f) {
        // threshold=1の完全消去フレームを描画してから、次フレームで通常へ戻す。
        dissolvePlaying_ = false;
        dissolveThreshold_ = 1.0f;
        returnToNormalNextFrame_ = true;
        postProcess.SetDissolveThreshold(1.0f);
        return;
    }
    postProcess.SetDissolveThreshold(dissolveThreshold_);
}

void PostEffectShowcaseController::ReturnToNormal(PostProcessRenderer& postProcess) {
    dissolvePlaying_ = false;
    returnToNormalNextFrame_ = false;
    dissolveThreshold_ = 0.0f;
    postProcess.SetDissolveThreshold(0.0f);
    postProcess.SetPostEffectMode(0);
}

void PostEffectShowcaseController::DrawGameplayImGui(
    const PostProcessRenderer& postProcess) const {
    const ImGuiIO& io = ImGui::GetIO();
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    constexpr float width = 360.0f;
#ifdef NDEBUG
    const float sceneRight = io.DisplaySize.x;
    const float gameplayHeaderY = 18.0f;
#else
    // Inspectorの左端をシーンビュー右端として扱い、HUDをパネルの裏へ入れない。
    const float inspectorWidth =
        std::clamp(io.DisplaySize.x * 0.20f, 340.0f, 400.0f);
    const float sceneRight = io.DisplaySize.x - inspectorWidth;
    const float gameplayHeaderY = 56.0f;
#endif
    ImGui::SetNextWindowPos(
        ImVec2(sceneRight - width - 18.0f, gameplayHeaderY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, 74.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.68f);
    ImGui::Begin("CG5 Gameplay PostEffect", nullptr, flags);
    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.36f, 1.0f), "CG5 PostEffect: %s",
        GetEffectName(postProcess.GetPostEffectMode()));
    ImGui::TextUnformatted("N: Normal  1: Grayscale  9: Full Dissolve  2-0: Extra");
    ImGui::End();
}

void PostEffectShowcaseController::DrawImGui(const PostProcessRenderer& postProcess) const {
    const ImGuiIO& io = ImGui::GetIO();
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;

#ifdef NDEBUG
    const float headerX = 24.0f;
    const float headerWidth = io.DisplaySize.x - 48.0f;
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

    ImGui::SetNextWindowPos(ImVec2(headerX, headerY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(headerWidth, 92.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("PostEffect Showcase Header", nullptr, flags);
    ImGui::SetWindowFontScale(1.45f);
    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.36f, 1.0f), "POST EFFECT SHOWCASE");
    ImGui::SetWindowFontScale(1.15f);
    ImGui::Text("Current : %s", GetEffectName(postProcess.GetPostEffectMode()));
    ImGui::TextUnformatted("N: Normal / 9: Full Dissolve / 1-0: PostEffects");
    ImGui::End();

#ifdef NDEBUG
    const float margin = 24.0f;
    const float height = 214.0f;
    ImGui::SetNextWindowPos(ImVec2(margin, io.DisplaySize.y - height - margin), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - margin * 2.0f, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("PostEffect Showcase Controls", nullptr, flags);
    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.36f, 1.0f), "PostEffect Controls");
    ImGui::Separator();
    ImGui::TextUnformatted("Particle effects are disabled so each screen-space effect is easy to inspect.");
    ImGui::Columns(2, "PostEffectOnlyKeyColumns", false);
    for (int i = 0; i < static_cast<int>(std::size(kBindings)); ++i) {
        ImGui::Text("%s : %s", kBindings[i].keyLabel, kBindings[i].effectName);
        if (i == 4) {
            ImGui::NextColumn();
        }
    }
    ImGui::Columns(1);
    ImGui::TextUnformatted("TAB : Back to Stage Select     MMB : Orbit     Shift+MMB : Pan     Wheel : Zoom");
    ImGui::End();
#endif
}
