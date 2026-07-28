#include "DirectXCommon.h"
#include "WinApp.h"
#include <vector>
#include <cassert>
#include <format>
#include <thread>
#include <stdexcept>
#include <filesystem>
#include <fstream>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

using namespace Microsoft::WRL;

namespace {
std::filesystem::path MakeShaderCachePath(const std::filesystem::path& absolutePath, const wchar_t* profile) {
    const std::wstring profileName = profile ? profile : L"unknown";
    const size_t hash = std::hash<std::wstring>{}(absolutePath.wstring() + L"|" + profileName);
    const std::wstring cacheName =
        absolutePath.stem().wstring() + L"_" + profileName + L"_" + std::format(L"{:016x}", hash) + L".cso";
    return absolutePath.parent_path() / L"Compiled" / cacheName;
}

bool IsShaderCacheFresh(const std::filesystem::path& sourcePath, const std::filesystem::path& cachePath) {
    std::error_code ec;
    if (!std::filesystem::exists(cachePath, ec)) {
        return false;
    }

    const auto sourceTime = std::filesystem::last_write_time(sourcePath, ec);
    if (ec) {
        return false;
    }

    const auto cacheTime = std::filesystem::last_write_time(cachePath, ec);
    if (ec) {
        return false;
    }

    return cacheTime >= sourceTime;
}

ComPtr<IDxcBlob> LoadCachedShaderBlob(IDxcUtils* dxcUtils, const std::filesystem::path& cachePath) {
    ComPtr<IDxcBlobEncoding> cachedEncoding;
    HRESULT hr = dxcUtils->LoadFile(cachePath.wstring().c_str(), nullptr, &cachedEncoding);
    if (FAILED(hr)) {
        return nullptr;
    }

    ComPtr<IDxcBlob> cachedBlob;
    cachedEncoding.As(&cachedBlob);
    return cachedBlob;
}

void SaveCachedShaderBlob(const std::filesystem::path& cachePath, IDxcBlob* shaderBlob) {
    if (!shaderBlob) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(cachePath.parent_path(), ec);
    if (ec) {
        return;
    }

    std::ofstream file(cachePath, std::ios::binary);
    if (!file) {
        return;
    }

    file.write(
        static_cast<const char*>(shaderBlob->GetBufferPointer()),
        static_cast<std::streamsize>(shaderBlob->GetBufferSize()));
}
}

void DirectXCommon::Initialize(WinApp* winApp) {
    assert(winApp);
    winApp_ = winApp;

    InitializeFixFPS();
    InitializeDevice();
    InitializeCommand();
    InitializeSwapChain();
    InitializeRenderTargetView();
    InitializeDepthStencilView();
    InitializeFence();
    InitializeDXC();

#ifdef USE_IMGUI

    // --- ImGuiの初期化 ---
    // 1. Context作成
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // ImGuiの組み込みフォントは日本語グリフを持たないため、日本語UIが「?」になる。
    // Windows標準のメイリオを日本語グリフ範囲付きで読み込み、ダイアログやエディタを
    // UTF-8の日本語で表示できるようにする。
    ImGuiIO& imguiIO = ImGui::GetIO();
    ImFont* japaneseFont = imguiIO.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/meiryo.ttc",
        16.0f,
        nullptr,
        imguiIO.Fonts->GetGlyphRangesJapanese());

    // Windows側でフォントが削除・変更されていても、ImGui自体は起動できるようにする。
    if (japaneseFont != nullptr) {
        imguiIO.FontDefault = japaneseFont;
    } else {
        imguiIO.Fonts->AddFontDefault();
    }

    // 2. Win32バックエンドの初期化
    ImGui_ImplWin32_Init(winApp_->GetHwnd());

    // 3. DX12用SRVヒープの作成 (ImGuiのフォントテクスチャ用)
    imguiSrvManager_.Initialize(this, kMaxImGuiSrvDescriptors);
    const uint32_t fontSrvIndex = imguiSrvManager_.Allocate();
    assert(fontSrvIndex == 0);

    // 4. DX12バックエンドの初期化
    ImGui_ImplDX12_Init(
        device_.Get(),
        2, // バックバッファ数
        DXGI_FORMAT_R8G8B8A8_UNORM, // swapChainDesc.Formatと合わせる
        imguiSrvManager_.GetDescriptorHeap(),
        imguiSrvManager_.GetCPUDescriptorHandle(fontSrvIndex),
        imguiSrvManager_.GetGPUDescriptorHandle(fontSrvIndex)
    );

#endif
}

