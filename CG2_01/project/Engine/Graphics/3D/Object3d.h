#pragma once
#include "Object3dCommon.h"
#include "Model.h"
#include "MyMath.h"

// Constant buffer data bound to Object3d.VS.hlsl b0.
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 lightViewProjection;
    Matrix4x4 WorldInverseTranspose;
};

// Material constants bound to Object3d.PS.hlsl.
struct Material {
    Vector4   color;
    int32_t   enableLighting;
    float     shininess;
    float     metallic;
    float     emissive;
    Matrix4x4 uvTransform;
    float     environmentCoefficient;
};

// Renderable 3D object instance.
// Object3d owns per-object transform/material buffers. Shared PSO and root
// signature state are owned by Object3dCommon.
class Object3d {
public:
    // Creates constant buffers and writes default material values.
    void Initialize(Object3dCommon* object3dCommon);

    // Updates world, WVP, and light-view-projection matrices.
    void Update(const Matrix4x4& lightVP);

    // Draws the assigned model with the current transform and material.
    void Draw();

    // Sets a non-owning model pointer. The caller must keep the model alive.
    void SetModel(Model* model) { model_ = model; }

    void SetPosition(const Vector3& position) { transform_.translate = position; }
    void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    // Draws depth for the shadow-map pass.
    void DrawShadow(const Matrix4x4& lightViewProjection);

    // Sets the camera matrices used by the next Update().
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_       = view;
        projectionMatrix_ = projection;
    }

    void SetColor(const Vector4& color) { if (materialData_) materialData_->color = color; }
    void SetEnableLighting(bool enable) { if (materialData_) materialData_->enableLighting = (enable ? 1 : 0); }
    void SetShininess(float shininess) { if (materialData_) materialData_->shininess = shininess; }
    void SetMetallic(float metallic) { if (materialData_) materialData_->metallic = metallic; }
    void SetEmissive(float emissive) { if (materialData_) materialData_->emissive = emissive; }
    void SetEnvironmentCoefficient(float coefficient) { if (materialData_) materialData_->environmentCoefficient = coefficient; }
    void SetUVTransform(const Transform& uvTransform);

    Model* GetModel() const { return model_; }
    const Transform& GetTransform() const { return transform_; }
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
    const Material& GetMaterial() const { return *materialData_; }
    const Vector3& GetPosition() const { return transform_.translate; }

    ID3D12Resource* GetTransformationResource() const { return transformationResource_.Get(); }
    ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
    Object3dCommon* GetObject3dCommon() const { return object3dCommon_; }

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Model*          model_          = nullptr;

    Transform transform_ = { {1,1,1}, {0,0,0}, {0,0,0} };

    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
    TransformationMatrix* transformationData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};
