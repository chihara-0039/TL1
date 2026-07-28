#include "PostProcessRenderer.h"
#include "externals/imgui/imgui.h"
#include <DirectXTex.h>
#include <cassert>

// ==========================================================
//  PostProcessRenderer::Initialize
//  全リソース (RenderTexture / RTV / SRV / RS / PSO 群 / 定数バッファ) を生成する
// ==========================================================
void PostProcessRenderer::Initialize(DirectXCommon* dxCommon, const Vector4& clearColor) {
    clearColor_ = clearColor;
    auto device = dxCommon->GetDevice();
    depthStencilResource_ = dxCommon->GetDepthStencilResource();

    // ----------------------------------------------------------
    // 1. RenderTexture リソースの生成 (1280x720 / RGBA8 / RT 可)
    // ----------------------------------------------------------
    renderTexture_ = CreateRenderTextureResource(
        device, 1280, 720, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor_);

    // ----------------------------------------------------------
    // 2. RTV デスクリプタヒープと RTV の生成
    //    RenderTexture を「書き込み先」として設定するために必要
    // ----------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // CPU からのみアクセス
    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_));
    assert(SUCCEEDED(hr));

    device->CreateRenderTargetView(
        renderTexture_.Get(), nullptr,
        rtvHeap_->GetCPUDescriptorHandleForHeapStart());

    // ----------------------------------------------------------
    // 3. SRV デスクリプタヒープと SRV の生成
    //    コピーパスでシェーダーがテクスチャをサンプリングするため SHADER_VISIBLE 必須
    // ----------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 4;
    srvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_));
    assert(SUCCEEDED(hr));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels     = 1;
    device->CreateShaderResourceView(
        renderTexture_.Get(), &srvDesc,
        srvHeap_->GetCPUDescriptorHandleForHeapStart());

    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
    depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE depthSrvHandle = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    depthSrvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateShaderResourceView(depthStencilResource_, &depthSrvDesc, depthSrvHandle);

    dissolveMaskTextures_[0] = CreateTextureResourceFromFile(device, L"Resources/Models/Work/noise0.png");
    dissolveMaskTextures_[1] = CreateTextureResourceFromFile(device, L"Resources/Models/Work/noise1.png");
    for (int32_t index = 0; index < 2; ++index) {
        D3D12_SHADER_RESOURCE_VIEW_DESC maskSrvDesc{};
        maskSrvDesc.Format = dissolveMaskTextures_[index]->GetDesc().Format;
        maskSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        maskSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        maskSrvDesc.Texture2D.MipLevels = 1;

        D3D12_CPU_DESCRIPTOR_HANDLE maskSrvHandle = srvHeap_->GetCPUDescriptorHandleForHeapStart();
        maskSrvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * (2 + index);
        device->CreateShaderResourceView(dissolveMaskTextures_[index].Get(), &maskSrvDesc, maskSrvHandle);
    }

    // ----------------------------------------------------------
    // 4. コピー用 RootSignature の生成
    //    スロット構成：
    //      [0] DescriptorTable : SRV t0 (RenderTexture)
    //      [1] CBV             : b0  (ヴィネット定数)
    //    StaticSampler        : s0  (LINEAR / CLAMP)
    // ----------------------------------------------------------
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.NumDescriptors                    = 4;
    descriptorRange.BaseShaderRegister                = 0; // t0
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[6]{};
    // スロット 0 : SRV テーブル (Pixel Shader のみ参照)
    rootParameters[0].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges   = &descriptorRange;
    // スロット 1 : CBV (ヴィネット定数、Pixel Shader のみ参照)
    rootParameters[1].ParameterType                     = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility                  = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister         = 0; // b0
    rootParameters[1].Descriptor.RegisterSpace          = 0;
    // スロット 2 : CBV (Outline用定数、Pixel Shader のみ参照)
    rootParameters[2].ParameterType                     = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility                  = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].Descriptor.ShaderRegister         = 1; // b1
    rootParameters[2].Descriptor.RegisterSpace          = 0;
    // スロット 3 : CBV (RadialBlur用定数、Pixel Shader のみ参照)
    rootParameters[3].ParameterType                     = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility                  = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister         = 2; // b2
    rootParameters[3].Descriptor.RegisterSpace          = 0;
    // スロット 4 : CBV (Dissolve用定数、Pixel Shader のみ参照)
    rootParameters[4].ParameterType                     = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility                  = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].Descriptor.ShaderRegister         = 3; // b3
    rootParameters[4].Descriptor.RegisterSpace          = 0;
    // スロット 5 : CBV (Random用定数、Pixel Shader のみ参照)
    rootParameters[5].ParameterType                     = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[5].ShaderVisibility                  = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].Descriptor.ShaderRegister         = 4; // b4
    rootParameters[5].Descriptor.RegisterSpace          = 0;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[2]{};
    staticSamplers[0].Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD           = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister   = 0; // s0
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    staticSamplers[1] = staticSamplers[0];
    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSamplers[1].ShaderRegister = 1; // s1

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters     = 6;
    rootSignatureDesc.pParameters       = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 2;
    rootSignatureDesc.pStaticSamplers   = staticSamplers;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(
        &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) { OutputDebugStringA((char*)errorBlob->GetBufferPointer()); }
        assert(false);
    }
    hr = device->CreateRootSignature(
        0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&copyRootSignature_));
    assert(SUCCEEDED(hr));

    // ----------------------------------------------------------
    // 5. ポストプロセス用 PSO 群の生成
    //    頂点シェーダーは全エフェクト共通 (頂点バッファ不要の全画面三角形)
    //    ピクセルシェーダーがエフェクトごとに異なる
    // ----------------------------------------------------------
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob      = dxCommon->CompileShader(L"Resources/shaders/hlsl/Fullscreen.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psCopyBlob  = dxCommon->CompileShader(L"Resources/shaders/hlsl/CopyImage.PS.hlsl",  L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psGrayBlob  = dxCommon->CompileShader(L"Resources/shaders/hlsl/Grayscale.PS.hlsl",  L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psSepiaBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/Sepia.PS.hlsl",      L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psVigBlob   = dxCommon->CompileShader(L"Resources/shaders/hlsl/Vignette.PS.hlsl",   L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psBox3Blob  = dxCommon->CompileShader(L"Resources/shaders/hlsl/BoxFilter3x3.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psBox5Blob  = dxCommon->CompileShader(L"Resources/shaders/hlsl/BoxFilter5x5.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psGaussianBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/GaussianFilter.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psLuminanceOutlineBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/LuminanceBasedOutline.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psDepthOutlineBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/DepthBasedOutline.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psRadialBlurBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/RadialBlur.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psDissolveBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/Dissolve.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psRandomBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/Random.PS.hlsl", L"ps_6_0");

    // PSO の共通設定 (入力レイアウトなし・深度テストなし・三角形リスト)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = copyRootSignature_.Get();
    psoDesc.VS             = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.InputLayout.pInputElementDescs = nullptr;
    psoDesc.InputLayout.NumElements        = 0;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState.RenderTarget[0].BlendEnable           = false;
    psoDesc.RasterizerState.FillMode        = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode        = D3D12_CULL_MODE_NONE; // 全画面三角形は裏面が当たるため NONE
    psoDesc.DepthStencilState.DepthEnable   = false;                // 深度テスト不要
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.PrimitiveTopologyType           = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets                = 1;
    psoDesc.RTVFormats[0]                   = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat                       = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count               = 1;
    psoDesc.SampleMask                     = D3D12_DEFAULT_SAMPLE_MASK;

    // A. 通常コピー (エフェクトなし)
    psoDesc.PS = { psCopyBlob->GetBufferPointer(), psCopyBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&copyPipelineState_));
    assert(SUCCEEDED(hr));

    // B. グレースケール
    psoDesc.PS = { psGrayBlob->GetBufferPointer(), psGrayBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&grayscalePipelineState_));
    assert(SUCCEEDED(hr));

    // C. セピア調
    psoDesc.PS = { psSepiaBlob->GetBufferPointer(), psSepiaBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sepiaPipelineState_));
    assert(SUCCEEDED(hr));

    // D. ヴィネッティング
    psoDesc.PS = { psVigBlob->GetBufferPointer(), psVigBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&vignettePipelineState_));
    assert(SUCCEEDED(hr));

    // E. BoxFilter 3x3
    psoDesc.PS = { psBox3Blob->GetBufferPointer(), psBox3Blob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&boxFilter3x3PipelineState_));
    assert(SUCCEEDED(hr));

    // F. BoxFilter 5x5
    psoDesc.PS = { psBox5Blob->GetBufferPointer(), psBox5Blob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&boxFilter5x5PipelineState_));
    assert(SUCCEEDED(hr));

    // G. GaussianFilter
    psoDesc.PS = { psGaussianBlob->GetBufferPointer(), psGaussianBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&gaussianFilterPipelineState_));
    assert(SUCCEEDED(hr));

    // H. LuminanceBasedOutline
    psoDesc.PS = { psLuminanceOutlineBlob->GetBufferPointer(), psLuminanceOutlineBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&luminanceOutlinePipelineState_));
    assert(SUCCEEDED(hr));

    // I. DepthBasedOutline
    psoDesc.PS = { psDepthOutlineBlob->GetBufferPointer(), psDepthOutlineBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&depthOutlinePipelineState_));
    assert(SUCCEEDED(hr));

    // J. RadialBlur
    psoDesc.PS = { psRadialBlurBlob->GetBufferPointer(), psRadialBlurBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&radialBlurPipelineState_));
    assert(SUCCEEDED(hr));

    // K. Dissolve
    psoDesc.PS = { psDissolveBlob->GetBufferPointer(), psDissolveBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&dissolvePipelineState_));
    assert(SUCCEEDED(hr));

    // L. Random
    psoDesc.PS = { psRandomBlob->GetBufferPointer(), psRandomBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&randomPipelineState_));
    assert(SUCCEEDED(hr));

    // ----------------------------------------------------------
    // 6. ヴィネッティング用定数バッファの生成
    //    Upload ヒープで CPU から毎フレーム書き換え可能にする
    //    サイズは 256バイト境界にアライン (D3D12 の制約)
    // ----------------------------------------------------------
    D3D12_HEAP_PROPERTIES cbHeapProps = {
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN, 1, 1
    };
    D3D12_RESOURCE_DESC cbResDesc{};
    cbResDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbResDesc.Width              = (sizeof(VignetteParams) + 0xff) & ~0xff; // 256バイトアライン
    cbResDesc.Height             = 1;
    cbResDesc.DepthOrArraySize   = 1;
    cbResDesc.MipLevels          = 1;
    cbResDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    cbResDesc.SampleDesc.Count   = 1;
    hr = device->CreateCommittedResource(
        &cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbResDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&vignetteConstantBuffer_));
    assert(SUCCEEDED(hr));

    // Map して CPU から直接書き込めるようにしておく (アプリ終了まで Unmap しない)
    vignetteConstantBuffer_->Map(0, nullptr, (void**)&vignetteParamsData_);

    // 初期値の設定
    vignetteParamsData_->scale    = 16.0f;
    vignetteParamsData_->exponent = 0.8f;

    cbResDesc.Width = (sizeof(OutlineParams) + 0xff) & ~0xff;
    hr = device->CreateCommittedResource(
        &cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbResDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&outlineConstantBuffer_));
    assert(SUCCEEDED(hr));
    outlineConstantBuffer_->Map(0, nullptr, (void**)&outlineParamsData_);
    outlineParamsData_->projectionInverse = Math::MakeIdentity4x4();
    outlineParamsData_->depthStrength = 0.05f;

    cbResDesc.Width = (sizeof(RadialBlurParams) + 0xff) & ~0xff;
    hr = device->CreateCommittedResource(
        &cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbResDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&radialBlurConstantBuffer_));
    assert(SUCCEEDED(hr));
    radialBlurConstantBuffer_->Map(0, nullptr, (void**)&radialBlurParamsData_);
    radialBlurParamsData_->center = { 0.5f, 0.5f };
    radialBlurParamsData_->blurWidth = 0.01f;
    radialBlurParamsData_->sampleCount = 10;

    cbResDesc.Width = (sizeof(DissolveParams) + 0xff) & ~0xff;
    hr = device->CreateCommittedResource(
        &cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbResDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&dissolveConstantBuffer_));
    assert(SUCCEEDED(hr));
    dissolveConstantBuffer_->Map(0, nullptr, (void**)&dissolveParamsData_);
    dissolveParamsData_->threshold = 0.0f;
    dissolveParamsData_->edgeWidth = 0.03f;
    dissolveParamsData_->maskIndex = 0;
    dissolveParamsData_->padding = 0.0f;
    dissolveParamsData_->edgeColor = { 1.0f, 0.4f, 0.3f, 1.0f };

    cbResDesc.Width = (sizeof(RandomParams) + 0xff) & ~0xff;
    hr = device->CreateCommittedResource(
        &cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbResDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&randomConstantBuffer_));
    assert(SUCCEEDED(hr));
    randomConstantBuffer_->Map(0, nullptr, (void**)&randomParamsData_);
    randomParamsData_->time = 0.0f;
    randomParamsData_->mode = 0;
    randomParamsData_->strength = 0.5f;
    randomParamsData_->padding = 0.0f;
}

