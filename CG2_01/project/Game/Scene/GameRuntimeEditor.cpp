// デバッグビルド専用の編集UIを、ゲーム進行処理から分離する。
#include "GameRuntime.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <vector>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

namespace {
// エディタの各パネル寸法をまとめ、画面サイズ変更時の計算を一か所に保つ。
struct EditorLayout {
    float leftPanelWidth = 280.0f;
    float rightPanelWidth = 380.0f;
    float toolbarHeight = 38.0f;
    float bottomPanelHeight = 340.0f;
    float viewportX = 280.0f;
    float viewportY = 38.0f;
    float viewportWidth = 1260.0f;
    float viewportHeight = 702.0f;
    float sidePanelHeight = 1042.0f;
};

// ゲーム表示領域を確保しながら、現在のウィンドウに合うパネル寸法を求める。
EditorLayout MakeEditorLayout(const ImVec2& displaySize) {
    EditorLayout layout;
    const float width=(std::max)(displaySize.x,1.0f), height=(std::max)(displaySize.y,1.0f);
    const float leftPanel = std::clamp(width * 0.15f, 240.0f, 300.0f);
    const float rightPanel = std::clamp(width * 0.20f, 340.0f, 400.0f);
    const float toolbarHeight = 38.0f;
    float bottomPanel=std::clamp(height*0.32f,280.0f,420.0f);
    if (height < 820.0f) {
        bottomPanel = std::clamp(height * 0.28f, 220.0f, 320.0f);
    }
    layout.leftPanelWidth=leftPanel;
    layout.rightPanelWidth=rightPanel;
    layout.toolbarHeight=toolbarHeight;
    layout.bottomPanelHeight=bottomPanel;
    layout.viewportX=leftPanel;
    layout.viewportY=toolbarHeight;
    layout.viewportWidth=(std::max)(480.0f,width-leftPanel-rightPanel);
    layout.viewportHeight=(std::max)(300.0f,height-toolbarHeight-bottomPanel);
    layout.sidePanelHeight=(std::max)(300.0f,height-toolbarHeight);
    return layout;
}

// プリセット名を固定長入力バッファへ安全にコピーする。
void CopyPresetName(std::array<char,64>& buffer,const std::string& name){ buffer.fill('\0'); strncpy_s(buffer.data(),buffer.size(),name.c_str(),_TRUNCATE); }

// ポストエフェクト番号をUI表示名へ変換する。
const char* GetPostEffectShowcaseName(int mode) {
    switch(mode){case 1:return "Grayscale";case 2:return "Sepia";case 3:return "Vignetting";case 4:return "BoxFilter 3x3";case 5:return "BoxFilter 5x5";case 6:return "GaussianFilter / Smoothing";case 7:return "LuminanceBasedOutline";case 8:return "DepthBasedOutline";case 9:return "RadialBlur";case 10:return "Dissolve";case 11:return "Random";default:return "Normal";}
}
} // namespace

#ifndef NDEBUG
void GameRuntime::DrawStormEffectEditorImGui() {
    if (particleManager) {
        stormEffectEditor_.Draw(*particleManager, effectPreviewPosition_);
    }
}

void GameRuntime::DrawEffectPreviewEditorImGui() {
    HitEffectEditorView::Context context{
        particleManager.get(), camera.get(), effectPreviewPosition_,
        effectPreviewAutoPlay_, effectPreviewInterval_, effectPreviewShowGPUParticleSphere_,
        effectPreviewMirrorSlash_, effectPreviewStormMode_, effectPresetIncludeInShowcase_,
        effectPreviewBurstCount_, effectPreviewBurstRadius_, effectPreviewHitSettings_,
        effectPresetNameBuffer_, effectPresetNames_, effectPresetSelectedIndex_, effectPresetStatus_,
        [this]() { DrawStormEffectEditorImGui(); },
        [this]() { EmitEffectPreviewBurst(); },
        [this]() { LoadEffectPresetNames(); },
        [this](const std::string& name) { return SaveEffectPreset(name); },
        [this](const std::string& name) { return LoadEffectPreset(name); }
    };
    hitEffectEditorView_.Draw(context);
}

