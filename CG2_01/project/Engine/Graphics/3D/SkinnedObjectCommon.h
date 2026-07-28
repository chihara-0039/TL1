#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"

struct SkinnedPointLightData {
    Vector3 position;
    float intensity;
    Vector4 color;
    float radius;
    float padding[3];
};

// スキニング描画用シェーダに渡すライト情報。
struct DirectionalLight {
    Vector4 color;           // 平行光源の色
    Vector3 direction;       // 平行光源の向き
    float   intensity;       // 平行光源の強さ
    Vector3 cameraPosition;  // スペキュラ計算用のカメラ位置
    float   paddingLight;    // 16 byte alignment padding
    SkinnedPointLightData pointLights[8];
    uint32_t pointLightCount;
    float pointLightPadding[3];
};

// SkinnedObject 用の RootSignature / PSO / ライトバッファを管理する。
// 現在は一部 Object3dCommon と似た構成だが、スキニング頂点入力を扱うため別クラスとして残している。
class SkinnedObjectCommon {
public:
    // スキニング描画に必要な GPU リソースを作成する。
    void Initialize(DirectXCommon* dxCommon);

    // スキニング描画用 RootSignature / PSO / プリミティブ種別を CommandList に設定する。
    void PreDraw();

    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }
    ID3D12PipelineState* GetShadowPipelineState() const { return shadowPipelineState_.Get(); }

    ID3D12RootSignature* GetInstancedRootSignature() const { return instancedRootSignature_.Get(); }
    ID3D12PipelineState* GetInstancedPipelineState() const { return instancedPipelineState_.Get(); }
    ID3D12PipelineState* GetInstancedShadowPipelineState() const { return instancedShadowPipelineState_.Get(); }
    ID3D12PipelineState* GetInstancedAlphaPipelineState() const { return instancedAlphaPipelineState_.Get(); }

    void SetDefaultLight();

    void SetLightDirection(const Vector3& direction) {
        if (lightData_) {
            lightData_->direction = Math::Normalize(direction);
        }
    }

    void SetLightColor(const Vector4& color) {
        if (lightData_) {
            lightData_->color = color;
        }
    }

    void SetLightIntensity(float intensity) {
        if (lightData_) {
            lightData_->intensity = intensity;
        }
    }

    void SetCameraPosition(const Vector3& cameraPosition) {
        if (lightData_) {
            lightData_->cameraPosition = cameraPosition;
        }
    }

    void ClearPointLights() { if (lightData_) lightData_->pointLightCount = 0; }
    bool AddPointLight(const Vector3& pos, float intensity, const Vector4& color, float radius = 10.0f) {
        if (!lightData_ || lightData_->pointLightCount >= 8) return false;
        SkinnedPointLightData& light = lightData_->pointLights[lightData_->pointLightCount++];
        light.position = pos;
        light.intensity = intensity;
        light.color = color;
        light.radius = radius > 0.01f ? radius : 0.01f;
        light.padding[0] = light.padding[1] = light.padding[2] = 0.0f;
        return true;
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetLightGPUVirtualAddress() const {
        return lightResource_->GetGPUVirtualAddress();
    }

    void SetTextureManager(TextureManager* textureManager) { textureManager_ = textureManager; }
    TextureManager* GetTextureManager() const { return textureManager_; }

    // 壁越しのプレイヤーシルエット表示用 PSO を CommandList に設定する。
    void PreDrawPlayerHighlight();

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();
    void CreateLightBuffer();
    void CreateShadowPipeline();
    void CreateInstancedRootSignature();
    void CreateInstancedGraphicsPipeline();
    void CreateInstancedShadowPipeline();
    void CreatePlayerHighlightPipeline();
    void CreateInstancedAlphaPipeline();

private:
    DirectXCommon*  dxCommon_       = nullptr;
    TextureManager* textureManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> playerHighlightPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> instancedRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedShadowPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedAlphaPipelineState_;
};
