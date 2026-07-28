#include "GameBgmController.h"

void GameBgmController::Initialize() {
    if (initialized_) {
        return;
    }
    sound_.Initialize();
    gameBgmData_ = sound_.SoundLoadFile("Resources/Sound/gamePlay.mp3");
    state_ = State::Stopped;
    initialized_ = true;
}

void GameBgmController::Finalize() {
    if (!initialized_) {
        return;
    }
    sound_.BGMStop();
    sound_.Finalize();
    state_ = State::Stopped;
    initialized_ = false;
}

void GameBgmController::Update(bool shouldPlayGameBgm) {
    if (!initialized_) {
        return;
    }
    const State nextState = shouldPlayGameBgm ? State::PlayingGame : State::Stopped;
    if (nextState == state_) {
        return;
    }
    sound_.BGMStop();
    state_ = nextState;
    if (state_ == State::PlayingGame) {
        sound_.BGMPlay(gameBgmData_, volume_);
    }
}
