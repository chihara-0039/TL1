#pragma once

#include "SceneUpdateContext.h"

class GameRuntime;
class SceneManager;

class BaseScene {
public:
    virtual ~BaseScene() = default;

    void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

    virtual void Initialize(GameRuntime& game) = 0;
    virtual void Update(GameRuntime& game, const SceneUpdateContext& context) = 0;
    virtual void Draw(GameRuntime& game) = 0;
    virtual void Finalize(GameRuntime& game) = 0;

protected:
    SceneManager* sceneManager_ = nullptr;
};
