#include "MyGame.h"
#include "D3DResourceLeakChecker.h"
#include <Windows.h>
#include <exception>
#include <memory>

namespace {

class GameFinalizeGuard {
public:
    explicit GameFinalizeGuard(MyGame* game) : game_(game) {}

    ~GameFinalizeGuard() {
        if (game_ && active_) {
            game_->Finalize();
        }
    }

    void Dismiss() { active_ = false; }

private:
    MyGame* game_ = nullptr;
    bool active_ = true;
};

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    D3DResourceLeakChecker leakChecker;
    std::unique_ptr<MyGame> game = std::make_unique<MyGame>();

    try {
        game->Initialize();
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Initialization Error", MB_ICONERROR | MB_OK);
        return -1;
    } catch (...) {
        MessageBoxA(nullptr, "Unknown error during initialization.", "Initialization Error", MB_ICONERROR | MB_OK);
        return -1;
    }

    GameFinalizeGuard finalizeGuard(game.get());

    try {
        while (game->IsRunning()) {
            game->Update();
            game->Draw();
        }
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Runtime Error", MB_ICONERROR | MB_OK);
        return -1;
    } catch (...) {
        MessageBoxA(nullptr, "Unknown error during runtime.", "Runtime Error", MB_ICONERROR | MB_OK);
        return -1;
    }

    game->Finalize();
    finalizeGuard.Dismiss();
    return 0;
}
