#pragma once
#include <Windows.h>
#include <wrl.h>
#include <dinput.h> // DirectInput のヘッダー
#include <Xinput.h>
#include "WinApp.h"

// 入力ライブラリをリンクする。
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

struct MouseState {
    LONG x, y;       // 前フレームからのマウス移動量
    int wheel;       // ホイール回転量
    bool buttons[3]; // 0:左, 1:右, 2:中央

    // ウィンドウクライアント座標上の現在位置。
    LONG posX, posY;
};

struct GamePadState {
    bool connected = false;
    WORD buttons = 0;
    WORD prevButtons = 0;
    float leftStickX = 0.0f;
    float leftStickY = 0.0f;
    float rightStickX = 0.0f;
    float rightStickY = 0.0f;
    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;
};

class Input {
public:
    // COM オブジェクトを安全に扱うための別名。
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    // DirectInput と XInput の入力デバイスを初期化する。
    void Initialize(WinApp* winApp);

    // キーボード、マウス、ゲームパッドの状態を1フレーム分更新する。
    void Update();

    // 指定キーが現在押されているかを返す。
    // keyNumber: DIK_SPACE などの DirectInput キーコード。
    bool PushKey(BYTE keyNumber) const;

    // 指定キーがこのフレームで押された瞬間かを返す。
    bool TriggerKey(BYTE keyNumber) const;

    // 直近のマウス状態を取得する。
    const MouseState& GetMouseState() const { return mouseState_; }

    // 直近のゲームパッド状態を取得する。
    const GamePadState& GetGamePadState() const { return gamePadState_; }

    // Xbox コントローラーが接続されているかを返す。
    bool IsGamePadConnected() const { return gamePadState_.connected; }

    // 指定コントローラーボタンが現在押されているかを返す。
    bool PushControllerButton(WORD button) const;

    // 指定コントローラーボタンがこのフレームで押された瞬間かを返す。
    bool TriggerControllerButton(WORD button) const;

private:
    float NormalizeStickAxis(SHORT value, SHORT deadZone) const;
    float NormalizeTrigger(BYTE value) const;

    WinApp* winApp_ = nullptr;

    ComPtr<IDirectInput8> directInput_;
    ComPtr<IDirectInputDevice8> keyboard_;

    ComPtr<IDirectInputDevice8> mouse_; // マウス用 DirectInput デバイス
    MouseState mouseState_ = {};        // 毎フレーム更新されるマウス入力状態
    GamePadState gamePadState_ = {};    // 毎フレーム更新されるゲームパッド入力状態

    // キーボード入力状態。DirectInput の全キー 256 個を保持する。
    BYTE key_[256] = {};
    BYTE keyPre_[256] = {}; // 1フレーム前のキー状態。トリガー判定に使う。
};