void GameRuntime::UpdateImGui() {
    ImGuiIO& io        = ImGui::GetIO();
    const EditorLayout layout = MakeEditorLayout(io.DisplaySize);
    camera->SetAspectRatio(layout.viewportWidth / layout.viewportHeight);

    // Unity風の上部ツールバー。主要モードへ常に同じ位置から移動できる。
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, layout.toolbarHeight), ImGuiCond_Always);
    ImGui::Begin(
        "Main Toolbar", nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextUnformatted("CG2 Engine");
    ImGui::SameLine();
    if (ImGui::Button("Scene")) {
        RequestSceneChange(SceneType::StageEditor);
    }
    ImGui::SameLine();
    if (ImGui::Button("Play")) {
        RequestSceneChange(SceneType::GamePlay);
    }
    ImGui::SameLine();
    if (ImGui::Button("Effects")) {
        RequestSceneChange(SceneType::EffectPreview);
    }
    ImGui::SameLine();
    if (ImGui::Button("Animation")) {
        RequestSceneChange(SceneType::SkinningEditor);
    }
    ImGui::End();

    // 左列はHierarchyとシーン共通設定。
    ImGui::SetNextWindowPos(
        ImVec2(0, layout.toolbarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(layout.leftPanelWidth, layout.sidePanelHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::Text("FPS: %.1f (%.3f ms/f)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::SameLine(layout.leftPanelWidth - 60.0f);
    if (ImGui::Button("Exit", ImVec2(50, 20))) {
        PostQuitMessage(0);
    }
    ImGui::Separator();

    const bool isStageToolMode = (currentMode_ == AppMode::StageEditor ||
                                  currentMode_ == AppMode::GamePlay_BlockPlace);


    if (ImGui::CollapsingHeader("Hierarchy / Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        const SceneType selectableScenes[] = {
            SceneType::DebugView,
            SceneType::StageEditor,
            SceneType::GamePlay,
            SceneType::SkinningEditor,
            SceneType::EffectPreview,
            SceneType::EffectShowcase,
            SceneType::PostEffectShowcase,
        };
        const char* modeNames[] = {
            "DebugView",
            "StageEditor",
            "GamePlay",
            "SkinningEditor",
            "EffectPreview",
            "EffectShowcase",
            "PostEffectShowcase"
        };
        int modeIndex = 0;
        const SceneType currentSceneType = GetCurrentSceneType();
        for (int i = 0; i < static_cast<int>(std::size(selectableScenes)); ++i) {
            if (selectableScenes[i] == currentSceneType) {
                modeIndex = i;
                break;
            }
        }
        if (ImGui::Combo("App Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
            if (modeIndex >= 0 && modeIndex < static_cast<int>(std::size(selectableScenes))) {
                RequestSceneChange(selectableScenes[modeIndex]);
            }
        }
        if (currentMode_ == AppMode::EffectPreview) {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Effect only viewport");
            ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);
        } else {

            ImGui::Checkbox("Show 3D Objects",      &debugFlags_.show3DObjects);
            ImGui::Checkbox("Show Terrain",          &debugFlags_.showTerrain);
            ImGui::Checkbox("Show Skybox",           &debugFlags_.showSkybox);
            ImGui::Checkbox("Show Skybox (Cubemap)", &showSkyboxCubemap_);
            if (currentMode_ == AppMode::DebugView) {
                ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
            }
            ImGui::Checkbox("Show Particles",        &debugFlags_.showParticles);
            ImGui::Checkbox("Show Collision Boxes (F3)", &debugFlags_.showCollisionBoxes);
        }
    }


    if (postProcessInitialized_) {
        postProcess_.DrawImGui();
    }

    if (currentMode_ == AppMode::StageEditor &&
        ImGui::CollapsingHeader("External Level", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Load a stage JSON exported by Blender or another external tool.");

        // Resources/Levelsへ保存された外部レベルを毎フレーム列挙する。
        // Blenderで新しいJSONを書き出した直後でも、専用の更新操作なしで一覧へ現れる。
        std::vector<std::filesystem::path> externalLevelFiles;
        std::error_code directoryError;
        const std::filesystem::path externalLevelDirectory = "Resources/Levels";
        if (std::filesystem::exists(externalLevelDirectory, directoryError) && !directoryError) {
            for (const auto& entry :
                 std::filesystem::directory_iterator(externalLevelDirectory, directoryError)) {
                if (directoryError) {
                    break;
                }
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    externalLevelFiles.push_back(entry.path());
                }
            }
            std::sort(externalLevelFiles.begin(), externalLevelFiles.end());
        }

        ImGui::TextUnformatted("Saved External Levels:");
        if (ImGui::BeginListBox("##ExternalLevelList", ImVec2(-FLT_MIN, 90.0f))) {
            for (const std::filesystem::path& levelFile : externalLevelFiles) {
                const std::string levelPath = levelFile.generic_string();
                const bool selected = levelPath == blenderLevelPath_.data();
                if (ImGui::Selectable(levelFile.filename().string().c_str(), selected)) {
                    strncpy_s(
                        blenderLevelPath_.data(),
                        blenderLevelPath_.size(),
                        levelPath.c_str(),
                        _TRUNCATE);

                    // 一覧で別ファイルを選んだだけでは読み込まず、Load操作を待つ。
                    blenderStageActive_ = false;
                    if (player_) {
                        player_->SetExternalCollisionBoxes(nullptr);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }

        const bool pathChanged = ImGui::InputText(
            "Level JSON",
            blenderLevelPath_.data(),
            blenderLevelPath_.size());
        if (pathChanged && blenderStageActive_) {
            // パスを入力しただけでは読み込まない。次のLoad操作まで現在の外部レベルを外す。
            blenderStageActive_ = false;
            if (player_) {
                player_->SetExternalCollisionBoxes(nullptr);
            }
        }

        // エディターを終了せず、指定した外部レベルを現在の編集画面へ読み込む。
        if (ImGui::Button("Load in Editor", ImVec2(-FLT_MIN, 0.0f))) {
            LoadBlenderStage(false);
        }

        // 同じファイルを上書きした後、変更検知を待たずに手動で再読込できる。
        if (ImGui::Button("Reload External Level", ImVec2(-FLT_MIN, 0.0f))) {
            LoadBlenderStage(false);
        }

        // 外部レベルを読み込み、そのまま通常ゲームとして開始する。
        if (ImGui::Button("Play External Level", ImVec2(-FLT_MIN, 0.0f))) {
            LoadBlenderStage(true);
        }

        if (blenderStageActive_) {
            // ネイティブのグリッドステージだけを編集したい場合に外部レベルを取り外す。
            if (ImGui::Button("Hide External Level", ImVec2(-FLT_MIN, 0.0f))) {
                blenderStageActive_ = false;
                if (player_) {
                    player_->SetExternalCollisionBoxes(nullptr);
                    stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
                }
            }
        }

        ImGui::TextWrapped("%s", blenderRuntimeLevel_.GetStatus().c_str());
        ImGui::TextDisabled("External changes are detected after loading.");
    }


    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Use First-Person Camera", &useFirstPersonCamera_);
        if (currentMode_ == AppMode::StageEditor &&
            ImGui::Button("Frame Stage / Player", ImVec2(-FLT_MIN, 0.0f))) {
            const Vector3 focus =
                player_ ? player_->GetPosition() : Vector3{ 8.0f, 1.0f, 8.0f };
            camera->ForceReset(focus, 18.0f, { 0.35f, 0.0f, 0.0f });
        }
        if (useFirstPersonCamera_) {
            ImGui::SliderFloat("FPS Yaw",   &fpsCameraYaw_,   -6.28f, 6.28f);
            ImGui::SliderFloat("FPS Pitch", &fpsCameraPitch_, -1.4f,  1.4f);
        }
        camera->DrawImGui();

        if (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) {
            gameplayCameraController_.SetFov(*camera->GetFovPtr());
        }
    }

    if (isStageToolMode) {
        if (ImGui::CollapsingHeader("StageMap Info")) {
            stageMap_.DrawImGui();
        }
        if (ImGui::CollapsingHeader("Cursor Info")) {
            mapCursor_->DrawImGui();
        }
    }

    ImGui::End(); // 左パネルここまで

    if (isStageToolMode) {
        stageEditorController_.DrawImGui(stageMap_, stageRenderer_.get(), mapCursor_.get(), player_.get());
    }

    if (currentMode_ == AppMode::EffectPreview) {
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - layout.rightPanelWidth, layout.toolbarHeight),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(layout.rightPanelWidth, layout.sidePanelHeight), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::Begin("Effect Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        DrawEffectPreviewEditorImGui();
        ImGui::End();
    }

    // 下部はシーンビューと同じ幅のコンソール／タイムライン／操作パネル。
    ImGui::SetNextWindowPos(
        ImVec2(layout.viewportX, io.DisplaySize.y - layout.bottomPanelHeight),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(layout.viewportWidth, layout.bottomPanelHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::Begin("Tools & Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (currentMode_ == AppMode::SkinningEditor && skinningEditorInitialized_ && skinningEditor_.HasPreviewObject()) {

        if (ImGui::BeginTabBar("SkinningBottomTabs")) {
            if (ImGui::BeginTabItem("Timeline")) {
                skinningEditor_.DrawImGuiTimeline();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Assets")) {
                skinningEditor_.DrawAssetBrowserPanel(player_.get(), models[2].get());
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

    } else if (currentMode_ == AppMode::EffectPreview) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Preview ]");
        ImGui::Text("Effect Type: %s", effectPreviewStormMode_ ? "Tempest Storm" : "Hit Effect");
        ImGui::Text("Editor controls are on the right panel.");
        ImGui::Text("SPACE / H : Trigger Saber Hit");
        ImGui::Text("Saved preset: %s", effectPresetNameBuffer_.data());
        ImGui::Text("GPU Sphere: %s", effectPreviewShowGPUParticleSphere_ ? "ON" : "OFF");
        if (ImGui::Button("Trigger Saber Hit", ImVec2(180, 24)) && particleManager) {
            EmitEffectPreviewBurst();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Particles", ImVec2(140, 24)) && particleManager) {
            particleManager->ClearParticles();
        }

    } else if (false && currentMode_ == AppMode::EffectPreview) {
        ImGui::Columns(2, "EffectPreviewColumns", false);
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Preview ]");
        ImGui::Text("SPACE / H : Trigger Saber Hit");
        ImGui::Checkbox("Auto Trigger", &effectPreviewAutoPlay_);
        ImGui::Checkbox("Show GPU Particle Sphere", &effectPreviewShowGPUParticleSphere_);
        ImGui::SliderFloat("Interval", &effectPreviewInterval_, 0.2f, 3.0f);
        if (ImGui::Button("Trigger Saber Hit", ImVec2(180, 24)) && particleManager) {
            EmitEffectPreviewBurst();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Particles", ImVec2(140, 24)) && particleManager) {
            particleManager->ClearParticles();
        }
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.55f, 0.9f, 1.0f, 1.0f), "[ Hit Effect Tuning ]");
        ImGui::SliderFloat("Size", &effectPreviewHitSettings_.size, 0.2f, 3.0f);
        ImGui::SliderFloat("Brightness", &effectPreviewHitSettings_.brightness, 0.1f, 2.5f);
        ImGui::SliderFloat("Life Scale", &effectPreviewHitSettings_.lifeScale, 0.2f, 3.0f);
        ImGui::SliderFloat("Slash Angle", &effectPreviewHitSettings_.slashAngle, -3.14f, 3.14f);
        ImGui::SliderFloat("Slash Spread", &effectPreviewHitSettings_.slashSpread, 0.2f, 3.14f);
        ImGui::Checkbox("Mirror Slash", &effectPreviewMirrorSlash_);
        ImGui::SliderInt("Burst Count", &effectPreviewBurstCount_, 1, 8);
        ImGui::SliderFloat("Burst Radius", &effectPreviewBurstRadius_, 0.0f, 2.0f);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.35f, 1.0f), "[ Presets ]");
        if (ImGui::Button("Saber Impact", ImVec2(120, 24))) {
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
        ImGui::SameLine();
        if (ImGui::Button("Blue Flash", ImVec2(110, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.brightness = 1.9f;
            effectPreviewHitSettings_.lifeScale = 0.85f;
            effectPreviewHitSettings_.slashSpread = 2.2f;
            effectPreviewHitSettings_.sparkCount = 112;
            effectPreviewHitSettings_.blueRatio = 0.95f;
            effectPreviewHitSettings_.corePower = 1.6f;
            effectPreviewHitSettings_.crossPower = 1.8f;
            effectPreviewHitSettings_.pillarPower = 1.2f;
            effectPreviewHitSettings_.coolColor = { 0.42f, 0.78f, 1.0f, 1.0f };
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        if (ImGui::Button("Spark Burst", ImVec2(120, 24))) {
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
        ImGui::SameLine();
        if (ImGui::Button("Heavy Hit", ImVec2(110, 24))) {
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
        if (ImGui::Button("Cinematic Finisher", ImVec2(240, 28))) {
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
            effectPreviewHitSettings_.coolColor = { 0.38f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.warmColor = { 1.0f, 0.48f, 0.10f, 1.0f };
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 4;
            effectPreviewBurstRadius_ = 0.28f;
            EmitEffectPreviewBurst();
        }
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.50f, 1.0f, 0.55f, 1.0f), "[ Saved Presets ]");
        ImGui::InputText("Preset Name", effectPresetNameBuffer_.data(), effectPresetNameBuffer_.size());
        if (ImGui::Button("Save Preset", ImVec2(120, 24))) {
            SaveEffectPreset(effectPresetNameBuffer_.data());
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh List", ImVec2(120, 24))) {
            LoadEffectPresetNames();
        }

        const char* selectedPresetName = effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())
            ? effectPresetNames_[effectPresetSelectedIndex_].c_str()
            : "Select saved preset";
        if (ImGui::BeginCombo("Saved Presets", selectedPresetName)) {
            for (int i = 0; i < static_cast<int>(effectPresetNames_.size()); ++i) {
                bool selected = effectPresetSelectedIndex_ == i;
                if (ImGui::Selectable(effectPresetNames_[i].c_str(), selected)) {
                    effectPresetSelectedIndex_ = i;
                    CopyPresetName(effectPresetNameBuffer_, effectPresetNames_[i]);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Load Selected", ImVec2(120, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                LoadEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                effectPresetStatus_ = "Preset: nothing selected";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Over", ImVec2(120, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                SaveEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                SaveEffectPreset(effectPresetNameBuffer_.data());
            }
        }
        ImGui::TextUnformatted(effectPresetStatus_.c_str());

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.25f, 1.0f), "[ Spawn Transform ]");
        ImGui::DragFloat3("Position", &effectPreviewPosition_.x, 0.05f, -20.0f, 20.0f);
        if (ImGui::Button("Focus Camera", ImVec2(160, 24))) {
            camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
        }
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "[ Detail ]");
        ImGui::SliderInt("Slash Count", &effectPreviewHitSettings_.slashCount, 3, 32);
        ImGui::SliderInt("Spark Count", &effectPreviewHitSettings_.sparkCount, 0, 160);
        ImGui::SliderFloat("Spark Speed", &effectPreviewHitSettings_.sparkSpeed, 0.1f, 3.0f);
        ImGui::SliderFloat("Spark Length", &effectPreviewHitSettings_.sparkLength, 0.1f, 3.0f);
        ImGui::SliderFloat("Scatter Radius", &effectPreviewHitSettings_.scatterRadius, 0.0f, 3.0f);
        ImGui::SliderFloat("Blue Ratio", &effectPreviewHitSettings_.blueRatio, 0.0f, 1.0f);
        ImGui::SliderFloat("Ring Power", &effectPreviewHitSettings_.ringPower, 0.0f, 3.0f);
        ImGui::SliderFloat("Core Power", &effectPreviewHitSettings_.corePower, 0.0f, 3.0f);
        ImGui::SliderFloat("Cross Power", &effectPreviewHitSettings_.crossPower, 0.0f, 3.0f);
        ImGui::SliderFloat("Pillar Power", &effectPreviewHitSettings_.pillarPower, 0.0f, 3.0f);
        ImGui::ColorEdit3("Cool Color", &effectPreviewHitSettings_.coolColor.x);
        ImGui::ColorEdit3("Warm Color", &effectPreviewHitSettings_.warmColor.x);
        if (ImGui::Button("Reset Tuning", ImVec2(160, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        ImGui::Text("Particles are forced visible in this mode.");
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::GamePlay && player_) {
        ImGui::Columns(2, "GameplayColumns", false);

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Game Controls & Objective ]");
        ImGui::Text("A / D : Move Left / Right");
        ImGui::Text("SPACE : Jump");
        ImGui::Text("B     : Block Inventory");
        ImGui::Text("ESC   : Return to Stage Select");
        ImGui::Separator();
        ImGui::Text("Objective: Pick up bubbles and reach the Goal!");

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Player Debug ]");
        Vector3 playerPosition = player_->GetPosition();
        ImGui::Text("Pos: X:%.2f Y:%.2f Z:%.2f", playerPosition.x, playerPosition.y, playerPosition.z);
        if (isGoalReached_) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "GOAL REACHED!");
        } else {
            ImGui::Text("Status: Playing");
        }
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::StageEditor) {
        ImGui::Columns(2, "EditorColumns", false);

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Stage Editor Controls ]");
        ImGui::Text("W/A/S/D : Cursor Horizontal");
        ImGui::Text("Q/E     : Cursor Up/Down");
        ImGui::Text("ENTER   : Place Block");
        ImGui::Text("SPACE   : Erase Block");
        ImGui::Text("R       : Rotate Block");

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Stage Map Data ]");
        const Int3& cursor = mapCursor_->GetIndex();
        ImGui::Text("Cursor: X:%d Y:%d Z:%d", cursor.x, cursor.y, cursor.z);
        ImGui::Text("Block: %s (ID:%d)",
            BlockTypeToString(stageEditorController_.GetSelectedBlockType()),
            stageEditorController_.GetSelectedBlockType());
        ImGui::Text("Stock: %d", blockInventory_.GetBlockCount());
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::GamePlay_BlockPlace) {
        ImGui::Columns(2, "BlockPlaceColumns", false);

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Block Placement ]");
        ImGui::Text("W/A/S/D : Move Cursor");
        ImGui::Text("Q/E     : Cursor Up/Down");
        ImGui::Text("ENTER   : Place Block");
        ImGui::Text("R       : Rotate Block");
        ImGui::Text("ESC / B : Cancel");

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Placement State ]");
        const Int3& cursor = mapCursor_->GetIndex();
        ImGui::Text("Cursor: X:%d Y:%d Z:%d", cursor.x, cursor.y, cursor.z);
        if (blockInventoryUI_) {
            ImGui::Text("Selected: %s", BlockTypeToString(blockInventoryUI_->GetSelectedBlockType()));
        }
        ImGui::Text("Rotation Y: %.1f deg", placeRotationY_ * 57.29578f);
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::EffectShowcase) {
        const auto& showcaseNames = effectShowcaseController_.GetPresetNames();
        const int presetCount = static_cast<int>(showcaseNames.size());
        const int safeIndex = presetCount > 0
            ? std::clamp(effectShowcaseController_.GetSelectedIndex(), 0, presetCount - 1)
            : 0;
        const char* presetName = presetCount > 0
            ? showcaseNames[safeIndex].c_str()
            : "No showcase presets";

        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Showcase ]");
        ImGui::Text("Preset: %02d / %02d  %s", presetCount > 0 ? safeIndex + 1 : 0, presetCount, presetName);
        ImGui::Text("LEFT / RIGHT : Select    SPACE : Replay    A : Auto Play [%s]    TAB : Game",
            effectShowcaseController_.IsAutoPlayEnabled() ? "ON" : "OFF");
        ImGui::Text("MMB Drag : Orbit    Shift + MMB : Pan    Mouse Wheel : Zoom");

    } else if (currentMode_ == AppMode::PostEffectShowcase) {
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.36f, 1.0f), "[ PostEffect Showcase ]");
        ImGui::Text("Current: %s", GetPostEffectShowcaseName(postProcess_.GetPostEffectMode()));
        ImGui::Text("1 Grayscale / 2 Vignetting / 3 Smoothing / 4-0 Extra PostEffects");
        ImGui::Text("Particle showcase effects are disabled in this mode.");

    } else if (currentMode_ == AppMode::StageSelect) {
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "[ Stage Select ]");
        ImGui::Text("Choose a stage from the center view.");
        ImGui::Text("Only stage selection UI is active in this mode.");

    } else if (currentMode_ == AppMode::DebugView) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Debug View ]");
        ImGui::Text("General object, terrain, sprite, and particle checks.");

        if (ImGui::CollapsingHeader("Player Settings")) {
            ImGui::SliderFloat("Player Glow", &playerGlow_, 0.0f, 5.0f);
            if (player_) {
                player_->SetGlow(playerGlow_);
            }
        }

        if (ImGui::CollapsingHeader("Environment Map", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Debug Objects", &debugObjectEnvironmentCoefficient_, 0.0f, 1.0f);
            ImGui::SliderFloat("Terrain", &terrainEnvironmentCoefficient_, 0.0f, 1.0f);
            ImGui::SliderFloat("Player", &playerEnvironmentCoefficient_, 0.0f, 1.0f);

            for (auto& obj : objectList) {
                if (obj) {
                    obj->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);
                }
            }
            if (terrainObject_) {
                terrainObject_->SetEnvironmentCoefficient(terrainEnvironmentCoefficient_);
            }
            if (player_) {
                player_->SetEnvironmentCoefficient(playerEnvironmentCoefficient_);
            }
        }

    } else {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Application Status ]");
        ImGui::Text("No mode-specific tools.");
    }

    ImGui::End(); // 下パネルここまで

    if (currentMode_ == AppMode::SkinningEditor && skinningEditorInitialized_ && skinningEditor_.HasPreviewObject()) {
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - layout.rightPanelWidth, layout.toolbarHeight),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(layout.rightPanelWidth, layout.sidePanelHeight), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::Begin("Skinning Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        skinningEditor_.DrawImGuiSidePanel(camera.get(), player_.get(), models[2].get());
        ImGui::End();
    }

    const bool hasDedicatedInspector =
        currentMode_ == AppMode::StageEditor ||
        currentMode_ == AppMode::EffectPreview ||
        (currentMode_ == AppMode::SkinningEditor &&
         skinningEditorInitialized_ && skinningEditor_.HasPreviewObject());
    if (!hasDedicatedInspector) {
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - layout.rightPanelWidth, layout.toolbarHeight),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(layout.rightPanelWidth, layout.sidePanelHeight), ImGuiCond_Always);
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "[ Inspector ]");
        ImGui::Text("Mode: %d", static_cast<int>(currentMode_));
        ImGui::Separator();
        ImGui::TextWrapped("Select an editor mode to inspect and edit its properties.");
        ImGui::End();
    }
}
#endif // !NDEBUG

