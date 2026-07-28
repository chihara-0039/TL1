#include "Object3d.h"
#include "TextureManager.h" // GetSrvHandleGPUを使うために必要
#include <cassert>

// 初期化関数。Object3dCommonへのポインタを受け取ります
void Object3d::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;
    auto device = object3dCommon_->GetDxCommon()->GetDevice();

    // Transform Buffer
    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = (sizeof(TransformationMatrix) + 0xff) & ~0xff;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&transformationResource_));
    transformationResource_->Map(0, nullptr, (void**)&transformationData_);
    transformationData_->WVP = Math::MakeIdentity4x4();
    transformationData_->World = Math::MakeIdentity4x4();
    transformationData_->WorldInverseTranspose = Math::MakeIdentity4x4();

    // Material Buffer
    resDesc.Width = (sizeof(Material) + 0xff) & ~0xff;
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&materialResource_));
    materialResource_->Map(0, nullptr, (void**)&materialData_);

    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 1;
    materialData_->shininess = 0.5f;
    materialData_->metallic = 0.0f;
    materialData_->emissive = 0.0f;
    materialData_->uvTransform = Math::MakeIdentity4x4();
    materialData_->environmentCoefficient = 0.0f;
}

// 毎フレーム呼び出す更新関数。ワールド行列とWVP行列を計算して定数バッファに転送します
void Object3d::Update(const Matrix4x4& lightVP) {
    // 1. ワールド行列の計算
    Matrix4x4 worldMatrix = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

    // 2. カメラ視点の WVP 行列の計算
    Matrix4x4 wvpMatrix = Math::Multiply(worldMatrix, Math::Multiply(viewMatrix_, projectionMatrix_));
    Matrix4x4 worldInverseTransposeMatrix = Math::Inverse(Math::Transpose(worldMatrix));

    // 3. 定数バッファ(GPUに送るデータ)への書き込み
    transformationData_->WVP = wvpMatrix;
    transformationData_->World = worldMatrix;
    transformationData_->WorldInverseTranspose = worldInverseTransposeMatrix;

    // ★ ここが重要：ピクセルシェーダーでの影判定に使うため、ライト行列を転送します
    transformationData_->lightViewProjection = lightVP;
}

// UV変換のセッター。引数にスケール、回転、平行移動をまとめた Transform 構造体を受け取ります
void Object3d::SetUVTransform(const Transform& t) {
    if (materialData_) {
        Matrix4x4 w = Math::MakeAffineMatrix(t.scale, t.rotate, t.translate);
        materialData_->uvTransform = w;
    }
}

// 描画関数。モデルがセットされていない場合は何もしない
void Object3d::Draw() {
    if (!model_) return;
    auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();

    // 0. マテリアル
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    // 1. Transform (トランスフォーム)
    commandList->SetGraphicsRootConstantBufferView(1, transformationResource_->GetGPUVirtualAddress());
    // 2. Light (平行光源) (Commonが持つ)
    commandList->SetGraphicsRootConstantBufferView(2, object3dCommon_->GetLightGPUVirtualAddress());
    // 3. Texture (テクスチャ)
    // ★ここが修正ポイント: Common経由でTextureManagerを呼び出す
    if (object3dCommon_->GetTextureManager()) {
        auto gpuHandle = object3dCommon_->GetTextureManager()->GetSrvHandleGPU(model_->GetTextureHandle());
        commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);

        auto environmentHandle = object3dCommon_->GetTextureManager()->GetSrvHandleGPU(object3dCommon_->GetEnvironmentTextureHandle());
        commandList->SetGraphicsRootDescriptorTable(6, environmentHandle);
    }

    model_->Draw(commandList);
}

// 影描画用の関数。引数にライトの ViewProjection 行列を受け取ります
void Object3d::DrawShadow(const Matrix4x4& lightViewProjection) {
    if (!model_) { 
        return;
    }

    // 3. コマンドを積む
    auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();

    // ルートパラメータ 1番（Transform）に定数バッファをセット
    commandList->SetGraphicsRootConstantBufferView(1, transformationResource_->GetGPUVirtualAddress());

    // モデル（頂点バッファ）の描画。引数に commandList が必要です
    model_->Draw(commandList);
}

