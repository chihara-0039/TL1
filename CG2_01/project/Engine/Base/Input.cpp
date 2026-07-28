#include "Input.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

void Input::Initialize(WinApp* winApp) {
    winApp_ = winApp;
    HRESULT resultCode;

    // DirectInput 本体を生成する。キーボードとマウスはこのオブジェクトから作る。
    resultCode = DirectInput8Create(
        winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.GetAddressOf()), nullptr);
    assert(SUCCEEDED(resultCode));

    if (directInput_) {
        // キーボードデバイスを生成する。
        resultCode = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
        assert(SUCCEEDED(resultCode));

        // キーボードの入力データ形式を DirectInput 標準形式に設定する。
        resultCode = keyboard_->SetDataFormat(&c_dfDIKeyboard);
        assert(SUCCEEDED(resultCode));

        // ゲームウィンドウが前面にある間だけ、他アプリと共存する形で入力を受け取る。
        resultCode = keyboard_->SetCooperativeLevel(
            winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
        assert(SUCCEEDED(resultCode));

        // マウスデバイスを生成する。
        resultCode = directInput_->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), nullptr);
        assert(SUCCEEDED(resultCode));

        // マウスの入力データ形式を DirectInput 標準形式に設定する。
        resultCode = mouse_->SetDataFormat(&c_dfDIMouse);
        assert(SUCCEEDED(resultCode));

        // マウスはウィンドウ外で動く場合もあるため、非排他で取得する。
        resultCode = mouse_->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
        assert(SUCCEEDED(resultCode));
    }

    // 初回更新前に前回入力が残らないよう、入力バッファを明示的にクリアする。
    std::memset(key_, 0, sizeof(key_));
    std::memset(keyPre_, 0, sizeof(keyPre_));
}

void Input::Update() {
    HRESULT resultCode;

    // トリガー判定のため、更新前のキーボード/パッド状態を保存する。
    std::memcpy(keyPre_, key_, sizeof(key_));
    gamePadState_.prevButtons = gamePadState_.buttons;

    if (keyboard_) {
        // フォーカス復帰直後は Acquire が必要になることがある。
        resultCode = keyboard_->Acquire();

        resultCode = keyboard_->GetDeviceState(sizeof(key_), key_);
        if (FAILED(resultCode)) {
            keyboard_->Acquire();
            resultCode = keyboard_->GetDeviceState(sizeof(key_), key_);
        }
    } else {
        std::memset(key_, 0, sizeof(key_));
    }

    // マウスの相対移動量、ホイール、ボタン状態を取得する。
    DIMOUSESTATE mouseData{};
    if (mouse_) {
        resultCode = mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseData);
        if (FAILED(resultCode)) {
            mouse_->Acquire();
            mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseData);
        }

        mouseState_.x = mouseData.lX;
        mouseState_.y = mouseData.lY;
        mouseState_.wheel = mouseData.lZ;
        for (int buttonIndex = 0; buttonIndex < 3; buttonIndex++) {
            mouseState_.buttons[buttonIndex] = (mouseData.rgbButtons[buttonIndex] & 0x80) != 0;
        }
    }

    // マウスカーソルの絶対位置はウィンドウのクライアント座標に変換して保持する。
    POINT cursorPosition;
    GetCursorPos(&cursorPosition);
    ScreenToClient(winApp_->GetHwnd(), &cursorPosition);
    mouseState_.posX = cursorPosition.x;
    mouseState_.posY = cursorPosition.y;

    // Xbox コントローラーは XInput で1番スロットを取得する。
    XINPUT_STATE xinputState{};
    DWORD xinputResult = XInputGetState(0, &xinputState);
    gamePadState_.connected = (xinputResult == ERROR_SUCCESS);

    if (gamePadState_.connected) {
        const XINPUT_GAMEPAD& gamePad = xinputState.Gamepad;
        gamePadState_.buttons = gamePad.wButtons;
        gamePadState_.leftStickX = NormalizeStickAxis(gamePad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        gamePadState_.leftStickY = NormalizeStickAxis(gamePad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        gamePadState_.rightStickX = NormalizeStickAxis(gamePad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        gamePadState_.rightStickY = NormalizeStickAxis(gamePad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        gamePadState_.leftTrigger = NormalizeTrigger(gamePad.bLeftTrigger);
        gamePadState_.rightTrigger = NormalizeTrigger(gamePad.bRightTrigger);
    } else {
        // 未接続時は前回値で動き続けないよう、ゲームパッド入力をリセットする。
        gamePadState_.buttons = 0;
        gamePadState_.leftStickX = 0.0f;
        gamePadState_.leftStickY = 0.0f;
        gamePadState_.rightStickX = 0.0f;
        gamePadState_.rightStickY = 0.0f;
        gamePadState_.leftTrigger = 0.0f;
        gamePadState_.rightTrigger = 0.0f;
    }
}

bool Input::PushKey(BYTE keyNumber) const {
    return key_[keyNumber] != 0;
}

bool Input::TriggerKey(BYTE keyNumber) const {
    return key_[keyNumber] != 0 && keyPre_[keyNumber] == 0;
}

bool Input::PushControllerButton(WORD button) const {
    return gamePadState_.connected && (gamePadState_.buttons & button) != 0;
}

bool Input::TriggerControllerButton(WORD button) const {
    return gamePadState_.connected &&
        (gamePadState_.buttons & button) != 0 &&
        (gamePadState_.prevButtons & button) == 0;
}

float Input::NormalizeStickAxis(SHORT value, SHORT deadZone) const {
    const int absoluteValue = std::abs(static_cast<int>(value));
    if (absoluteValue <= deadZone) {
        return 0.0f;
    }

    // デッドゾーンを除いた残りの範囲を -1.0 から 1.0 に正規化する。
    const float sign = value < 0 ? -1.0f : 1.0f;
    const float normalizedValue = static_cast<float>(absoluteValue - deadZone) /
        static_cast<float>(32767 - deadZone);
    return sign * std::clamp(normalizedValue, 0.0f, 1.0f);
}

float Input::NormalizeTrigger(BYTE value) const {
    if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        return 0.0f;
    }

    // トリガー閾値を越えた分だけを 0.0 から 1.0 に正規化する。
    return static_cast<float>(value - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) /
        static_cast<float>(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}
