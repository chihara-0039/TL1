// MyGame.cpp
// アプリケーション入口用の薄いFacade。ゲーム固有処理はGameRuntimeへ委譲する。
#include "MyGame.h"

#include "GameRuntime.h"

MyGame::MyGame() = default;
MyGame::~MyGame() = default;

void MyGame::Initialize() {
    runtime_ = std::make_unique<GameRuntime>();
    runtime_->Initialize();
}

void MyGame::Update() {
    runtime_->Update();
}

void MyGame::Draw() {
    runtime_->Draw();
}

void MyGame::Finalize() {
    if (runtime_) {
        runtime_->Finalize();
        runtime_.reset();
    }
}

bool MyGame::IsRunning() {
    return runtime_ && runtime_->IsRunning();
}
