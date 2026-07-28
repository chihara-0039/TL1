#pragma once
#include "Object3dCommon.h"
#include "Object3d.h"
#include "MyMath.h"
#include <d3d12.h>
#include <wrl.h>

// キューブマップ風の背景を描画するスカイボックス。
class Skybox {
public:
    // 使用するテクスチャと描画リソースを初期化する。
    void Initialize(Object3dCommon* object3dCommon, uint32_t textureHandle);
    // 現在のトランスフォームとカメラ行列から定数バッファを更新する。
    void Update();
    // カメラ位置へ追従させる場合の更新処理。
    void Update(const Vector3& cameraPosition);
    // スカイボックス専用PSOで背景を描画する。
    void Draw();

    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_ = view;
        projectionMatrix_ = projection;
    }

    void SetColor(const Vector4& color) {
        if (materialData_) {
            materialData_->color = color;
        }
    }

    void SetScale(const Vector3& scale) {
        transform_.scale = scale;
    }

    void SetPosition(const Vector3& pos) {
        transform_.translate = pos;
    }

private:
    // 立方体メッシュの頂点・インデックスバッファを作成する。
    void CreateMesh();
    // スカイボックス描画用のパイプラインステートを作成する。
    void CreateGraphicsPipeline();

private:
    Object3dCommon* object3dCommon_ = nullptr;
    uint32_t textureHandle_ = 0;

    // 背景として使うため、基本的にはカメラ位置に追従させるトランスフォーム。
    Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    // 立方体メッシュの頂点・インデックスリソース。
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

    // ワールド変換と色をシェーダーへ渡す定数バッファ。
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
    TransformationMatrix* transformationData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    // パイプラインステート。RootSignatureはObject3dCommon側のものを使用する。
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
