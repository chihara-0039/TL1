#pragma once

#include <memory>
#include "BaseScene.h"
#include "SceneType.h"
#include "SceneUpdateContext.h"

class GameRuntime;
class SceneFactory;

class SceneManager {
public:
    static SceneManager* GetInstance();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    void Initialize(const SceneFactory* sceneFactory, SceneType initialScene, GameRuntime& game);
    void Update(GameRuntime& game, const SceneUpdateContext& context);
    void Draw(GameRuntime& game);
    void Finalize(GameRuntime& game);
    void ChangeScene(SceneType nextScene, GameRuntime& game);

    SceneType GetCurrentSceneType() const { return currentSceneType_; }

private:
    SceneManager() = default;
    ~SceneManager() = default;

    const SceneFactory* sceneFactory_ = nullptr;
    std::unique_ptr<BaseScene> currentScene_;
    SceneType currentSceneType_ = SceneType::DebugView;
};
