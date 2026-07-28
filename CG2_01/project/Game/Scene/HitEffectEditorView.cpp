#include "HitEffectEditorView.h"

#include <cstring>

#include "Camera.h"
#include "externals/imgui/imgui.h"

namespace {
void CopyPresetName(std::array<char, 64>& buffer, const std::string& name) {
    buffer.fill('\0');
    strncpy_s(buffer.data(), buffer.size(), name.c_str(), _TRUNCATE);
}
}

void HitEffectEditorView::Draw(Context& context) {
    auto* particleManager = context.particles;
    auto* camera = context.camera;
    auto& effectPreviewPosition_ = context.previewPosition;
    auto& effectPreviewAutoPlay_ = context.autoPlay;
    auto& effectPreviewInterval_ = context.interval;
    auto& effectPreviewShowGPUParticleSphere_ = context.showGpuSphere;
    auto& effectPreviewMirrorSlash_ = context.mirrorSlash;
    auto& effectPreviewStormMode_ = context.stormMode;
    auto& effectPresetIncludeInShowcase_ = context.includeInShowcase;
    auto& effectPreviewBurstCount_ = context.burstCount;
    auto& effectPreviewBurstRadius_ = context.burstRadius;
    auto& effectPreviewHitSettings_ = context.settings;
    auto& effectPresetNameBuffer_ = context.presetNameBuffer;
    auto& effectPresetNames_ = context.presetNames;
    auto& effectPresetSelectedIndex_ = context.selectedPresetIndex;
    auto& effectPresetStatus_ = context.status;
    const auto& DrawStormEffectEditorImGui = context.drawStormEditor;
    const auto& EmitEffectPreviewBurst = context.emitPreviewBurst;
    const auto& LoadEffectPresetNames = context.reloadPresetNames;
    const auto& SaveEffectPreset = context.savePreset;
    const auto& LoadEffectPreset = context.loadPreset;
    // ImGui の UI 要素を表示・更新する。
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Editor ]");
    int effectType = effectPreviewStormMode_ ? 1 : 0;
    const char* effectTypes[] = { "Hit Effect", "Tempest Storm" };
    // ImGui コンボ「Effect Type」で候補から選択する。
    if (ImGui::Combo("Effect Type", &effectType, effectTypes, IM_ARRAYSIZE(effectTypes))) {
        effectPreviewStormMode_ = effectType == 1;
        if (particleManager) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            if (effectPreviewStormMode_) {
                particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
            }
        }
    }
    if (effectPreviewStormMode_) {
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.52f, 0.72f, 1.0f, 1.0f), "Persistent preview: clouds / wind / rain / lightning");
        DrawStormEffectEditorImGui();
        return;
    }
    // ImGui テキスト「SPACE / H : Trigger」を表示する。
    ImGui::Text("SPACE / H : Trigger");
    // ImGui チェックボックス「Auto Trigger」で ON/OFF を切り替える。
    ImGui::Checkbox("Auto Trigger", &effectPreviewAutoPlay_);
    // ImGui チェックボックス「Show GPU Sphere」で ON/OFF を切り替える。
    ImGui::Checkbox("Show GPU Sphere", &effectPreviewShowGPUParticleSphere_);
    // ImGui スライダー「Interval」で小数値を調整する。
    ImGui::SliderFloat("Interval", &effectPreviewInterval_, 0.2f, 3.0f);

    // ImGui ボタンを表示し、押されたら処理する。
    if (ImGui::Button(effectPreviewStormMode_ ? "Restart Tempest Storm" : "Trigger Saber Hit", ImVec2(-1, 24)) && particleManager) {
        if (effectPreviewStormMode_) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        } else {
            EmitEffectPreviewBurst();
        }
    }
    // ImGui ボタン「Clear Particles」を表示し、押されたら処理する。
    if (ImGui::Button("Clear Particles", ImVec2(-1, 24)) && particleManager) {
        particleManager->ClearParticles();
    }

    // ImGui セクション「Core Shape」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Core Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui スライダー「Size」で小数値を調整する。
        ImGui::SliderFloat("Size", &effectPreviewHitSettings_.size, 0.2f, 3.0f);
        // ImGui スライダー「Brightness」で小数値を調整する。
        ImGui::SliderFloat("Brightness", &effectPreviewHitSettings_.brightness, 0.1f, 2.5f);
        // ImGui スライダー「Life Scale」で小数値を調整する。
        ImGui::SliderFloat("Life Scale", &effectPreviewHitSettings_.lifeScale, 0.2f, 3.0f);
        // ImGui スライダー「Slash Angle」で小数値を調整する。
        ImGui::SliderFloat("Slash Angle", &effectPreviewHitSettings_.slashAngle, -3.14f, 3.14f);
        // ImGui スライダー「Slash Spread」で小数値を調整する。
        ImGui::SliderFloat("Slash Spread", &effectPreviewHitSettings_.slashSpread, 0.2f, 3.14f);
        // ImGui チェックボックス「Mirror Slash」で ON/OFF を切り替える。
        ImGui::Checkbox("Mirror Slash", &effectPreviewMirrorSlash_);
        // ImGui スライダー「Burst Count」で整数値を調整する。
        ImGui::SliderInt("Burst Count", &effectPreviewBurstCount_, 1, 8);
        // ImGui スライダー「Burst Radius」で小数値を調整する。
        ImGui::SliderFloat("Burst Radius", &effectPreviewBurstRadius_, 0.0f, 2.0f);
    }

    // ImGui セクション「Detail」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Detail", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui スライダー「Slash Count」で整数値を調整する。
        ImGui::SliderInt("Slash Count", &effectPreviewHitSettings_.slashCount, 1, 32);
        // ImGui スライダー「Spark Count」で整数値を調整する。
        ImGui::SliderInt("Spark Count", &effectPreviewHitSettings_.sparkCount, 0, 160);
        // ImGui スライダー「Spark Speed」で小数値を調整する。
        ImGui::SliderFloat("Spark Speed", &effectPreviewHitSettings_.sparkSpeed, 0.1f, 3.0f);
        // ImGui スライダー「Spark Length」で小数値を調整する。
        ImGui::SliderFloat("Spark Length", &effectPreviewHitSettings_.sparkLength, 0.1f, 3.0f);
        // ImGui スライダー「Scatter Radius」で小数値を調整する。
        ImGui::SliderFloat("Scatter Radius", &effectPreviewHitSettings_.scatterRadius, 0.0f, 3.0f);
        // ImGui スライダー「Blue Ratio」で小数値を調整する。
        ImGui::SliderFloat("Blue Ratio", &effectPreviewHitSettings_.blueRatio, 0.0f, 1.0f);
        // ImGui スライダー「Ring Power」で小数値を調整する。
        ImGui::SliderFloat("Ring Power", &effectPreviewHitSettings_.ringPower, 0.0f, 3.0f);
        // ImGui スライダー「Core Power」で小数値を調整する。
        ImGui::SliderFloat("Core Power", &effectPreviewHitSettings_.corePower, 0.0f, 3.0f);
        // ImGui スライダー「Cross Power」で小数値を調整する。
        ImGui::SliderFloat("Cross Power", &effectPreviewHitSettings_.crossPower, 0.0f, 3.0f);
        // ImGui スライダー「Pillar Power」で小数値を調整する。
        ImGui::SliderFloat("Pillar Power", &effectPreviewHitSettings_.pillarPower, 0.0f, 3.0f);
        // ImGui スライダー「Main Bolt Count」で整数値を調整する。
        ImGui::SliderInt("Main Bolt Count", &effectPreviewHitSettings_.lightningCount, 0, 12);
        // ImGui スライダー「Lightning Segments」で整数値を調整する。
        ImGui::SliderInt("Lightning Segments", &effectPreviewHitSettings_.lightningSegments, 2, 8);
        // ImGui スライダー「Lightning Length」で小数値を調整する。
        ImGui::SliderFloat("Lightning Length", &effectPreviewHitSettings_.lightningLength, 0.1f, 4.0f);
        // ImGui スライダー「Lightning Spread」で小数値を調整する。
        ImGui::SliderFloat("Lightning Spread", &effectPreviewHitSettings_.lightningSpread, 0.0f, 3.0f);
        // ImGui スライダー「Lightning Power」で小数値を調整する。
        ImGui::SliderFloat("Lightning Power", &effectPreviewHitSettings_.lightningPower, 0.0f, 3.0f);
        // ImGui スライダー「Main Bolt Width」で小数値を調整する。
        ImGui::SliderFloat("Main Bolt Width", &effectPreviewHitSettings_.lightningWidth, 0.1f, 4.0f);
        // ImGui スライダー「Glow Width」で小数値を調整する。
        ImGui::SliderFloat("Glow Width", &effectPreviewHitSettings_.lightningGlowWidth, 1.0f, 8.0f);
        // ImGui スライダー「Glow Opacity」で小数値を調整する。
        ImGui::SliderFloat("Glow Opacity", &effectPreviewHitSettings_.lightningGlowOpacity, 0.0f, 1.0f);
        // ImGui スライダー「Branch Count」で整数値を調整する。
        ImGui::SliderInt("Branch Count", &effectPreviewHitSettings_.lightningBranchCount, 0, 12);
        // ImGui スライダー「Branch Length」で小数値を調整する。
        ImGui::SliderFloat("Branch Length", &effectPreviewHitSettings_.lightningBranchLength, 0.05f, 1.0f);
        // ImGui スライダー「Branch Spread」で小数値を調整する。
        ImGui::SliderFloat("Branch Spread", &effectPreviewHitSettings_.lightningBranchSpread, 0.0f, 1.57f);
        // ImGui スライダー「Branch Width」で小数値を調整する。
        ImGui::SliderFloat("Branch Width", &effectPreviewHitSettings_.lightningBranchWidth, 0.1f, 1.0f);
        const char* lightningModes[] = { "Radial", "Slash Forward", "Slash Axis", "Custom" };
        // ImGui コンボ「Lightning Mode」で候補から選択する。
        ImGui::Combo("Lightning Mode", &effectPreviewHitSettings_.lightningMode, lightningModes, 4);
        if (effectPreviewHitSettings_.lightningMode == 3) {
            // ImGui スライダー「Lightning Direction」で小数値を調整する。
            ImGui::SliderFloat("Lightning Direction", &effectPreviewHitSettings_.lightningDirection, -3.14f, 3.14f);
        }
        if (effectPreviewHitSettings_.lightningMode != 0) {
            // ImGui スライダー「Direction Spread」で小数値を調整する。
            ImGui::SliderFloat("Direction Spread", &effectPreviewHitSettings_.lightningDirectionSpread, 0.0f, 1.57f);
        }
    }

    // ImGui セクション「Colors」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui カラー編集「Core Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Core Color", &effectPreviewHitSettings_.coreColor.x);
        // ImGui カラー編集「Slash Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Slash Color", &effectPreviewHitSettings_.slashColor.x);
        // ImGui カラー編集「Spark Primary」で RGBA 色を調整する。
        ImGui::ColorEdit4("Spark Primary", &effectPreviewHitSettings_.sparkColor.x);
        // ImGui カラー編集「Spark Secondary」で RGBA 色を調整する。
        ImGui::ColorEdit4("Spark Secondary", &effectPreviewHitSettings_.sparkSecondaryColor.x);
        // ImGui カラー編集「Ring Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Ring Color", &effectPreviewHitSettings_.ringColor.x);
        // ImGui カラー編集「Cross Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Cross Color", &effectPreviewHitSettings_.crossColor.x);
        // ImGui カラー編集「Pillar Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Pillar Color", &effectPreviewHitSettings_.pillarColor.x);
        // ImGui カラー編集「Lightning Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Lightning Color", &effectPreviewHitSettings_.lightningColor.x);
        // ImGui カラー編集「Lightning Glow」で RGBA 色を調整する。
        ImGui::ColorEdit4("Lightning Glow", &effectPreviewHitSettings_.lightningGlowColor.x);
    }

    // ImGui セクション「Randomization」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Randomization", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui チェックボックス「Random Position」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Position", &effectPreviewHitSettings_.randomizePosition);
        // ImGui チェックボックス「Random Direction」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Direction", &effectPreviewHitSettings_.randomizeDirection);
        // ImGui チェックボックス「Random Angle」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Angle", &effectPreviewHitSettings_.randomizeAngle);
        if (effectPreviewHitSettings_.randomizeAngle) {
            // ImGui スライダー「Angle Random Range」で小数値を調整する。
            ImGui::SliderFloat("Angle Random Range", &effectPreviewHitSettings_.angleRandomRange, 0.0f, 3.14f);
        }
        // ImGui チェックボックス「Random Scale」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Scale", &effectPreviewHitSettings_.randomizeScale);
        // ImGui チェックボックス「Random Lifetime」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Lifetime", &effectPreviewHitSettings_.randomizeLifetime);
        // ImGui チェックボックス「Random Color」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Color", &effectPreviewHitSettings_.randomizeColor);
    }

    // ImGui セクション「Presets」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui ボタン「Saber Impact」を表示し、押されたら処理する。
        if (ImGui::Button("Saber Impact", ImVec2(140, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.25f;
            effectPreviewHitSettings_.brightness = 1.55f;
            effectPreviewHitSettings_.slashCount = 18;
            effectPreviewHitSettings_.sparkCount = 84;
            effectPreviewHitSettings_.ringPower = 1.35f;
            effectPreviewHitSettings_.corePower = 1.15f;
            effectPreviewHitSettings_.crossPower = 1.3f;
            effectPreviewHitSettings_.pillarPower = 0.8f;
            effectPreviewBurstCount_ = 2;
            effectPreviewBurstRadius_ = 0.18f;
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Blue Flash」を表示し、押されたら処理する。
        if (ImGui::Button("Blue Flash", ImVec2(120, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.brightness = 1.9f;
            effectPreviewHitSettings_.lifeScale = 0.85f;
            effectPreviewHitSettings_.slashSpread = 2.2f;
            effectPreviewHitSettings_.sparkCount = 112;
            effectPreviewHitSettings_.blueRatio = 0.95f;
            effectPreviewHitSettings_.corePower = 1.6f;
            effectPreviewHitSettings_.crossPower = 1.8f;
            effectPreviewHitSettings_.pillarPower = 1.2f;
            effectPreviewHitSettings_.slashColor = { 0.42f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkColor = { 0.42f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.crossColor = { 0.55f, 0.82f, 1.0f, 1.0f };
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        // ImGui ボタン「Spark Burst」を表示し、押されたら処理する。
        if (ImGui::Button("Spark Burst", ImVec2(140, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 0.9f;
            effectPreviewHitSettings_.brightness = 1.35f;
            effectPreviewHitSettings_.lifeScale = 1.45f;
            effectPreviewHitSettings_.slashCount = 7;
            effectPreviewHitSettings_.sparkCount = 150;
            effectPreviewHitSettings_.sparkSpeed = 2.15f;
            effectPreviewHitSettings_.sparkLength = 1.6f;
            effectPreviewHitSettings_.scatterRadius = 1.6f;
            effectPreviewHitSettings_.blueRatio = 0.25f;
            effectPreviewHitSettings_.ringPower = 0.65f;
            effectPreviewHitSettings_.corePower = 0.8f;
            effectPreviewHitSettings_.crossPower = 0.55f;
            effectPreviewHitSettings_.pillarPower = 0.2f;
            effectPreviewBurstCount_ = 3;
            effectPreviewBurstRadius_ = 0.35f;
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Heavy Hit」を表示し、押されたら処理する。
        if (ImGui::Button("Heavy Hit", ImVec2(120, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.85f;
            effectPreviewHitSettings_.brightness = 1.25f;
            effectPreviewHitSettings_.lifeScale = 2.0f;
            effectPreviewHitSettings_.slashSpread = 1.05f;
            effectPreviewHitSettings_.slashCount = 13;
            effectPreviewHitSettings_.sparkCount = 44;
            effectPreviewHitSettings_.ringPower = 2.2f;
            effectPreviewHitSettings_.corePower = 1.35f;
            effectPreviewHitSettings_.crossPower = 0.85f;
            effectPreviewHitSettings_.pillarPower = 1.65f;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        // ImGui ボタン「Cinematic Finisher」を表示し、押されたら処理する。
        if (ImGui::Button("Cinematic Finisher", ImVec2(-1, 28))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 2.15f;
            effectPreviewHitSettings_.brightness = 2.15f;
            effectPreviewHitSettings_.lifeScale = 1.55f;
            effectPreviewHitSettings_.slashAngle = -0.42f;
            effectPreviewHitSettings_.slashSpread = 2.75f;
            effectPreviewHitSettings_.slashCount = 30;
            effectPreviewHitSettings_.sparkCount = 160;
            effectPreviewHitSettings_.sparkSpeed = 2.45f;
            effectPreviewHitSettings_.sparkLength = 2.2f;
            effectPreviewHitSettings_.scatterRadius = 1.35f;
            effectPreviewHitSettings_.blueRatio = 0.72f;
            effectPreviewHitSettings_.ringPower = 2.8f;
            effectPreviewHitSettings_.corePower = 2.25f;
            effectPreviewHitSettings_.crossPower = 2.65f;
            effectPreviewHitSettings_.pillarPower = 1.45f;
            effectPreviewHitSettings_.slashColor = { 0.38f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkColor = { 0.38f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkSecondaryColor = { 1.0f, 0.48f, 0.10f, 1.0f };
            effectPreviewHitSettings_.ringColor = { 0.38f, 0.78f, 1.0f, 1.0f };
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 4;
            effectPreviewBurstRadius_ = 0.28f;
            EmitEffectPreviewBurst();
        }
        // ImGui ボタン「Lightning Slash」を表示し、押されたら処理する。
        if (ImGui::Button("Lightning Slash", ImVec2(-1, 28))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.55f;
            effectPreviewHitSettings_.brightness = 2.05f;
            effectPreviewHitSettings_.lifeScale = 0.95f;
            effectPreviewHitSettings_.slashAngle = -0.28f;
            effectPreviewHitSettings_.slashSpread = 0.55f;
            effectPreviewHitSettings_.slashCount = 1;
            effectPreviewHitSettings_.sparkCount = 92;
            effectPreviewHitSettings_.sparkSpeed = 1.85f;
            effectPreviewHitSettings_.sparkLength = 1.75f;
            effectPreviewHitSettings_.scatterRadius = 1.05f;
            effectPreviewHitSettings_.blueRatio = 0.88f;
            effectPreviewHitSettings_.ringPower = 1.25f;
            effectPreviewHitSettings_.corePower = 1.55f;
            effectPreviewHitSettings_.crossPower = 1.35f;
            effectPreviewHitSettings_.pillarPower = 0.45f;
            effectPreviewHitSettings_.lightningCount = 3;
            effectPreviewHitSettings_.lightningSegments = 5;
            effectPreviewHitSettings_.lightningLength = 2.15f;
            effectPreviewHitSettings_.lightningSpread = 1.55f;
            effectPreviewHitSettings_.lightningPower = 1.55f;
            effectPreviewHitSettings_.lightningWidth = 2.2f;
            effectPreviewHitSettings_.lightningGlowWidth = 3.6f;
            effectPreviewHitSettings_.lightningGlowOpacity = 0.28f;
            effectPreviewHitSettings_.lightningBranchCount = 4;
            effectPreviewHitSettings_.lightningBranchLength = 0.38f;
            effectPreviewHitSettings_.lightningBranchSpread = 0.72f;
            effectPreviewHitSettings_.lightningBranchWidth = 0.38f;
            effectPreviewHitSettings_.lightningMode = 2;
            effectPreviewHitSettings_.lightningDirectionSpread = 0.28f;
            effectPreviewHitSettings_.lightningColor = { 0.40f, 0.86f, 1.0f, 1.0f };
            effectPreviewHitSettings_.lightningGlowColor = { 0.24f, 0.28f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkColor = { 0.40f, 0.86f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkSecondaryColor = { 1.0f, 0.86f, 0.40f, 1.0f };
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 2;
            effectPreviewBurstRadius_ = 0.16f;
            EmitEffectPreviewBurst();
        }
        // ImGui ボタン「Shock Ring」を表示し、押されたら処理する。
        if (ImGui::Button("Shock Ring", ImVec2(140, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.35f;
            effectPreviewHitSettings_.brightness = 1.75f;
            effectPreviewHitSettings_.lifeScale = 1.35f;
            effectPreviewHitSettings_.slashCount = 1;
            effectPreviewHitSettings_.slashSpread = 0.2f;
            effectPreviewHitSettings_.sparkCount = 32;
            effectPreviewHitSettings_.sparkSpeed = 0.75f;
            effectPreviewHitSettings_.sparkLength = 0.9f;
            effectPreviewHitSettings_.scatterRadius = 0.45f;
            effectPreviewHitSettings_.ringPower = 3.0f;
            effectPreviewHitSettings_.corePower = 1.25f;
            effectPreviewHitSettings_.crossPower = 0.15f;
            effectPreviewHitSettings_.pillarPower = 0.15f;
            effectPreviewHitSettings_.lightningCount = 6;
            effectPreviewHitSettings_.lightningSegments = 6;
            effectPreviewHitSettings_.lightningLength = 0.8f;
            effectPreviewHitSettings_.lightningSpread = 2.4f;
            effectPreviewHitSettings_.lightningPower = 0.75f;
            effectPreviewHitSettings_.lightningMode = 0;
            effectPreviewHitSettings_.lightningColor = { 0.52f, 0.92f, 1.0f, 1.0f };
            effectPreviewHitSettings_.ringColor = { 0.52f, 0.92f, 1.0f, 1.0f };
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
            EmitEffectPreviewBurst();
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Thin Cut」を表示し、押されたら処理する。
        if (ImGui::Button("Thin Cut", ImVec2(120, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.2f;
            effectPreviewHitSettings_.brightness = 2.2f;
            effectPreviewHitSettings_.lifeScale = 0.62f;
            effectPreviewHitSettings_.slashAngle = -0.64f;
            effectPreviewHitSettings_.slashSpread = 0.2f;
            effectPreviewHitSettings_.slashCount = 1;
            effectPreviewHitSettings_.sparkCount = 24;
            effectPreviewHitSettings_.sparkSpeed = 1.25f;
            effectPreviewHitSettings_.sparkLength = 1.25f;
            effectPreviewHitSettings_.scatterRadius = 0.25f;
            effectPreviewHitSettings_.blueRatio = 1.0f;
            effectPreviewHitSettings_.ringPower = 0.35f;
            effectPreviewHitSettings_.corePower = 0.75f;
            effectPreviewHitSettings_.crossPower = 0.25f;
            effectPreviewHitSettings_.pillarPower = 0.0f;
            effectPreviewHitSettings_.lightningCount = 2;
            effectPreviewHitSettings_.lightningSegments = 3;
            effectPreviewHitSettings_.lightningLength = 1.15f;
            effectPreviewHitSettings_.lightningSpread = 0.55f;
            effectPreviewHitSettings_.lightningPower = 0.55f;
            effectPreviewHitSettings_.lightningMode = 1;
            effectPreviewHitSettings_.lightningDirectionSpread = 0.12f;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
            EmitEffectPreviewBurst();
        }
    }

    // ImGui セクション「Saved Presets」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Saved Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui チェックボックス「Include in Showcase」で ON/OFF を切り替える。
        ImGui::Checkbox("Include in Showcase", &effectPresetIncludeInShowcase_);
        // ImGui 入力欄「Preset Name」で名前や文字列を編集する。
        ImGui::InputText("Preset Name", effectPresetNameBuffer_.data(), effectPresetNameBuffer_.size());
        // ImGui ボタン「Save Preset」を表示し、押されたら処理する。
        if (ImGui::Button("Save Preset", ImVec2(135, 24))) {
            SaveEffectPreset(effectPresetNameBuffer_.data());
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Refresh」を表示し、押されたら処理する。
        if (ImGui::Button("Refresh", ImVec2(100, 24))) {
            LoadEffectPresetNames();
        }

        const char* selectedPresetName = effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())
            ? effectPresetNames_[effectPresetSelectedIndex_].c_str()
            : "Select saved preset";
        // ImGui コンボ「Saved」の選択リストを開始する。
        if (ImGui::BeginCombo("Saved", selectedPresetName)) {
            for (int i = 0; i < static_cast<int>(effectPresetNames_.size()); ++i) {
                bool selected = effectPresetSelectedIndex_ == i;
                // ImGui の選択項目を表示し、選ばれたら選択状態を更新する。
                if (ImGui::Selectable(effectPresetNames_[i].c_str(), selected)) {
                    effectPresetSelectedIndex_ = i;
                    CopyPresetName(effectPresetNameBuffer_, effectPresetNames_[i]);
                }
                if (selected) {
                    // 現在選択中の ImGui 項目へ既定フォーカスを当てる。
                    ImGui::SetItemDefaultFocus();
                }
            }
            // ImGui コンボの選択リストを閉じる。
            ImGui::EndCombo();
        }
        // ImGui ボタン「Load Selected」を表示し、押されたら処理する。
        if (ImGui::Button("Load Selected", ImVec2(135, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                LoadEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                effectPresetStatus_ = "Preset: nothing selected";
            }
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Save Over」を表示し、押されたら処理する。
        if (ImGui::Button("Save Over", ImVec2(100, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                SaveEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                SaveEffectPreset(effectPresetNameBuffer_.data());
            }
        }
        // ImGui に折り返し付きのステータス文字列を表示する。
        ImGui::TextWrapped("%s", effectPresetStatus_.c_str());
    }

    // ImGui セクション「Spawn」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui ドラッグ入力「Position」で 3D 座標を調整する。
        ImGui::DragFloat3("Position", &effectPreviewPosition_.x, 0.05f, -20.0f, 20.0f);
        // ImGui ボタン「Focus Camera」を表示し、押されたら処理する。
        if (ImGui::Button("Focus Camera", ImVec2(-1, 24))) {
            camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
        }
        // ImGui ボタン「Reset Tuning」を表示し、押されたら処理する。
        if (ImGui::Button("Reset Tuning", ImVec2(-1, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
    }
}
