import re

with open('project/Game/Scene/MyGame.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Remove initializations
content = re.sub(r'^\s*titleScene_ = std::make_unique<TitleScene>\(\);\s*\n\s*titleScene_->Initialize.*?;\s*\n', '', content, flags=re.MULTILINE)
content = re.sub(r'^\s*gameClearScene_ = std::make_unique<GameClearScene>\(\);\s*\n\s*gameClearScene_->Initialize.*?;\s*\n', '', content, flags=re.MULTILINE)

# Remove bgm loading
content = re.sub(r'^\s*titleBgmData\s*=\s*sound\.SoundLoadFile.*?;\s*\n', '', content, flags=re.MULTILINE)
content = re.sub(r'^\s*clearBgmData\s*=\s*sound\.SoundLoadFile.*?;\s*\n', '', content, flags=re.MULTILINE)

# Remove bgm cases
content = re.sub(r'\s*case BgmType::Title:.*?break;', '', content, flags=re.DOTALL)
content = re.sub(r'\s*case BgmType::Clear:.*?break;', '', content, flags=re.DOTALL)

# Set starting mode to StageSelect (or DebugView)
content = re.sub(r'currentMode_\s*=\s*AppMode::Title;', 'currentMode_ = AppMode::StageSelect;', content)

# Remove clearGuideSprite
content = re.sub(r'^\s*clearGuideTexture_.*?\n', '', content, flags=re.MULTILINE)
content = re.sub(r'^\s*clearGuideSprite_ =.*?\n', '', content, flags=re.MULTILINE)
content = re.sub(r'^\s*clearGuideSprite_->Initialize.*?\n', '', content, flags=re.MULTILINE)
content = re.sub(r'^\s*clearGuideSprite_->SetPosition.*?\n', '', content, flags=re.MULTILINE)
content = re.sub(r'^\s*clearGuideSprite_->SetSize.*?\n', '', content, flags=re.MULTILINE)
content = re.sub(r'^\s*if \(clearGuideSprite_\) \{ clearGuideSprite_->Update\(\); \}\n', '', content, flags=re.MULTILINE)
content = re.sub(r'^\s*if \(currentMode_ == AppMode::GameClear && clearGuideSprite_\).*?clearGuideSprite_->Draw\(\);\s*\}\n', '', content, flags=re.MULTILINE|re.DOTALL)

# Remove GameClear from particle updates
content = re.sub(r'debugFlags_\.showParticles \|\| currentMode_ == AppMode::GameClear', 'debugFlags_.showParticles', content)

# Remove GameClear transition
content = re.sub(r'^\s*if \(isGoalReached_\) \{\s*\n\s*currentMode_ = AppMode::GameClear;\s*\n\s*\}\n', '', content, flags=re.MULTILINE)

# Remove titleScene_.reset() and gameClearScene_.reset()
content = re.sub(r'^\s*gameClearScene_\.reset\(\);\s*\n', '', content, flags=re.MULTILINE)
content = re.sub(r'^\s*titleScene_\.reset\(\);\s*\n', '', content, flags=re.MULTILINE)

# Remove UpdateTitle definition
content = re.sub(r'// ---\s*\n//  UpdateTitle.*?\nvoid MyGame::UpdateTitle\(\) \{.*?\}\n', '', content, flags=re.MULTILINE|re.DOTALL)
content = re.sub(r'// -+\n//  UpdateTitle.*?\nvoid MyGame::UpdateTitle\(\) \{.*?\}\n', '', content, flags=re.MULTILINE|re.DOTALL)

with open('project/Game/Scene/MyGame.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
