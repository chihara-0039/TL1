$content = Get-Content project/Game/Scene/MyGame.cpp -Raw -Encoding UTF8

$content = $content.Replace('titleScene_ = std::make_unique<TitleScene>();', '')
$content = $content.Replace('titleScene_->Initialize(object3dCommon.get(), input.get());', '')
$content = $content.Replace('gameClearScene_ = std::make_unique<GameClearScene>();', '')
$content = $content.Replace('gameClearScene_->Initialize(object3dCommon.get());', '')

$content = $content.Replace('titleBgmData = sound.SoundLoadFile("Resources/Sound/gameTitle.mp3");', '')
$content = $content.Replace('clearBgmData = sound.SoundLoadFile("Resources/Sound/gameClear.mp3");', '')

$content = $content.Replace('currentMode_           = AppMode::Title;', 'currentMode_           = AppMode::StageSelect;')

$content = $content.Replace('clearGuideTexture_ = textureManager->LoadTexture("Resources/UI/clear_guide.png");', '')
$content = $content.Replace('clearGuideSprite_ = std::make_unique<Sprite>();', '')
$content = $content.Replace('clearGuideSprite_->Initialize(spriteCommon.get(), clearGuideTexture_);', '')
$content = $content.Replace('clearGuideSprite_->SetPosition({ 396, 635 });', '')
$content = $content.Replace('clearGuideSprite_->SetSize({ 488, 65 });', '')
$content = $content.Replace('if (clearGuideSprite_) { clearGuideSprite_->Update(); }', '')
$content = $content.Replace('if (currentMode_ == AppMode::GameClear && clearGuideSprite_) {', 'if (false) {')

$content = $content.Replace('if (debugFlags_.showParticles || currentMode_ == AppMode::GameClear) {', 'if (debugFlags_.showParticles) {')

$content = $content.Replace('gameClearScene_.reset();', '')
$content = $content.Replace('titleScene_.reset();', '')

Set-Content project/Game/Scene/MyGame.cpp -Value $content -Encoding UTF8
