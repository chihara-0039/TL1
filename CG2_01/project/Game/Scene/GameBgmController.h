#pragma once

#include "Sound.h"

/// ゲーム中BGMの読み込み、再生切り替え、破棄を一括管理する。
/// GameRuntimeは再生条件だけを通知し、音源の所有権を持たない。
class GameBgmController {
public:
    void Initialize();
    void Finalize();
    void Update(bool shouldPlayGameBgm);

private:
    enum class State { Stopped, PlayingGame };

    Sound sound_;
    Sound::SoundData gameBgmData_{};
    State state_ = State::Stopped;
    float volume_ = 0.5f;
    bool initialized_ = false;
};
