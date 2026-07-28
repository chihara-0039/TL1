$content = Get-Content project/Game/Scene/MyGame.cpp -Raw -Encoding UTF8

# Remove Title and GameClear from switch in UpdateImGui
$content = $content -replace '(?s)\s*case AppMode::Title:.*?break;', ''
$content = $content -replace '(?s)\s*case AppMode::GameClear:.*?break;', ''

# Remove from AppMode string names array (ImGui mode combo)
# Just let it be, AppMode::Title/GameClear cases are gone. Wait!
# In UpdateImGui(), there is:
$content = $content -replace '(?s)\s*if \(currentMode_ == AppMode::Title\) \{.*?ImGui::Text\("Scene: TITLE"\);\}', ''
$content = $content -replace '(?s)\s*else if \(currentMode_ == AppMode::GameClear\) \{.*?ImGui::Text\("Scene: CLEAR"\);\}', ''

# Remove from RenderScene()
$content = $content -replace '(?s)\s*if \(currentMode_ == AppMode::Title\) \{\s*if \(titleScene_\) \{ titleScene_->Draw\(\); \}\s*\}\s*else if', 'if'
$content = $content -replace '(?s)\s*else if \(currentMode_ == AppMode::GameClear\) \{\s*if \(gameClearScene_\) \{ gameClearScene_->Draw\(\); \}\s*\}', ''

# UpdateTitle() might still be defined if the regex failed, wait!
# Line 1263 says 'titleScene_' undefined. That's inside void MyGame::UpdateTitle() !!
# Let's remove UpdateTitle entirely.
$content = $content -replace '(?s)\s*// -+\r?\n//  UpdateTitle.*?\r?\nvoid MyGame::UpdateTitle\(\) \{.*?\}\r?\n', ''
# And if it's there without the comment:
$content = $content -replace '(?s)\s*void MyGame::UpdateTitle\(\) \{.*?\}\r?\n(?=// -+\r?\n//  UpdateStageSelect)', ''

# Line 1318 and 1328 is BgmType::Title and BgmType::Clear cases.
$content = $content -replace '(?s)\s*case BgmType::Title:.*?break;', ''
$content = $content -replace '(?s)\s*case BgmType::Clear:.*?break;', ''

# Remove nextBgmType = BgmType::Title / AppMode::Title / GameClear from UpdateBgm
$content = $content -replace '(?s)\s*case AppMode::Title:\s*\r?\n\s*case AppMode::StageSelect:', 'case AppMode::StageSelect:'
$content = $content -replace '(?s)\s*nextBgmType = BgmType::Title;', 'nextBgmType = BgmType::None;'
$content = $content -replace '(?s)\s*case AppMode::GameClear:\s*\r?\n\s*nextBgmType = BgmType::Clear;\s*\r?\n\s*break;', ''

Set-Content project/Game/Scene/MyGame.cpp -Value $content -Encoding UTF8
