#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h" // 追加
#include <wrl.h>
#include <d3d12.h>
#include <memory>

// Sprite描画で共通利用するRootSignature、PSO、TextureManager参照を管理する。
class SpriteCommon {
public:
    // 2D描画用のRootSignatureとGraphicsPipelineを作成する。
    void Initialize(DirectXCommon* dxCommon);

    // Sprite描画前に共通PSOとRootSignatureをCommandListへ設定する。
    void PreDraw();

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    TextureManager* GetTextureManager() { return textureManager_; }
    ID3D12RootSignature* GetRootSignature() { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() { return pipelineState_.Get(); }

    // SpriteがテクスチャSRVを取得できるよう、TextureManagerを関連付ける。
    void SetTextureManager(TextureManager* textureManager) {
        textureManager_ = textureManager;
    }

private:
    // Sprite用RootSignatureを作成する。
    void CreateRootSignature();
    // Sprite用のGraphicsPipelineStateを作成する。
    void CreateGraphicsPipeline();

private:
    // DirectXCommonとTextureManagerは外部所有。ここでは参照だけ保持する。
    DirectXCommon* dxCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};
