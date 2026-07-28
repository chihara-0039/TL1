#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <dxcapi.h>
#include <cstdint>
#include <string>
#include <chrono>
#include <unordered_map>
#include "SrvManager.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

class WinApp;

// ==============================================================
//  DirectXCommon
//
//  DirectX 12 の初期化・描画フレームの制御を担当するクラス。
//  エンジン全体で唯一存在する「DirectX の窓口」。
//
//  ─── フレームの流れ ───────────────────────────────────
//   1. BeginImGui()       … ImGui の入力受付開始 (Debug のみ)
//   2. PreDraw()          … バックバッファを RT に切り替え、クリア
//   3. (各オブジェクトが commandList に描画コマンドを積む)
//   4. EndImGui()         … ImGui の描画コマンドを commandList に追加
//   5. PostDraw()         … コマンドを GPU に送信し SwapChain を Present
//
//  ─── 主要 DirectX オブジェクト ────────────────────────
//   Device         : GPU と CPU が通信するための論理デバイス。
//                    バッファ生成・PSO 作成など「準備」全般に使う。
//   CommandList    : GPU への命令書。Draw/Barrier/Clear などを積む。
//   CommandQueue   : 命令書を GPU に投入するキュー。
//   SwapChain      : ダブルバッファリング管理 (表示中↔描画中を交互に切替)。
//   Fence          : GPU 完了を CPU で待つための同期オブジェクト。
//
//  ─── ヒープ (Descriptor Heap) ────────────────────────
//   RTV ヒープ : Render Target View。バックバッファへの書き込み口。
//   DSV ヒープ : Depth Stencil View。深度バッファへの書き込み口。
//   SRV ヒープ : Shader Resource View。テクスチャをシェーダーから読む口。
//              ImGui のフォント SRV もここに入る。
// ==============================================================
class DirectXCommon {
public:
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    DirectXCommon() = default;
    ~DirectXCommon() = default;

    // -------------------------------------------------------
    //  初期化
    //  WinApp が持つウィンドウハンドル (HWND) を元に
    //  Device / SwapChain / 各ヒープ / Fence / DXC を作成する。
    //  呼び出しは必ずアプリ起動時に 1 回だけ。
    // -------------------------------------------------------
    void Initialize(WinApp* winApp);

    // -------------------------------------------------------
    //  GPU の全コマンド処理完了を CPU 側で待機する。
    //  Finalize() の直前に必ず呼ぶこと。
    //  (呼ばないと GPU が使用中のリソースを解放してクラッシュする)
    // -------------------------------------------------------
    void WaitForGpu();

    // -------------------------------------------------------
    //  ImGui のフレーム開始 / 終了  [Debug ビルドのみ使用]
    //  BeginImGui() : ImGui の NewFrame を呼ぶ。Update の先頭で呼ぶ。
    //  EndImGui()   : ImGui の Render を呼び描画コマンドを積む。
    //                 PostDraw() より前に呼ぶこと。
    // -------------------------------------------------------
    void BeginImGui();
    void EndImGui();

    D3D12_GPU_DESCRIPTOR_HANDLE RegisterImGuiTexture(
        ID3D12Resource* textureResource,
        const D3D12_RESOURCE_DESC& resourceDesc);

    // -------------------------------------------------------
    //  描画前処理 (フレームの開始)
    //  - バックバッファのリソースバリアを Present → RenderTarget に遷移
    //  - RTV と DSV をセット
    //  - 画面をクリア (塗りつぶし色は SkyColor)
    //  - ビューポートとシザー矩形をセット
    // -------------------------------------------------------
    void PreDraw(bool clearDepth = true);
    void SetClearColor(float r, float g, float b, float a = 1.0f) {
        clearColor_[0] = r; clearColor_[1] = g; clearColor_[2] = b; clearColor_[3] = a;
    }

    // -------------------------------------------------------
    //  描画後処理 (フレームの終了)
    //  - バックバッファのリソースバリアを RenderTarget → Present に遷移
    //  - CommandList を閉じて CommandQueue に Submit
    //  - SwapChain.Present() で画面に表示
    //  - 次フレームの CommandAllocator をリセット
    //  - FPS 固定のための待機
    // -------------------------------------------------------
    void PostDraw();

    // -------------------------------------------------------
    //  ImGui の終了処理  [アプリ終了時に 1 回だけ呼ぶ]
    //  ImGui_ImplDX12_Shutdown などのクリーンアップを行う。
    // -------------------------------------------------------
    void FinalizeImGui();