void DirectXCommon::InitializeDevice() {
    // DXGIファクトリーの生成
#ifdef ENABLE_D3D12_DEBUG_LAYER
    ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        // GPU Based Validationは起動時のPSO作成や実行時検証がかなり重い。
        // 必要な調査時だけTRUEに戻す。
        debugController->SetEnableGPUBasedValidation(FALSE);
    }
#endif

    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    // アダプターの列挙とデバイス生成
    ComPtr<IDXGIAdapter4> useAdapter = nullptr;
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        if (FAILED(hr)) continue;

        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            // D3D12デバイス生成を試みる
            hr = D3D12CreateDevice(useAdapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&device_));
            if (SUCCEEDED(hr)) {
                break;
            }
        }
        useAdapter = nullptr;
    }
    assert(device_ != nullptr);

#ifdef USE_IMGUI
    // デバッグ時のエラー停止設定
    ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true); 
    }
#endif
}

void DirectXCommon::InitializeCommand() {
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    HRESULT hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeSwapChain() {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = WinApp::kWindowWidth;
    swapChainDesc.Height = WinApp::kWindowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    //swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        winApp_->GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeRenderTargetView() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 2;
    HRESULT hr = device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap_));
    assert(SUCCEEDED(hr));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    UINT handleSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (UINT i = 0; i < 2; ++i) {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        assert(SUCCEEDED(hr));
        device_->CreateRenderTargetView(swapChainResources_[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += handleSize;
    }
}

void DirectXCommon::InitializeDepthStencilView() {
    // DSV用ヒープ生成
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvDescriptorHeap_));
    assert(SUCCEEDED(hr));

    // 深度リソース生成
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = WinApp::kWindowWidth;
    resourceDesc.Height = WinApp::kWindowHeight;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;

    hr = device_->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&depthStencilResource_));
    assert(SUCCEEDED(hr));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
}

void DirectXCommon::InitializeFence() {
    HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

void DirectXCommon::InitializeDXC() {
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));
}

void DirectXCommon::WaitForGpu() {
    // 1. フェンス値を更新
    fenceValue_++;
    // 2. GPUに「ここまで実行したらフェンス値をこの値にしてね」という信号を送る
    commandQueue_->Signal(fence_.Get(), fenceValue_);

    // 3. GPUがまだその値に達していなければ待機する
    if (fence_->GetCompletedValue() < fenceValue_) {
        // フェンスが指定値に達したらイベントを発行するように設定
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        // イベントが発生する（GPUが完了する）までCPUを止める
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile) {
    // 1. 絶対パスに事前変換する（これで相対インクルード解決が完全に保証されます）
    std::filesystem::path absolutePath = std::filesystem::absolute(filePath);
    std::wstring absFilePath = absolutePath.wstring();
    std::wstring absDirectoryPath = absolutePath.parent_path().wstring();
    const std::wstring cacheKey = absFilePath + L"|" + (profile ? profile : L"");
    if (auto it = shaderBlobCache_.find(cacheKey); it != shaderBlobCache_.end()) {
        return it->second;
    }

    const std::filesystem::path cachePath = MakeShaderCachePath(absolutePath, profile);
    if (IsShaderCacheFresh(absolutePath, cachePath)) {
        if (ComPtr<IDxcBlob> cachedBlob = LoadCachedShaderBlob(dxcUtils_.Get(), cachePath)) {
            shaderBlobCache_[cacheKey] = cachedBlob;
            return cachedBlob;
        }
    }

    // --- デバッグログ：何を読み込もうとしているか出力 ---
    OutputDebugStringW(L"----------------------------------------\n");
    OutputDebugStringW(L"Begin CompileShader (Absolute): ");
    OutputDebugStringW(absFilePath.c_str());
    OutputDebugStringW(L"\n");

    // 2. hlslファイルを読む
    ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
    HRESULT hr = dxcUtils_->LoadFile(absFilePath.c_str(), nullptr, &shaderSource);

    // ★ここが最重要：ファイルが見つからなかったら例外を投げる
    if (FAILED(hr)) {
        OutputDebugStringA("ERROR: Failed to load shader file.\n");
        OutputDebugStringA("Please check if the file path is correct and the file exists.\n");
        OutputDebugStringW(absFilePath.c_str()); // 失敗したパスを表示
        OutputDebugStringA("\n----------------------------------------\n");
        // 警告回避：一文字ずつ明示的に char にキャストして変換する
        std::string pathStr;
        for (wchar_t w : absFilePath) {
            pathStr += static_cast<char>(w);
        }

        throw std::runtime_error("Shader File Not Found: " + pathStr);
    }

    // 3. コンパイル引数準備
    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    // 様々な表記法に対応するため、-I 展開形と結合形、相対/絶対の全てをインクルードパスに登録
    std::wstring includeOptionCombined = L"-I" + absDirectoryPath;
    std::wstring includeOptionCombinedRel = L"-I" + filePath.substr(0, filePath.find_last_of(L"/\\"));

    std::vector<LPCWSTR> compileArgs = {
        absFilePath.c_str(),
        L"-E", L"main",
        L"-T", profile,
        L"-Zi", L"-Qembed_debug",
        L"-Od",
        L"-Zpr",
        L"-I", absDirectoryPath.c_str(),     // -I 絶対パス (分離)
        includeOptionCombined.c_str(),        // -I絶対パス (結合)
        includeOptionCombinedRel.c_str(),     // -I相対パス (結合)
    };

    // 4. コンパイル実行
    ComPtr<IDxcResult> shaderResult = nullptr;
    hr = dxcCompiler_->Compile(
        &shaderSourceBuffer,
        compileArgs.data(),
        static_cast<UINT32>(compileArgs.size()),
        includeHandler_.Get(),
        IID_PPV_ARGS(&shaderResult));

    // コンパイル自体の失敗チェック
    if (FAILED(hr)) {
        OutputDebugStringA("ERROR: DxcCompiler::Compile failed completely.\n");
        throw std::runtime_error("DxcCompiler::Compile failed");
    }

    // 4. エラーメッセージ取得
    ComPtr<IDxcBlobUtf8> shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        std::string errorMsg = shaderError->GetStringPointer();
        OutputDebugStringA("----------------------------------------\n");
        OutputDebugStringA("HLSL Compile Error:\n");
        OutputDebugStringA(errorMsg.c_str());
        OutputDebugStringA("----------------------------------------\n");
        throw std::runtime_error(std::string("HLSL Compile Error: ") + errorMsg);
    }

    // 5. 結果の取得
    ComPtr<IDxcBlob> shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    OutputDebugStringA("CompileShader Success!\n");
    OutputDebugStringA("----------------------------------------\n");

    SaveCachedShaderBlob(cachePath, shaderBlob.Get());
    shaderBlobCache_[cacheKey] = shaderBlob;
    return shaderBlob;
}

