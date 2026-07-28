#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "MyMath.h"

class TextureManager {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // Loads a texture if needed and returns its SRV handle index.
    uint32_t LoadTexture(const std::string& filePath);

    // Registers a texture resource owned by another system and returns its SRV handle index.
    uint32_t RegisterExternalTexture(ID3D12Resource* resource);

    ID3D12DescriptorHeap* GetSrvHeap() const { return srvManager_ ? srvManager_->GetDescriptorHeap() : nullptr; }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureHandle);
    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandleCPU(uint32_t textureHandle) const;

    const D3D12_RESOURCE_DESC& GetResourceDesc(uint32_t textureHandle);
    ID3D12Resource* GetResource(uint32_t textureHandle) const;

private:
    struct TextureData {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
        D3D12_RESOURCE_DESC resourceDesc;
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::vector<TextureData> textures_;
    std::unordered_map<std::string, uint32_t> fileMap_;

    static const size_t kMaxTextures = 128;
};
