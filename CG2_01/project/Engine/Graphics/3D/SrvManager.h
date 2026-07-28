#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;

// SRV/CBV/UAV用ディスクリプタヒープを一元管理するクラス。
// TextureManagerやImGuiなど、複数システムがSRVを必要とする場合の共通基盤として使う。
class SrvManager {
public:
    static constexpr uint32_t kMaxSRVCount = 512;

    void Initialize(DirectXCommon* dxCommon, uint32_t maxDescriptors = kMaxSRVCount);

    uint32_t Allocate();

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index) const;
    ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

    void CreateSRVforTexture2D(uint32_t index, ID3D12Resource* resource, DXGI_FORMAT format, UINT mipLevels);
    void CreateSRVforStructuredBuffer(uint32_t index, ID3D12Resource* resource, UINT numElements, UINT structureByteStride);
    void CreateSRV(uint32_t index, ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);

    uint32_t GetUseCount() const { return useIndex_; }
    uint32_t GetMaxCount() const { return maxDescriptors_; }

private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    uint32_t descriptorSize_ = 0;
    uint32_t useIndex_ = 0;
    uint32_t maxDescriptors_ = kMaxSRVCount;
};
