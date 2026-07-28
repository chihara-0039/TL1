#include "SkinnedObject.h"
#include "MyMath.h"
#include <algorithm>
#include <cmath>
#include <cctype>

namespace {
// ジョイント名の部分一致検索で大文字小文字を無視するための小文字化。
std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

// このプロジェクトの行列は平行移動を 4 行目に持つため、そこから座標を抜き出す。
Vector3 ExtractTranslation(const Matrix4x4& matrix) {
    return { matrix.m[3][0], matrix.m[3][1], matrix.m[3][2] };
}

// 軸方向が潰れている場合でもデバッグ描画が破綻しないようにする。
Vector3 NormalizeSafe(const Vector3& value, const Vector3& fallback) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.0001f) {
        return fallback;
    }
    return { value.x / length, value.y / length, value.z / length };
}

// 選択中ジョイントのローカル軸を細い棒として描画する。
void DrawAxisRod(
    Object3d& axisObject,
    const Vector3& start,
    const Vector3& direction,
    const Vector4& color,
    float length,
    const Matrix4x4& view,
    const Matrix4x4& projection)
{
    const Vector3 dir = NormalizeSafe(direction, { 0.0f, 1.0f, 0.0f });
    const Vector3 center = {
        start.x + dir.x * length * 0.5f,
        start.y + dir.y * length * 0.5f,
        start.z + dir.z * length * 0.5f
    };
    const float yaw = std::atan2(dir.x, dir.z);
    const float pitch = std::atan2(std::sqrt(dir.x * dir.x + dir.z * dir.z), dir.y);

    axisObject.SetCamera(view, projection);
    axisObject.SetPosition(center);
    axisObject.SetRotation({ pitch, yaw, 0.0f });
    axisObject.SetScale({ 0.008f, length * 0.5f, 0.008f });
    axisObject.SetColor(color);
    axisObject.SetEnableLighting(false);
    axisObject.Update(Math::MakeIdentity4x4());
    axisObject.Draw();
}
}

void SkinnedObject::Initialize(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, TextureManager* textureManager) {
    // 1. スキニングモデルの生成
    skinnedModel_ = std::make_unique<SkinnedModel>();
    skinnedModel_->Initialize(dxCommon, textureManager);

    // 2. 表示用のObject3dを初期化して、SkinnedModel内部のModelを登録
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(skinnedModel_->GetModel());
}

void SkinnedObject::InitializeFromGltf(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, const std::string& filePath, TextureManager* textureManager) {
    // 1. スキニングモデルをglTFから生成
    skinnedModel_ = std::make_unique<SkinnedModel>();
    skinnedModel_->InitializeFromGltf(dxCommon, filePath, textureManager);

    // 2. 表示用のObject3dを初期化して、SkinnedModel内部のModelを登録
    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetModel(skinnedModel_->GetModel());
}

void SkinnedObject::Update(DirectXCommon* dxCommon, const Matrix4x4& lightVP) {
    // 1. CPU側で現在フレームのボーン姿勢を決める。
    // playAnimation_ は組み込みテスト用、playCustomAnimation_ は glTF/自作モーション用。
    if (playAnimation_) {
        // 60FPS想定で時間を進める
        animationTime_ += (1.0f / 60.0f);
        skinnedModel_->ApplyTestAnimation(animationTime_, animationSpeed_);
    } else if (playCustomAnimation_) {
        // カスタムキーフレームモーションの再生
        const float deltaTime = (1.0f / 60.0f) * animationSpeed_;
        animationTime_ += deltaTime;
        float motionDuration = skinnedModel_->GetMotionDuration();
        currentKeyframeTime_ = std::fmod(animationTime_, motionDuration);
        if (currentKeyframeTime_ < 0.0f) {
            currentKeyframeTime_ += motionDuration;
        }

        if (playBlendAnimation_) {
            // モーション切り替え時は、現在のモーションからターゲットへ一定時間で補間する。
            blendElapsed_ += deltaTime;
            blendRate_ = blendDuration_ > 0.0f
                ? std::clamp(blendElapsed_ / blendDuration_, 0.0f, 1.0f)
                : 1.0f;
            skinnedModel_->ApplyMotionBlend(
                blendFromMotionIndex_,
                blendTargetMotionIndex_,
                currentKeyframeTime_,
                blendRate_);

            if (blendRate_ >= 1.0f) {
                skinnedModel_->SetActiveMotionIndex(blendTargetMotionIndex_);
                playBlendAnimation_ = false;
                blendFromMotionIndex_ = blendTargetMotionIndex_;
            }
        } else {
            skinnedModel_->ApplyMotion(currentKeyframeTime_);
        }
    }

    // 2. SkinnedModel 側でボーン行列を更新し、GPU用パレットへ転送する。
    skinnedModel_->Update(dxCommon);

    // 3. 通常の Object3d と同じワールド行列を更新し、描画時の WVP に反映する。
    if (object3d_) {
        object3d_->SetPosition(position_);
        object3d_->SetRotation(rotation_);
        object3d_->SetScale(scale_);
        object3d_->SetCamera(viewMatrix_, projectionMatrix_);
        object3d_->Update(lightVP);
    }
}