// ==========================================================
//  PostProcessRenderer::BeginRender
//  RenderTexture を RT 状態へ遷移し、ビューポート・クリアを設定する
// ==========================================================
void PostProcessRenderer::BeginRender(ID3D12GraphicsCommandList* cmdList, DirectXCommon* dxCommon) {
    // 1. RenderTexture を RENDER_TARGET 状態へ遷移 (現在の状態と異なるときのみバリアを張る)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = renderTexture_.Get();
    barrier.Transition.StateBefore = renderTextureState_;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    if (barrier.Transition.StateBefore != barrier.Transition.StateAfter) {
        cmdList->ResourceBarrier(1, &barrier);
    }
    renderTextureState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // 2. レンダーターゲットに RenderTexture をセット (深度バッファは通常 DSV を流用)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHeap()->GetCPUDescriptorHandleForHeapStart();
    cmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

    // 3. ビューポートとシザーを RenderTexture のサイズ (1280x720) に合わせる
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    D3D12_RECT     scissor  = { 0, 0, 1280, 720 };
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    // 4. カラーバッファと深度バッファのクリア
    float cc[4] = { clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w };
    cmdList->ClearRenderTargetView(rtvHandle, cc, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

// ==========================================================
//  PostProcessRenderer::EndRender
//  RenderTexture を PIXEL_SHADER_RESOURCE 状態へ遷移させる
// ==========================================================
void PostProcessRenderer::EndRender(ID3D12GraphicsCommandList* cmdList) {
    // コピーパスでシェーダーからサンプリングするために状態を遷移させる
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = renderTexture_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
    renderTextureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

// ==========================================================
//  PostProcessRenderer::DrawToBackBuffer
//  選択されたポストエフェクト PSO でバックバッファに全画面コピー描画する
// ==========================================================
void PostProcessRenderer::DrawToBackBuffer(ID3D12GraphicsCommandList* cmdList, const Matrix4x4& projectionMatrix) {
    // RootSignature とエフェクトに対応した PSO をバインド
    if (outlineParamsData_) {
        outlineParamsData_->projectionInverse = Math::Inverse(projectionMatrix);
    }
    if (randomParamsData_) {
        randomParamsData_->time += 1.0f / 60.0f;
    }

    const bool usesDepth = postEffectMode_ == 8 && depthStencilResource_;
    if (usesDepth) {
        D3D12_RESOURCE_BARRIER depthBarrier{};
        depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        depthBarrier.Transition.pResource = depthStencilResource_;
        depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &depthBarrier);
    }

    cmdList->SetGraphicsRootSignature(copyRootSignature_.Get());
    switch (postEffectMode_) {
    case 1: // グレースケール
        cmdList->SetPipelineState(grayscalePipelineState_.Get());
        break;
    case 2: // セピア調
        cmdList->SetPipelineState(sepiaPipelineState_.Get());
        break;
    case 3: // ヴィネッティング (定数バッファも一緒にバインド)
        cmdList->SetPipelineState(vignettePipelineState_.Get());
        cmdList->SetGraphicsRootConstantBufferView(
            1, vignetteConstantBuffer_->GetGPUVirtualAddress());
        break;
    case 4: // BoxFilter 3x3
        cmdList->SetPipelineState(boxFilter3x3PipelineState_.Get());
        break;
    case 5: // BoxFilter 5x5
        cmdList->SetPipelineState(boxFilter5x5PipelineState_.Get());
        break;
    case 6: // GaussianFilter
        cmdList->SetPipelineState(gaussianFilterPipelineState_.Get());
        break;
    case 7: // LuminanceBasedOutline
        cmdList->SetPipelineState(luminanceOutlinePipelineState_.Get());
        break;
    case 8: // DepthBasedOutline
        cmdList->SetPipelineState(depthOutlinePipelineState_.Get());
        if (outlineConstantBuffer_) {
            cmdList->SetGraphicsRootConstantBufferView(
                2, outlineConstantBuffer_->GetGPUVirtualAddress());
        }
        break;
    case 9: // RadialBlur
        cmdList->SetPipelineState(radialBlurPipelineState_.Get());
        if (radialBlurConstantBuffer_) {
            cmdList->SetGraphicsRootConstantBufferView(
                3, radialBlurConstantBuffer_->GetGPUVirtualAddress());
        }
        break;
    case 10: // Dissolve
        cmdList->SetPipelineState(dissolvePipelineState_.Get());
        if (dissolveConstantBuffer_) {
            cmdList->SetGraphicsRootConstantBufferView(
                4, dissolveConstantBuffer_->GetGPUVirtualAddress());
        }
        break;
    case 11: // Random
        cmdList->SetPipelineState(randomPipelineState_.Get());
        if (randomConstantBuffer_) {
            cmdList->SetGraphicsRootConstantBufferView(
                5, randomConstantBuffer_->GetGPUVirtualAddress());
        }
        break;
    default: // 通常コピー (エフェクトなし)
        cmdList->SetPipelineState(copyPipelineState_.Get());
        break;
    }

    // SRV ヒープをバインドし、RenderTexture の SRV をスロット 0 (t0) にセット
    ID3D12DescriptorHeap* heaps[] = { srvHeap_.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(
        0, srvHeap_->GetGPUDescriptorHandleForHeapStart());

    // 全画面三角形を描画 (Fullscreen.VS.hlsl が SV_VertexID から頂点を内部生成するため VB 不要)
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);

    if (usesDepth) {
        D3D12_RESOURCE_BARRIER depthBarrier{};
        depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        depthBarrier.Transition.pResource = depthStencilResource_;
        depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &depthBarrier);
    }
}

// ==========================================================
//  PostProcessRenderer::DrawImGui
//  設定パネルの描画 (MyGame の左パネル "Information" 内から呼ばれる)
// ==========================================================
void PostProcessRenderer::DrawImGui() {
    if (ImGui::CollapsingHeader("Offscreen Rendering (RenderTexture)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Offscreen Rendering", &enabled_);
        ImGui::ColorEdit4("Clear Color (VRAM)", &clearColor_.x);

        const char* skyboxModes[] = { "Ignore", "Link (Multiply)" };
        ImGui::Combo("Skybox Color Link", &skyboxLinkMode_, skyboxModes, IM_ARRAYSIZE(skyboxModes));

        const char* effectNames[] = { "Normal", "Grayscale", "Sepia", "Vignette", "BoxFilter 3x3", "BoxFilter 5x5", "GaussianFilter", "Luminance Outline", "Depth Outline", "RadialBlur", "Dissolve", "Random" };
        ImGui::Combo("Post Effect", &postEffectMode_, effectNames, IM_ARRAYSIZE(effectNames));

        // ヴィネット選択時のみパラメータスライダーを表示
        if (postEffectMode_ == 3 && vignetteParamsData_) {
            ImGui::DragFloat("Vignette Scale",    &vignetteParamsData_->scale,    0.1f, 0.0f, 100.0f, "%.1f");
            ImGui::DragFloat("Vignette Exponent", &vignetteParamsData_->exponent, 0.05f, 0.0f, 10.0f, "%.2f");
        }
        if (postEffectMode_ == 8 && outlineParamsData_) {
            ImGui::DragFloat("Depth Outline Strength", &outlineParamsData_->depthStrength, 0.005f, 0.0f, 1.0f, "%.3f");
        }
        if (postEffectMode_ == 9 && radialBlurParamsData_) {
            ImGui::DragFloat2("Radial Center", &radialBlurParamsData_->center.x, 0.005f, 0.0f, 1.0f, "%.3f");
            ImGui::DragFloat("Radial Blur Width", &radialBlurParamsData_->blurWidth, 0.001f, 0.0f, 0.2f, "%.3f");
            ImGui::SliderInt("Radial Samples", &radialBlurParamsData_->sampleCount, 1, 32);
        }
        if (postEffectMode_ == 10 && dissolveParamsData_) {
            const char* maskNames[] = { "noise0", "noise1" };
            ImGui::Combo("Dissolve Mask", &dissolveParamsData_->maskIndex, maskNames, IM_ARRAYSIZE(maskNames));
            ImGui::DragFloat("Dissolve Threshold", &dissolveParamsData_->threshold, 0.005f, 0.0f, 1.0f, "%.3f");
            ImGui::DragFloat("Dissolve Edge Width", &dissolveParamsData_->edgeWidth, 0.001f, 0.0f, 0.2f, "%.3f");
            ImGui::ColorEdit4("Dissolve Edge Color", &dissolveParamsData_->edgeColor.x);
        }
        if (postEffectMode_ == 11 && randomParamsData_) {
            const char* randomModes[] = { "Grayscale Noise", "Multiply Image" };
            ImGui::Combo("Random Mode", &randomParamsData_->mode, randomModes, IM_ARRAYSIZE(randomModes));
            ImGui::DragFloat("Random Strength", &randomParamsData_->strength, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::Text("Random Time: %.2f", randomParamsData_->time);
        }
    }
}

// ==========================================================
//  PostProcessRenderer::CreateRenderTextureResource  [private]
//  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET 付きの Texture2D リソースを生成する
// ==========================================================
Microsoft::WRL::ComPtr<ID3D12Resource> PostProcessRenderer::CreateRenderTextureResource(
    ID3D12Device* device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4& clearColor)
{
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width            = width;
    resourceDesc.Height           = height;
    resourceDesc.MipLevels        = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format           = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // RT として使うため必須

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // GPU 専用メモリ

    // クリアカラーの最適化ヒントを渡しておく (同じ値でクリアすると GPU が最適化できる)
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format   = format;
    clearValue.Color[0] = clearColor.x;
    clearValue.Color[1] = clearColor.y;
    clearValue.Color[2] = clearColor.z;
    clearValue.Color[3] = clearColor.w;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> PostProcessRenderer::CreateTextureResourceFromFile(
    ID3D12Device* device,
    const wchar_t* filePath)
{
    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromWICFile(filePath, DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    assert(SUCCEEDED(hr));

    const DirectX::Image* firstImage = image.GetImage(0, 0, 0);
    const DirectX::TexMetadata& metadata = image.GetMetadata();
    assert(firstImage);

    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = static_cast<UINT>(metadata.width);
    textureDesc.Height = static_cast<UINT>(metadata.height);
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = metadata.format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES heapProps{
        D3D12_HEAP_TYPE_CUSTOM,
        D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
        D3D12_MEMORY_POOL_L0,
        1,
        1
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    hr = resource->WriteToSubresource(
        0,
        nullptr,
        firstImage->pixels,
        static_cast<UINT>(firstImage->rowPitch),
        static_cast<UINT>(firstImage->slicePitch));
    assert(SUCCEEDED(hr));

    return resource;
}
