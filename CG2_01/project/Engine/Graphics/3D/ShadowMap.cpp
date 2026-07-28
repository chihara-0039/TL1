#include "ShadowMap.h"
#include <cassert>

using namespace Microsoft::WRL;

void ShadowMap::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager) {
    ID3D12Device* device = dxCommon->GetDevice();

    // 1. 影用テクスチャリソースの設定
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Width = kWidth;   // 2048などの解像度
    resDesc.Height = kHeight;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    // 深度書き込み(DSV)とテクスチャ読み込み(SRV)で使い分けるため TYPELESS にする
    resDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    resDesc.SampleDesc.Count = 1;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // 深度バッファとして使うフラグ
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    // クリア値の設定（影がない場所は一番遠い「1.0」で埋める）
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    // リソースの生成
    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // 初期状態はSRV（読み取り用）にしておくのがポイント
        &clearValue, IID_PPV_ARGS(&resource_));
    assert(SUCCEEDED(hr));

    // --- 2. DSV用の棚(ヒープ)を作成 ---
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1; // 1つだけ
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; // 深度用
    device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_));

    // 棚の先頭の住所(ハンドル)を保存
    dsvHandle_ = dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    // 「このテクスチャは深度バッファです」というカードを作成して棚に置く
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // 元がR32_TYPELESSなので、ここでD32と定義
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(resource_.Get(), &dsvDesc, dsvHandle_);


    // --- 3. SRV用の棚(ヒープ)を作成 ---
    uint32_t handle = textureManager->RegisterExternalTexture(resource_.Get());
    srvHandle_ = textureManager->GetSrvHandleGPU(handle);
    // 棚の先頭の住所(ハンドル)を保存
    //srvHandle_ = srvHeap_->GetGPUDescriptorHandleForHeapStart();

    // 「このテクスチャを読み取り用として使うよ」というカードを作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // ここでR32と定義
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    // CPU側のハンドル（SRV用）を取得して作成
    //D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    //device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvCpuHandle);
}

void ShadowMap::PreDraw(ID3D12GraphicsCommandList* commandList) {
    // 1. リソースバリアを張る（読み取り用から書き込み用へ）
    // 前のフレームでシェーダーが影を参照していた状態から、書き込みモードに切り替えます
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    // 2. レンダーターゲット（RTV）は使わず、DSVのみセットする
    // 第1引数を 0、第2引数を nullptr にするのがポイントです
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle_);

    // 3. 影用テクスチャをクリアする（一番遠い 1.0f で塗りつぶす）
    commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 4. ビューポートとシザー矩形をシャドウマップの解像度に合わせる
    // 画面サイズ（1280x720等）ではなく、2048x2048などに設定します
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)kWidth, (float)kHeight, 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, kWidth, kHeight };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
}

void ShadowMap::PostDraw(ID3D12GraphicsCommandList* commandList) {
    // 描画後は、リソースバリアを戻す（書き込み用から読み取り用へ）
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
}