void SkinnedObject::Draw() {
    if (!skinnedModel_ || !object3d_) {
        return;
    }
    
    auto commandList = object3d_->GetObject3dCommon()->GetDxCommon()->GetCommandList();

    // 描画直前に Compute Shader でスキニング済み頂点バッファを更新する。
    // 以降の描画パイプラインは通常の Object3d と同じ頂点形式として扱う。
    skinnedModel_->DispatchSkinning(object3d_->GetObject3dCommon()->GetDxCommon());
    
    auto* object3dCommon = object3d_->GetObject3dCommon();
    if (object3dCommon->GetRootSignature()) {
        commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    }
    if (object3dCommon->GetPipelineState()) {
        commandList->SetPipelineState(object3dCommon->GetPipelineState());
    }
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 0. マテリアル
    commandList->SetGraphicsRootConstantBufferView(0, object3d_->GetMaterialResource()->GetGPUVirtualAddress());
    // 1. トランスフォーム
    commandList->SetGraphicsRootConstantBufferView(1, object3d_->GetTransformationResource()->GetGPUVirtualAddress());
    // 2. 平行光源
    commandList->SetGraphicsRootConstantBufferView(2, object3d_->GetObject3dCommon()->GetLightGPUVirtualAddress());
    // 3. テクスチャ
    if (object3d_->GetObject3dCommon()->GetTextureManager()) {
        auto gpuHandle = object3d_->GetObject3dCommon()->GetTextureManager()->GetSrvHandleGPU(skinnedModel_->GetTextureHandle());
        commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);

        auto environmentHandle = object3d_->GetObject3dCommon()->GetTextureManager()->GetSrvHandleGPU(object3d_->GetObject3dCommon()->GetEnvironmentTextureHandle());
        commandList->SetGraphicsRootDescriptorTable(6, environmentHandle);
    }

    const D3D12_VERTEX_BUFFER_VIEW& skinnedVertexBufferView = skinnedModel_->GetSkinnedVertexBufferView();
    commandList->IASetVertexBuffers(0, 1, &skinnedVertexBufferView);
    commandList->DrawInstanced(static_cast<UINT>(skinnedModel_->GetVertexCount()), 1, 0, 0);
}

