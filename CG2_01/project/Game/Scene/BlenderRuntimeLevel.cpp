#include "BlenderRuntimeLevel.h"

#include "DirectXCommon.h"
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "TextureManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>

namespace {
/// <summary>行ベクトル規約の行列を使い、ローカル座標をワールド座標へ変換する。</summary>
Vector3 TransformPoint(const Vector3& point, const Matrix4x4& matrix) {
    return {
        point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
        point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
        point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2]
    };
}
} // namespace

// Model/Object3dの完全型が見える翻訳単位で所有リソースを構築・破棄する。
BlenderRuntimeLevel::BlenderRuntimeLevel() = default;
BlenderRuntimeLevel::~BlenderRuntimeLevel() = default;

void BlenderRuntimeLevel::Initialize(
    Object3dCommon* object3dCommon,
    DirectXCommon* dxCommon,
    TextureManager* textureManager) {
    object3dCommon_ = object3dCommon;
    dxCommon_ = dxCommon;
    textureManager_ = textureManager;
}

bool BlenderRuntimeLevel::Load(const std::string& filePath) {
    // 再読み込み時に古いレベル情報が混ざらないよう、最初に全状態を破棄する。
    objects_.clear();
    collisionBoxes_.clear();
    hasPlayerSpawn_ = false;

    if (!object3dCommon_ || !dxCommon_ || !textureManager_) {
        status_ = "Blender level load failed: renderer is not initialized.";
        return false;
    }

    LevelData levelData;
    if (!LevelDataLoader::Load(filePath, levelData, &status_)) {
        return false;
    }

    bool loadedAnyNode = false;
    for (const LevelObjectData& objectData : levelData.objects) {
        loadedAnyNode = AppendObjectRecursive(objectData) || loadedAnyNode;
    }

    status_ = "Blender level: " + levelData.name +
        " (objects=" + std::to_string(objects_.size()) +
        ", colliders=" + std::to_string(collisionBoxes_.size()) +
        ", playerSpawn=" + (hasPlayerSpawn_ ? "yes" : "no") + ")";
    return loadedAnyNode || hasPlayerSpawn_ || !collisionBoxes_.empty();
}

void BlenderRuntimeLevel::Update(
    const Matrix4x4& view,
    const Matrix4x4& projection,
    const Matrix4x4& lightViewProjection) {
    for (RuntimeObject& runtimeObject : objects_) {
        if (!runtimeObject.object) {
            continue;
        }
        runtimeObject.object->SetCamera(view, projection);
        runtimeObject.object->Update(lightViewProjection);
    }
}

void BlenderRuntimeLevel::Draw() {
    for (RuntimeObject& runtimeObject : objects_) {
        if (runtimeObject.object) {
            runtimeObject.object->Draw();
        }
    }
}

void BlenderRuntimeLevel::DrawShadow(const Matrix4x4& lightViewProjection) {
    for (RuntimeObject& runtimeObject : objects_) {
        if (runtimeObject.object) {
            runtimeObject.object->DrawShadow(lightViewProjection);
        }
    }
}