void DirectXCommon::PreDraw(bool clearDepth) {
    // バックバッファのインデックス取得
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    // リソースバリア（Present -> RenderTarget）
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    // RTV / DSV セット
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += (backBufferIndex * device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    if (clearDepth) {
        commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
    } else {
        commandList_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    }

    // クリア処理
    commandList_->ClearRenderTargetView(rtvHandle, clearColor_, 0, nullptr);
    if (clearDepth) {
        commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

#ifdef NDEBUG
    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(WinApp::kWindowWidth);
    viewport.Height = static_cast<float>(WinApp::kWindowHeight);
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList_->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = WinApp::kWindowWidth;
    scissorRect.bottom = WinApp::kWindowHeight;
    commandList_->RSSetScissorRects(1, &scissorRect);
#else
    // Unity風ワークスペース:
    // 上Toolbar、左Hierarchy、中央Scene、右Inspector、下Console。
    constexpr float hierarchyWidth =
        static_cast<float>(WinApp::kWindowWidth) * 0.15f;
    constexpr float inspectorWidth =
        static_cast<float>(WinApp::kWindowWidth) * 0.20f;
    constexpr float toolbarHeight = 38.0f;
    constexpr float consoleHeight =
        static_cast<float>(WinApp::kWindowHeight) * 0.32f;
    constexpr float editorViewportWidth =
        static_cast<float>(WinApp::kWindowWidth) - hierarchyWidth - inspectorWidth;
    constexpr float editorViewportHeight =
        static_cast<float>(WinApp::kWindowHeight) - toolbarHeight - consoleHeight;

    D3D12_VIEWPORT viewport{};
    viewport.Width = editorViewportWidth;
    viewport.Height = editorViewportHeight;
    viewport.TopLeftX = hierarchyWidth;
    viewport.TopLeftY = toolbarHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList_->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect{};
    scissorRect.left = static_cast<LONG>(hierarchyWidth);
    scissorRect.top = static_cast<LONG>(toolbarHeight);
    scissorRect.right =
        static_cast<LONG>(hierarchyWidth + editorViewportWidth);
    scissorRect.bottom =
        static_cast<LONG>(toolbarHeight + editorViewportHeight);
    commandList_->RSSetScissorRects(1, &scissorRect);
#endif
}

// 描画の終了
void DirectXCommon::PostDraw() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    // リソースバリア（RenderTarget -> Present）
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    // コマンドクローズ & 実行
    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

	// コマンドリストをGPUに渡す
    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);

    // フリップ
    swapChain_->Present(1, 0);

    // フェンスでGPU完了待ち
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);

    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // 次フレーム用にコマンドリセット
    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));

    UpdateFixFPS();
}