void SkinnedObject::DrawShadow(const Matrix4x4& lightViewProjection) {
    if (!skinnedModel_ || !object3d_) {
        return;
    }

    // Object3dが保持している共通描画機能から、現在のコマンドリストを取得する。
    auto* object3dCommon = object3d_->GetObject3dCommon();
    auto* dxCommon = object3dCommon ? object3dCommon->GetDxCommon() : nullptr;
    auto* commandList = dxCommon ? dxCommon->GetCommandList() : nullptr;
    if (!commandList || !object3dCommon->GetRootSignature() ||
        !object3dCommon->GetShadowPipelineState()) {
        return;
    }

    // Compute Shaderでアニメーション適用済み頂点を生成する。
    // 出力形式はModelVertexDataなので、位置だけを利用する静的モデル用の影パイプラインと共有できる。
    skinnedModel_->DispatchSkinning(dxCommon);

    commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    commandList->SetPipelineState(object3dCommon->GetShadowPipelineState());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // Object3d::Updateで書き込まれたライト用WVPを、影パスのTransform定数バッファへ設定する。
    commandList->SetGraphicsRootConstantBufferView(
        1, object3d_->GetTransformationResource()->GetGPUVirtualAddress());

    // 元モデルの頂点ではなく、Compute Shaderが更新したスキニング済み頂点を描画する。
    const D3D12_VERTEX_BUFFER_VIEW& skinnedVertexBufferView =
        skinnedModel_->GetSkinnedVertexBufferView();
    commandList->IASetVertexBuffers(0, 1, &skinnedVertexBufferView);
    commandList->DrawInstanced(
        static_cast<UINT>(skinnedModel_->GetVertexCount()), 1, 0, 0);

    // lightViewProjectionはObject3d::Update時点で定数バッファへ反映済み。
    // Object3d::DrawShadowと同じインターフェースを維持するため引数自体は残している。
    (void)lightViewProjection;
}
void SkinnedObject::DrawSkeleton(Object3dCommon* object3dCommon, Model* cubeModel, const Matrix4x4& view, const Matrix4x4& projection) {
    if (!showSkeleton_ || !cubeModel) {
        return;
    }

    const auto& joints = skinnedModel_->GetJoints();
    size_t jointCount = joints.size();

    // ジョイントごとの小さなキューブを必要数だけ作成し、以後は使い回す。
    if (jointVisuals_.size() < jointCount) {
        jointVisuals_.resize(jointCount);
        for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
            jointVisuals_[jointIndex] = std::make_unique<Object3d>();
            jointVisuals_[jointIndex]->Initialize(object3dCommon);
            jointVisuals_[jointIndex]->SetModel(cubeModel);
            jointVisuals_[jointIndex]->SetScale({ 0.04f, 0.04f, 0.04f });
        }
    }

    Matrix4x4 objWorld = Math::MakeAffineMatrix(scale_, rotation_, position_);

    object3dCommon->PreDrawPlayerHighlight();

    // まず各ジョイント位置を点として描画する。選択中ジョイントは緑、それ以外は赤。
    for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
        Matrix4x4 jointWorld = Math::Multiply(joints[jointIndex].globalMatrix, objWorld);

        jointVisuals_[jointIndex]->SetCamera(view, projection);
        
        Vector3 globalPos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };
        jointVisuals_[jointIndex]->SetPosition(globalPos);
        jointVisuals_[jointIndex]->SetRotation({ 0, 0, 0 });
        jointVisuals_[jointIndex]->SetScale({ 0.04f, 0.04f, 0.04f });

        if (static_cast<int>(jointIndex) == selectedJointIndex_) {
            jointVisuals_[jointIndex]->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
        } else {
            jointVisuals_[jointIndex]->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
        }

        jointVisuals_[jointIndex]->SetEnableLighting(false);
        jointVisuals_[jointIndex]->Update(Math::MakeIdentity4x4());
        jointVisuals_[jointIndex]->Draw();
    }

    // 親子ジョイントの間を細い棒で結び、ボーン階層を線のように見せる。
    size_t boneVisualCount = 0;
    for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
        int parentJointIndex = joints[jointIndex].parentIndex;
        if (parentJointIndex == -1) {
            continue;
        }

        if (boneVisuals_.size() <= boneVisualCount) {
            auto boneObj = std::make_unique<Object3d>();
            boneObj->Initialize(object3dCommon);
            boneObj->SetModel(cubeModel);
            boneVisuals_.push_back(std::move(boneObj));
        }

        auto& boneObj = boneVisuals_[boneVisualCount];
        boneVisualCount++;

        Matrix4x4 parentJointWorld = Math::Multiply(joints[parentJointIndex].globalMatrix, objWorld);
        Matrix4x4 childJointWorld = Math::Multiply(joints[jointIndex].globalMatrix, objWorld);

        Vector3 parentPosition = { parentJointWorld.m[3][0], parentJointWorld.m[3][1], parentJointWorld.m[3][2] };
        Vector3 childPosition = { childJointWorld.m[3][0], childJointWorld.m[3][1], childJointWorld.m[3][2] };

        Vector3 parentToChild = Math::Subtract(childPosition, parentPosition);
        float boneLength = std::sqrt(
            parentToChild.x * parentToChild.x +
            parentToChild.y * parentToChild.y +
            parentToChild.z * parentToChild.z);
        if (boneLength < 0.001f) {
            continue;
        }

        Vector3 boneDirection = {
            parentToChild.x / boneLength,
            parentToChild.y / boneLength,
            parentToChild.z / boneLength
        };

        Vector3 centerPos = {
            (parentPosition.x + childPosition.x) * 0.5f,
            (parentPosition.y + childPosition.y) * 0.5f,
            (parentPosition.z + childPosition.z) * 0.5f
        };

        float boneYaw = std::atan2(boneDirection.x, boneDirection.z);
        float bonePitch = std::atan2(
            std::sqrt(boneDirection.x * boneDirection.x + boneDirection.z * boneDirection.z),
            boneDirection.y);

        boneObj->SetCamera(view, projection);
        boneObj->SetPosition(centerPos);
        boneObj->SetRotation({ bonePitch, boneYaw, 0.0f });
        boneObj->SetScale({ 0.015f, boneLength * 0.5f, 0.015f });

        boneObj->SetColor({ 0.9f, 0.9f, 0.5f, 1.0f });
        boneObj->SetEnableLighting(false);

        boneObj->Update(Math::MakeIdentity4x4());
        boneObj->Draw();
    }

    if (showJointAxes_ &&
        selectedJointIndex_ >= 0 &&
        selectedJointIndex_ < static_cast<int>(jointCount)) {
        if (axisVisuals_.size() < 3) {
            axisVisuals_.resize(3);
            for (auto& axis : axisVisuals_) {
                axis = std::make_unique<Object3d>();
                axis->Initialize(object3dCommon);
                axis->SetModel(cubeModel);
            }
        }

        const Matrix4x4 selectedJointWorld =
            Math::Multiply(joints[static_cast<size_t>(selectedJointIndex_)].globalMatrix, objWorld);
        const Vector3 jointPos = ExtractTranslation(selectedJointWorld);
        const Vector3 localX = { selectedJointWorld.m[0][0], selectedJointWorld.m[0][1], selectedJointWorld.m[0][2] };
        const Vector3 localY = { selectedJointWorld.m[1][0], selectedJointWorld.m[1][1], selectedJointWorld.m[1][2] };
        const Vector3 localZ = { selectedJointWorld.m[2][0], selectedJointWorld.m[2][1], selectedJointWorld.m[2][2] };

        DrawAxisRod(*axisVisuals_[0], jointPos, localX, { 1.0f, 0.15f, 0.15f, 1.0f }, 0.28f, view, projection);
        DrawAxisRod(*axisVisuals_[1], jointPos, localY, { 0.15f, 1.0f, 0.15f, 1.0f }, 0.28f, view, projection);
        DrawAxisRod(*axisVisuals_[2], jointPos, localZ, { 0.20f, 0.35f, 1.0f, 1.0f }, 0.28f, view, projection);
    }

    object3dCommon->PreDraw();
}

