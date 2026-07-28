#include "SceneFactory.h"

#include "BaseScene.h"
#include "GameRuntime.h"

#include <cassert>

namespace {
class StageSelectScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { game.OnSceneEntered(SceneType::StageSelect); }
    void Update(GameRuntime& game, const SceneUpdateContext&) override { game.RunStageSelectScene(); }
    void Draw(GameRuntime&) override {}
    void Finalize(GameRuntime& game) override { game.OnSceneExited(SceneType::StageSelect); }
};

class DebugViewScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { game.OnSceneEntered(SceneType::DebugView); }
    void Update(GameRuntime& game, const SceneUpdateContext&) override { game.RunDebugViewScene(); }
    void Draw(GameRuntime&) override {}
    void Finalize(GameRuntime& game) override { game.OnSceneExited(SceneType::DebugView); }
};

class StageEditorScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { game.OnSceneEntered(SceneType::StageEditor); }
    void Update(GameRuntime& game, const SceneUpdateContext&) override { game.RunStageEditorScene(); }
    void Draw(GameRuntime&) override {}
    void Finalize(GameRuntime& game) override { game.OnSceneExited(SceneType::StageEditor); }
};

class GamePlayScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { game.OnSceneEntered(SceneType::GamePlay); }
    void Update(GameRuntime& game, const SceneUpdateContext&) override { game.RunGamePlayScene(); }
    void Draw(GameRuntime&) override {}
    void Finalize(GameRuntime& game) override { game.OnSceneExited(SceneType::GamePlay); }
};

class GamePlayBlockPlaceScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { game.OnSceneEntered(SceneType::GamePlayBlockPlace); }
    void Update(GameRuntime& game, const SceneUpdateContext&) override { game.RunGamePlayBlockPlaceScene(); }
    void Draw(GameRuntime&) override {}
    void Finalize(GameRuntime& game) override { game.OnSceneExited(SceneType::GamePlayBlockPlace); }
};

class SkinningEditorScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { game.OnSceneEntered(SceneType::SkinningEditor); }
    void Update(GameRuntime& game, const SceneUpdateContext& context) override { game.RunSkinningEditorScene(context); }
    void Draw(GameRuntime&) override {}
    void Finalize(GameRuntime& game) override { game.OnSceneExited(SceneType::SkinningEditor); }
};

class EffectPreviewScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { game.OnSceneEntered(SceneType::EffectPreview); }
    void Update(GameRuntime& game, const SceneUpdateContext&) override { game.RunEffectPreviewScene(); }
    void Draw(GameRuntime&) override {}
    void Finalize(GameRuntime& game) override { game.OnSceneExited(SceneType::EffectPreview); }
};

class EffectShowcaseScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { game.OnSceneEntered(SceneType::EffectShowcase); }
    void Update(GameRuntime& game, const SceneUpdateContext&) override { game.RunEffectShowcaseScene(); }
    void Draw(GameRuntime&) override {}
    void Finalize(GameRuntime& game) override { game.OnSceneExited(SceneType::EffectShowcase); }
};

class PostEffectShowcaseScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { game.OnSceneEntered(SceneType::PostEffectShowcase); }
    void Update(GameRuntime& game, const SceneUpdateContext&) override { game.RunPostEffectShowcaseScene(); }
    void Draw(GameRuntime&) override {}
    void Finalize(GameRuntime& game) override { game.OnSceneExited(SceneType::PostEffectShowcase); }
};
}

std::unique_ptr<BaseScene> SceneFactory::CreateScene(SceneType type) const {
    switch (type) {
    case SceneType::StageSelect:
        return std::make_unique<StageSelectScene>();
    case SceneType::DebugView:
        return std::make_unique<DebugViewScene>();
    case SceneType::StageEditor:
        return std::make_unique<StageEditorScene>();
    case SceneType::GamePlay:
        return std::make_unique<GamePlayScene>();
    case SceneType::GamePlayBlockPlace:
        return std::make_unique<GamePlayBlockPlaceScene>();
    case SceneType::SkinningEditor:
        return std::make_unique<SkinningEditorScene>();
    case SceneType::EffectPreview:
        return std::make_unique<EffectPreviewScene>();
    case SceneType::EffectShowcase:
        return std::make_unique<EffectShowcaseScene>();
    case SceneType::PostEffectShowcase:
        return std::make_unique<PostEffectShowcaseScene>();
    }

    assert(false && "Unknown SceneType.");
    return std::make_unique<DebugViewScene>();
}
