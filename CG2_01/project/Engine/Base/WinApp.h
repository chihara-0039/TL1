#pragma once
#include <Windows.h>
#include <wrl.h>
#include <cstdint>

// Win32ウィンドウの生成、メッセージ処理、破棄を担当するアプリ基盤クラス。
class WinApp {
public:
    // Win32から呼び出されるウィンドウプロシージャ。
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
    // 定数（クライアント領域のサイズ - ゲームの解像度）
    static constexpr int32_t kClientWidth = 1280;
    static constexpr int32_t kClientHeight = 720;

    // ウィンドウ全体のサイズ（エディタレイアウト用）
    static constexpr int32_t kWindowWidth = 1920;
    static constexpr int32_t kWindowHeight = 1080;

    // ウィンドウクラス登録とゲーム用ウィンドウ生成を行う。
    void Initialize();
    // Windowsメッセージを処理する。終了要求を受けた場合はfalseを返す。
    bool ProcessMessage();
    // ウィンドウ破棄とクラス登録解除を行う。
    void Finalize();

    HWND GetHwnd() const { return hwnd_; }
    HINSTANCE GetHInstance() const { return wc_.hInstance; }

    // DirectXCommonやInputが参照するクライアント領域サイズ。
    int32_t GetWidth() const { return kClientWidth; }
    int32_t GetHeight() const { return kClientHeight; }

private:
    // 生成済みウィンドウのハンドル。
    HWND hwnd_ = nullptr;
    // 登録したウィンドウクラス情報。
    WNDCLASSW wc_{};
};