bool BlenderRuntimeLevel::AppendObjectRecursive(const LevelObjectData& objectData) {
    bool registeredAnyData = false;

    // disabledは描画・物理・スポーンのすべてをまとめて無効化する。
    if (!objectData.disabled) {
        // 空文字は旧データとの互換用としてBOX扱いにする。
        if (objectData.collider.enabled &&
            (objectData.collider.type.empty() || objectData.collider.type == "BOX")) {
            collisionBoxes_.push_back(MakeWorldCollisionBox(objectData.transform, objectData.collider));
            registeredAnyData = true;
        }

        // 複数のPlayerスポーンがある場合は、階層を先頭から走査して最初の一個を採用する。
        if (!hasPlayerSpawn_ && objectData.spawnPoint.enabled &&
            IsPlayerSpawnType(objectData.spawnPoint.type)) {
            playerSpawn_ = objectData.transform.translate;
            hasPlayerSpawn_ = true;
            registeredAnyData = true;
        }

        // 現在の通常ゲーム配置は静的OBJだけを対象とする。
        if (objectData.type == "MESH") {
            const std::string modelPath = ResolveModelPath(objectData.fileName);
            const std::filesystem::path path(modelPath);
            if (!modelPath.empty() && path.extension() == ".obj" && std::filesystem::exists(path)) {
                RuntimeObject runtimeObject;
                runtimeObject.transform = objectData.transform;
                runtimeObject.model = Model::CreateFromOBJ(
                    dxCommon_,
                    path.parent_path().generic_string(),
                    path.filename().generic_string(),
                    textureManager_);

                if (runtimeObject.model) {
                    runtimeObject.object = std::make_unique<Object3d>();
                    runtimeObject.object->Initialize(object3dCommon_);
                    runtimeObject.object->SetModel(runtimeObject.model.get());
                    runtimeObject.object->SetPosition(runtimeObject.transform.translate);
                    runtimeObject.object->SetRotation(runtimeObject.transform.rotate);
                    runtimeObject.object->SetScale(runtimeObject.transform.scale);
                    objects_.push_back(std::move(runtimeObject));
                    registeredAnyData = true;
                }
            }
        }
    }

    // LevelDataLoaderが親Transformを子へ合成済みなので、ここでは階層を平坦化して所有する。
    for (const LevelObjectData& child : objectData.children) {
        registeredAnyData = AppendObjectRecursive(child) || registeredAnyData;
    }
    return registeredAnyData;
}

std::string BlenderRuntimeLevel::ResolveModelPath(const std::string& fileName) {
    if (fileName.empty()) {
        return {};
    }

    const std::filesystem::path path(fileName);
    if (path.has_parent_path()) {
        return path.generic_string();
    }

    const std::string stem = path.stem().string();
    const std::filesystem::path candidates[] = {
        std::filesystem::path("Resources/Models") / stem / (stem + ".obj"),
        std::filesystem::path("Resources/Models") / (stem + ".obj"),
        std::filesystem::path("Resources/Models") / stem / fileName,
    };
    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.generic_string();
        }
    }
    return fileName;
}

bool BlenderRuntimeLevel::IsPlayerSpawnType(std::string type) {
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return type.empty() || type == "player" || type == "playerstart" ||
        type == "player_start" || type == "player spawn" || type == "playerspawn";
}

WorldCollisionBox BlenderRuntimeLevel::MakeWorldCollisionBox(
    const Transform& transform,
    const LevelColliderData& collider) {
    const Matrix4x4 world = Math::MakeAffineMatrix(
        transform.scale,
        transform.rotate,
        transform.translate);
    const Vector3 halfSize = {
        std::abs(collider.size.x) * 0.5f,
        std::abs(collider.size.y) * 0.5f,
        std::abs(collider.size.z) * 0.5f
    };

    WorldCollisionBox result;
    const float highest = (std::numeric_limits<float>::max)();
    result.minimum = { highest, highest, highest };
    result.maximum = { -highest, -highest, -highest };

    // 回転後のBOXを現在のAABB物理へ安全に渡すため、全8頂点を包む範囲を求める。
    for (int zSign = -1; zSign <= 1; zSign += 2) {
        for (int ySign = -1; ySign <= 1; ySign += 2) {
            for (int xSign = -1; xSign <= 1; xSign += 2) {
                const Vector3 localPoint = {
                    collider.center.x + halfSize.x * static_cast<float>(xSign),
                    collider.center.y + halfSize.y * static_cast<float>(ySign),
                    collider.center.z + halfSize.z * static_cast<float>(zSign)
                };
                const Vector3 worldPoint = TransformPoint(localPoint, world);
                result.minimum.x = (std::min)(result.minimum.x, worldPoint.x);
                result.minimum.y = (std::min)(result.minimum.y, worldPoint.y);
                result.minimum.z = (std::min)(result.minimum.z, worldPoint.z);
                result.maximum.x = (std::max)(result.maximum.x, worldPoint.x);
                result.maximum.y = (std::max)(result.maximum.y, worldPoint.y);
                result.maximum.z = (std::max)(result.maximum.z, worldPoint.z);
            }
        }
    }
    return result;
}
