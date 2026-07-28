#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "MyMath.h"
#include "SpriteCommon.h"

// 2D UIやHUD表示に使う矩形スプライト。
class Sprite {
public:
    // 1頂点分の位置とUV。
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
    };

    // スプライト全体に乗算する色。
    struct Material {
        Vector4 color;
    };

    // 2D描画用のワールド・ビュー・プロジェクション合成行列。
    struct TransformationMatrix {
        Matrix4x4 WVP;
    };

public:
    // 使用する共通描画設定とテクスチャを指定して初期化する。
    void Initialize(SpriteCommon* spriteCommon, uint32_t textureHandle);

    // 位置・サイズ・回転などの変更をGPUバッファへ反映する。
    void Update();

    // SpriteCommon::PreDraw後に呼び、現在のテクスチャで描画する。
    void Draw();

    // 画面上の表示位置を設定する。
    void SetPosition(const Vector2& position) { position_ = position; transferNeeded_ = true; }
    // Z軸回転角をラジアンで設定する。
    void SetRotation(float rotation) { rotation_ = rotation; transferNeeded_ = true; }
    // 表示サイズをピクセル基準で設定する。
    void SetSize(const Vector2& size) { size_ = size; transferNeeded_ = true; }
    void SetColor(const Vector4& color) { materialData_->color = color; }

    // アンカーポイント。0.0～1.0で指定し、中心なら{0.5f, 0.5f}。
    void SetAnchorPoint(const Vector2& anchor) { anchorPoint_ = anchor; transferNeeded_ = true; }

    // テクスチャの一部だけを表示するための切り抜き矩形をピクセル座標で指定する。
    void SetTextureRect(const Vector2& position, const Vector2& size);
    // 描画に使うテクスチャを差し替える。
    void SetTexture(uint32_t textureHandle);

    const Vector2& GetPosition() const { return position_; }
    float GetRotation() const { return rotation_; }
    const Vector2& GetSize() const { return size_; }

private:
    // 矩形描画用の頂点バッファを作成する。
    void CreateVertexBuffer();
    // 色情報を渡す定数バッファを作成する。
    void CreateMaterialBuffer();
    // WVP行列を渡す定数バッファを作成する。
    void CreateTransformationMatrixBuffer();

    // サイズ、アンカー、切り抜き範囲をもとに頂点データを再計算する。
    void UpdateVertexData();

private:
    SpriteCommon* spriteCommon_ = nullptr;
    uint32_t textureHandle_ = 0;

    // 2Dトランスフォーム情報。
    Vector2 position_ = { 0.0f, 0.0f };
    float rotation_ = 0.0f;
    Vector2 size_ = { 100.0f, 100.0f };
    Vector2 anchorPoint_ = { 0.0f, 0.0f };

    // テクスチャ切り抜き情報。
    Vector2 textureLeftTop_ = { 0.0f, 0.0f };
    Vector2 textureSize_ = { 100.0f, 100.0f };

    // CPU側の変更をGPUへ再転送する必要があるか。
    bool transferNeeded_ = true; // 頂点データの再転送が必要か

    // DirectXリソース。
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;
};
