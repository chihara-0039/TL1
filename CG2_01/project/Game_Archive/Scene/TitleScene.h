#pragma once

#include "../../Game/Scene/BaseScene.h"

// 課題資料のシーン継承構造を示すためのタイトルシーン。
// 現行ゲーム本体では SceneFactory 側のシーン群を使用している。
class TitleScene : public BaseScene {
public:
    void Initialize(GameRuntime& game) override { (void)game; }
    void Update(GameRuntime& game, const SceneUpdateContext& context) override {
        (void)game;
        (void)context;
    }
    void Draw(GameRuntime& game) override { (void)game; }
    void Finalize(GameRuntime& game) override { (void)game; }
};