void DirectXCommon::InitializeFixFPS() {
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS() {
    using namespace std::chrono;
    const microseconds kMinTime(static_cast<int64_t>(1000000.0f / 60.0f));
    const microseconds kMinCheckTime(static_cast<int64_t>(1000000.0f / 65.0f));

    steady_clock::time_point now = steady_clock::now();
    microseconds elapsed = duration_cast<microseconds>(now - reference_);

    if (elapsed < kMinCheckTime) {
        while (steady_clock::now() - reference_ < kMinTime) {
            std::this_thread::sleep_for(microseconds(1));
        }
    }
    reference_ = steady_clock::now();
}

// 描画の準備
void DirectXCommon::BeginImGui() {

#ifdef USE_IMGUI
    // If ImGui context is not created, skip calling backend NewFrame
    if (ImGui::GetCurrentContext() == nullptr) return;
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    
    ImGuiIO& io = ImGui::GetIO();
    
    // スワップチェーンのサイズ（1920x1080）と実際のウィンドウサイズが異なる場合（最大化など）、
    // ImGuiの内部解像度がズレるのを防ぐため、強制的に1920x1080の仮想解像度に固定する
    RECT rect;
    GetClientRect(winApp_->GetHwnd(), &rect);
    float clientW = static_cast<float>(rect.right - rect.left);
    float clientH = static_cast<float>(rect.bottom - rect.top);

    if (clientW > 0.0f && clientH > 0.0f) {
        io.DisplaySize = ImVec2(static_cast<float>(WinApp::kWindowWidth), static_cast<float>(WinApp::kWindowHeight));

        // POINTで生のマウス座標を毎回再取得し、ウィンドウサイズに対する比率から仮想解像度にスケールする
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(winApp_->GetHwnd(), &p);
        io.MousePos.x = static_cast<float>(p.x) * (static_cast<float>(WinApp::kWindowWidth) / clientW);
        io.MousePos.y = static_cast<float>(p.y) * (static_cast<float>(WinApp::kWindowHeight) / clientH);
    }

    ImGui::NewFrame();
#endif

}

// 描画の実行 (PostDrawの直前などで呼ぶ)
void DirectXCommon::EndImGui() {

#ifdef USE_IMGUI
    // If ImGui context is not created, skip
    if (ImGui::GetCurrentContext() == nullptr) return;

    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr) return;

    // コマンドリストにImGuiの描画コマンドを積む前に、
    // ゲーム描画用に小さくしたビューポート（1280x720）をフル画面（1920x1080）に戻す！
    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(WinApp::kWindowWidth);
    viewport.Height = static_cast<float>(WinApp::kWindowHeight);
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList_->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = WinApp::kWindowWidth;
    scissorRect.bottom = WinApp::kWindowHeight;
    commandList_->RSSetScissorRects(1, &scissorRect);

    ID3D12DescriptorHeap* heaps[] = { imguiSrvManager_.GetDescriptorHeap() };
    commandList_->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(draw_data, commandList_.Get());
#endif
}

// DirectXCommon.h に Finalize() を追加するか、デストラクタで実行
D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::RegisterImGuiTexture(
    ID3D12Resource* textureResource,
    const D3D12_RESOURCE_DESC& resourceDesc) {
#ifdef USE_IMGUI
    if (!textureResource || imguiSrvManager_.GetUseCount() >= imguiSrvManager_.GetMaxCount()) {
        return {};
    }

    const uint32_t srvIndex = imguiSrvManager_.Allocate();
    D3D12_CPU_DESCRIPTOR_HANDLE destinationCPU = imguiSrvManager_.GetCPUDescriptorHandle(srvIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE destinationGPU = imguiSrvManager_.GetGPUDescriptorHandle(srvIndex);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = resourceDesc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
        resourceDesc.DepthOrArraySize == 6) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = resourceDesc.MipLevels;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    }

    device_->CreateShaderResourceView(textureResource, &srvDesc, destinationCPU);

    return destinationGPU;
#else
    (void)textureResource;
    (void)resourceDesc;
    return {};
#endif
}

void DirectXCommon::FinalizeImGui() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
