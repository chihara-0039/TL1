$content = Get-Content project/Game/Scene/MyGame.cpp -Raw -Encoding UTF8

$content = $content -replace '(?m)^\s*titleScene_ = std::make_unique<TitleScene>\(\);\s*\r?\n\s*titleScene_->Initialize.*?;\s*\r?\n', ''
$content = $content -replace '(?m)^\s*gameClearScene_ = std::make_unique<GameClearScene>\(\);\s*\r?\n\s*gameClearScene_->Initialize.*?;\s*\r?\n', ''

$content = $content -replace '(?m)^\s*titleBgmData\s*=\s*sound\.SoundLoadFile.*?;\s*\r?\n', ''
$content = $content -replace '(?m)^\s*clearBgmData\s*=\s*sound\.SoundLoadFile.*?;\s*\r?\n', ''

$content = $content -replace '(?s)\s*case BgmType::Title:.*?break;', ''
$content = $content -replace '(?s)\s*case BgmType::Clear:.*?break;', ''

$content = $content -replace 'currentMode_\s*=\s*AppMode::Title;', 'currentMode_ = AppMode::StageSelect;'

$content = $content -replace '(?m)^\s*clearGuideTexture_.*?\r?\n', ''
$content = $content -replace '(?m)^\s*clearGuideSprite_ =.*?\r?\n', ''
$content = $content -replace '(?m)^\s*clearGuideSprite_->Initialize.*?\r?\n', ''
$content = $content -replace '(?m)^\s*clearGuideSprite_->SetPosition.*?\r?\n', ''
$content = $content -replace '(?m)^\s*clearGuideSprite_->SetSize.*?\r?\n', ''
$content = $content -replace '(?m)^\s*if \(clearGuideSprite_\) \{ clearGuideSprite_->Update\(\); \}\r?\n', ''
$content = $content -replace '(?s)\s*if \(currentMode_ == AppMode::GameClear && clearGuideSprite_\).*?clearGuideSprite_->Draw\(\);\s*\}\r?\n', ''

$content = $content -replace 'debugFlags_\.showParticles \|\| currentMode_ == AppMode::GameClear', 'debugFlags_.showParticles'

$content = $content -replace '(?s)\s*if \(isGoalReached_\) \{\s*\r?\n\s*currentMode_ = AppMode::GameClear;\s*\r?\n\s*\}\r?\n', ''

$content = $content -replace '(?m)^\s*gameClearScene_\.reset\(\);\s*\r?\n', ''
$content = $content -replace '(?m)^\s*titleScene_\.reset\(\);\s*\r?\n', ''

$content = $content -replace '(?s)\s*// -+\r?\n//  UpdateTitle.*?\r?\nvoid MyGame::UpdateTitle\(\) \{.*?\}\r?\n', ''

Set-Content project/Game/Scene/MyGame.cpp -Value $content -Encoding UTF8
