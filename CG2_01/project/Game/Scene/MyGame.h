#pragma once

#include <memory>

class GameRuntime;

/// アプリケーションの公開Facade。
///
/// main.cppへゲーム内部の依存を公開せず、ライフサイクル操作だけを提供する。
/// 実際のシーン、描画、入力、エディタ処理はGameRuntime以下へ委譲する。
class MyGame {
public:
    MyGame();
    ~MyGame();

    MyGame(const MyGame&) = delete;
    MyGame& operator=(const MyGame&) = delete;

    /// エンジンとゲームRuntimeを初期化する。
    void Initialize();
    /// 1フレーム分のゲーム処理をRuntimeへ委譲する。
    void Update();
    /// 1フレーム分の描画処理をRuntimeへ委譲する。
    void Draw();
    /// GPU・音声・ウィンドウ資源を安全な順序で終了する。
    void Finalize();
    /// ウィンドウが終了要求を受けるまでtrueを返す。
    bool IsRunning();

private:
    // GameRuntimeの完全型を公開ヘッダーから隠すPimpl所有。
    std::unique_ptr<GameRuntime> runtime_;
};