    // -------------------------------------------------------
    //  HLSL シェーダーファイルをコンパイルして IDxcBlob を返す。
    //  filePath : シェーダーファイルのパス (例: L"Shaders/Object3d.VS.hlsl")
    //  profile  : シェーダープロファイル (例: L"vs_6_0" / L"ps_6_0")
    //  戻り値   : コンパイル済みシェーダーバイナリ
    //             (PSO の BytecodeLength / pShaderBytecode に渡す)
    // -------------------------------------------------------
    ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile);

    // -------------------------------------------------------
    //  ゲッター
    // -------------------------------------------------------

    /// <summary>GPU リソース生成に使う論理デバイス</summary>
    ID3D12Device* GetDevice() const { return device_.Get(); }

    /// <summary>Draw / Barrier / Clear などの命令を積む CommandList</summary>
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    /// <summary>バックバッファ数 (常に 2: ダブルバッファリング)</summary>
    size_t GetBackBufferCount() const { return 2; }

    /// <summary>深度バッファの書き込み口 (ShadowMap の DSV 生成などに使う)</summary>
    ID3D12DescriptorHeap* GetDsvHeap() const { return dsvDescriptorHeap_.Get(); }
    ID3D12Resource* GetDepthStencilResource() const { return depthStencilResource_.Get(); }

private:
    // -------------------------------------------------------
    //  Initialize() から呼ばれる内部初期化関数群
    //  (各処理を分割して見通しを良くしている)
    // -------------------------------------------------------
    void InitializeDevice();          // DXGI Factory + Device の生成
    void InitializeCommand();         // CommandQueue / CommandAllocator / CommandList
    void InitializeSwapChain();       // ダブルバッファリング用 SwapChain
    void InitializeRenderTargetView(); // バックバッファの RTV を作成
    void InitializeDepthStencilView(); // 深度バッファと DSV を作成
    void InitializeFence();           // GPU-CPU 同期用フェンス
    void InitializeDXC();             // シェーダーコンパイラ (DXC) の初期化
    void InitializeFixFPS();          // FPS 固定用の基準時刻を記録

    /// <summary>フレームの最後に 60FPS になるよう CPU をスリープさせる</summary>
    void UpdateFixFPS();

private:
    WinApp* winApp_ = nullptr;
    float clearColor_[4] = { 0.1f, 0.25f, 0.5f, 1.0f };

    // ── DirectX 主要オブジェクト ──────────────────────────
    ComPtr<IDXGIFactory7>              dxgiFactory_;      // GPU の列挙と SwapChain 生成に使う
    ComPtr<ID3D12Device>               device_;           // 論理 GPU デバイス (最重要)
    ComPtr<ID3D12CommandQueue>         commandQueue_;     // コマンドリストを GPU に投入するキュー
    ComPtr<ID3D12CommandAllocator>     commandAllocator_; // コマンドリストのメモリ確保器
    ComPtr<ID3D12GraphicsCommandList>  commandList_;      // Draw 命令を積むリスト
    ComPtr<IDXGISwapChain4>            swapChain_;        // 表示中↔描画中バッファの切り替え管理

    // ── RTV (Render Target View) ─────────────────────────
    // バックバッファ (2枚) を描画ターゲットとして扱うためのビュー
    ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;    // RTV を格納するヒープ (棚)
    ComPtr<ID3D12Resource>       swapChainResources_[2]; // バックバッファリソース本体

    // ── DSV (Depth Stencil View) ─────────────────────────
    // 深度テスト用。手前にあるオブジェクトが奥のものを隠すために使う。
    ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;    // DSV を格納するヒープ
    ComPtr<ID3D12Resource>       depthStencilResource_; // 深度バッファリソース本体

    // ── SRV (Shader Resource View) ── ImGui 専用 ─────────
    // ImGui のフォントテクスチャを GPU シェーダーから参照するためのヒープ
    SrvManager imguiSrvManager_;
    static constexpr uint32_t kMaxImGuiSrvDescriptors = 256;

    // ── フェンス (GPU-CPU 同期) ───────────────────────────
    // GPU がコマンドを処理し終えたことを CPU に通知するオブジェクト。
    // PostDraw でシグナル、WaitForGpu でウェイトする。
    ComPtr<ID3D12Fence> fence_;
    uint64_t            fenceValue_ = 0;   // GPU に渡す単調増加カウンタ
    HANDLE              fenceEvent_ = nullptr; // CPU 側のイベントハンドル

    // ── DXC (DirectX Shader Compiler) ────────────────────
    // HLSL ファイルをランタイムでコンパイルする。
    // VS2022 付属のコンパイラより新しい機能 (SM 6.x) に対応している。
    ComPtr<IDxcUtils>          dxcUtils_;
    ComPtr<IDxcCompiler3>      dxcCompiler_;
    std::unordered_map<std::wstring, ComPtr<IDxcBlob>> shaderBlobCache_;
    ComPtr<IDxcIncludeHandler> includeHandler_; // #include 解決に使う

    // ── FPS 固定 ─────────────────────────────────────────
    // 高速な PC でも 60fps に抑え、ゲームロジックの速度を均一にする。
    std::chrono::steady_clock::time_point reference_;
};
