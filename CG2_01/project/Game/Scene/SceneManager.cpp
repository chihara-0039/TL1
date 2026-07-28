#include "SceneManager.h"
#include "BaseScene.h"
#include "SceneFactory.h"

#include <cassert>

SceneManager* SceneManager::GetInstance() {
    static SceneManager instance;
    return &instance;
}

void SceneManager::Initialize(const SceneFactory* sceneFactory, SceneType initialScene, GameRuntime& game) {
    assert(sceneFactory);
    sceneFactory_ = sceneFactory;
    ChangeScene(initialScene, game);
}

void SceneManager::Update(GameRuntime& game, const SceneUpdateContext& context) {
    if (currentScene_) {
        currentScene_->Update(game, context);
    }
}

void SceneManager::Draw(GameRuntime& game) {
    if (currentScene_) {
        currentScene_->Draw(game);
    }
}

void SceneManager::Finalize(GameRuntime& game) {
    if (currentScene_) {
        currentScene_->Finalize(game);
        currentScene_.reset();
    }
}

void SceneManager::ChangeScene(SceneType nextScene, GameRuntime& game) {
    assert(sceneFactory_);

    if (currentScene_ && currentSceneType_ == nextScene) {
        return;
    }

    if (currentScene_) {
        currentScene_->Finalize(game);
        currentScene_.reset();
    }

    currentSceneType_ = nextScene;
    currentScene_ = sceneFactory_->CreateScene(nextScene);
    assert(currentScene_);
    currentScene_->SetSceneManager(this);
    currentScene_->Initialize(game);
}