int SkinnedObject::FindJointIndexByNameHints(const std::vector<std::string>& nameHints) const {
    if (!skinnedModel_) {
        return -1;
    }

    const auto& joints = skinnedModel_->GetJoints();
    for (size_t i = 0; i < joints.size(); ++i) {
        const std::string jointName = ToLower(joints[i].name);
        for (const std::string& hint : nameHints) {
            if (hint.empty()) {
                continue;
            }

            const std::string loweredHint = ToLower(hint);
            if (jointName.find(loweredHint) != std::string::npos) {
                return static_cast<int>(i);
            }
        }
    }

    return -1;
}

bool SkinnedObject::TryGetJointWorldPosition(int jointIndex, Vector3& outPosition) const {
    if (!skinnedModel_) {
        return false;
    }

    const auto& joints = skinnedModel_->GetJoints();
    if (jointIndex < 0 || jointIndex >= static_cast<int>(joints.size())) {
        return false;
    }

    const Matrix4x4 objWorld = Math::MakeAffineMatrix(scale_, rotation_, position_);
    const Matrix4x4 jointWorld =
        Math::Multiply(joints[static_cast<size_t>(jointIndex)].globalMatrix, objWorld);
    outPosition = ExtractTranslation(jointWorld);
    return true;
}

bool SkinnedObject::TryGetJointWorldPosition(
    const std::vector<std::string>& nameHints,
    Vector3& outPosition) const
{
    const int jointIndex = FindJointIndexByNameHints(nameHints);
    return TryGetJointWorldPosition(jointIndex, outPosition);
}

void SkinnedObject::StartMotionBlend(int targetMotionIndex, float duration) {
    if (!skinnedModel_) {
        return;
    }

    const auto& motions = skinnedModel_->GetMotions();
    int activeMotionIndex = skinnedModel_->GetActiveMotionIndex();
    if (targetMotionIndex < 0 || targetMotionIndex >= static_cast<int>(motions.size()) ||
        activeMotionIndex < 0 || activeMotionIndex >= static_cast<int>(motions.size()) ||
        targetMotionIndex == activeMotionIndex) {
        return;
    }

    // Blend starts from the currently active animation and gradually reaches the selected target.
    blendFromMotionIndex_ = activeMotionIndex;
    blendTargetMotionIndex_ = targetMotionIndex;
    blendDuration_ = duration < 0.01f ? 0.01f : duration;
    blendElapsed_ = 0.0f;
    blendRate_ = 0.0f;
    playBlendAnimation_ = true;
    playCustomAnimation_ = true;
    playAnimation_ = false;
}

