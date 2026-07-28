$content = Get-Content project/Game/Scene/MyGame.cpp -Raw -Encoding UTF8

$oldText = "        ImGui::Text(`"Scene: DEBUG VIEW`");"
$newText = "        ImGui::Text(`"Scene: DEBUG VIEW`");

        // --- Weather & Environment ---
        if (ImGui::CollapsingHeader("Weather / Environment")) {
            auto& wpMgr = WeatherPresetManager::GetInstance();
            auto& presets = wpMgr.GetPresets();
            if (presets.empty()) {
                ImGui::Text("No presets available.");
            } else {
                std::string currentPresetName = stageMap_.GetWeatherPresetName();
                if (currentPresetName.empty()) {
                    currentPresetName = presets[0].name;
                    stageMap_.SetWeatherPresetName(currentPresetName);
                }
                if (ImGui::BeginCombo("Preset", currentPresetName.c_str())) {
                    for (const auto& p : presets) {
                        bool isSelected = (currentPresetName == p.name);
                        if (ImGui::Selectable(p.name.c_str(), isSelected)) {
                            stageMap_.SetWeatherPresetName(p.name);
                            stageMap_.SetClearColor(p.clearColor);
                            stageMap_.SetLightIntensity(p.lightIntensity);
                            stageMap_.SetLightColor(p.lightColor);
                            stageMap_.SetLightDirection(p.lightDirection);
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                WeatherPreset* currentPreset = wpMgr.GetPresetByName(currentPresetName);
                if (currentPreset) {
                    ImGui::Separator();
                    ImGui::Text("Lighting Settings");
                    ImGui::ColorEdit3("Light Color", &currentPreset->lightColor.x);
                    ImGui::SliderFloat("Light Intensity", &currentPreset->lightIntensity, 0.0f, 5.0f);
                    ImGui::SliderFloat3("Light Direction", &currentPreset->lightDirection.x, -1.0f, 1.0f);
                    ImGui::ColorEdit4("Clear Color", &currentPreset->clearColor.x);
                    ImGui::Separator();
                    if (ImGui::Button("Save as New Preset")) {
                        WeatherPreset newPreset = *currentPreset;
                        newPreset.name = currentPreset->name + " (Copy)";
                        presets.push_back(newPreset);
                        wpMgr.SavePresets();
                        stageMap_.SetWeatherPresetName(newPreset.name);
                    }
                    stageMap_.SetClearColor(currentPreset->clearColor);
                    stageMap_.SetLightIntensity(currentPreset->lightIntensity);
                    stageMap_.SetLightColor(currentPreset->lightColor);
                    stageMap_.SetLightDirection(currentPreset->lightDirection);
                }
            }
        }
        
        // --- Player Options ---
        if (ImGui::CollapsingHeader("Player Settings")) {
            ImGui::SliderFloat("Player Glow", &playerGlow_, 0.0f, 5.0f);
            if (player_) player_->SetGlow(playerGlow_);
        }"

$content = $content.Replace($oldText, $newText)
Set-Content project/Game/Scene/MyGame.cpp -Value $content -Encoding UTF8
