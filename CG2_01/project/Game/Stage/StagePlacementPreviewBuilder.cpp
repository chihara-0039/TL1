#include "StagePlacementPreviewBuilder.h"

#include <cmath>
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"

namespace {
// プレビュー用のスタイル情報をまとめる構造体
struct PreviewStyle {
    Vector4 color{ 1.0f, 1.0f, 1.0f, 0.4f };
    Model* model = nullptr;
    bool canPreview = true;
};

// カーソル位置のブロック種別とカスタムパーツIDに応じて、プレビュー用の色とモデルを決定する。
PreviewStyle ResolvePreviewStyle(
    const StageMap& stageMap,
    const StagePlacementPreviewBuilder::Models& models,
    BlockType type,
    int customId) {

    PreviewStyle style;
    style.model = models.wall;

    // カスタムパーツは登録済みスロットの色とベースモデルを優先して使う。
    if (customId >= 1 && customId <= 5) {
        const auto* part = stageMap.GetCustomPart(customId);
        if (part) {
            style.color = { part->colorR, part->colorG, part->colorB, 0.4f };
            style.model = (part->baseType == BlockType::Ladder) ? models.ladder : models.wall;
        }
        return style;
    }

    // 通常ブロックは配置予定の種類ごとに、実体と同じモデルか識別しやすい半透明色を選ぶ。
    switch (type) {
    case BlockType::Wall:
        style.color = { 1.0f, 0.4f, 0.4f, 0.4f };
        style.model = models.wall;
        break;
    case BlockType::TransparentBlock:
        style.color = { 1.0f, 1.0f, 1.0f, 0.4f };
        style.model = models.wall;
        break;
    case BlockType::Ladder:
        style.color = { 0.4f, 1.0f, 0.4f, 0.4f };
        style.model = models.ladder;
        break;
    case BlockType::Ground:
        style.color = { 0.7f, 0.7f, 0.7f, 0.4f };
        style.model = models.ground;
        break;
    case BlockType::IceBlock:
        style.color = { 0.5f, 0.85f, 1.0f, 0.4f };
        style.model = models.iceBlock;
        break;
    case BlockType::MovingFloor:
        style.color = { 0.9f, 0.65f, 0.4f, 0.4f };
        style.model = models.movingFloor;
        break;
    case BlockType::CrumblingFloor:
        style.color = { 0.8f, 0.6f, 0.4f, 0.4f };
        style.model = models.crumble;
        break;
    default:
        style.canPreview = false;
        break;
    }

    return style;
}

// プレビュー用の一時オブジェクトを作成するヘルパー関数
std::unique_ptr<Object3d> CreatePreviewObject(
    Object3dCommon* object3dCommon,
    Model* model,
    const Vector3& position,
    const Vector3& rotation,
    const Vector3& scale,
    const Vector4& color) {

    // プレビュー専用の一時オブジェクトを作成し、呼び出し側の previewObjects に所有権を渡す。
    auto previewObject = std::make_unique<Object3d>();
    previewObject->Initialize(object3dCommon);
    previewObject->SetModel(model);
    previewObject->SetPosition(position);
    previewObject->SetRotation(rotation);
    previewObject->SetScale(scale);
    previewObject->SetColor(color);
    return previewObject;
}
}

// カーソル位置周辺の配置可否とブロック種別に基づき、previewObjectsを作り直す。
void StagePlacementPreviewBuilder::Build(
    std::vector<std::unique_ptr<Object3d>>& previewObjects,
    Object3dCommon* object3dCommon,
    const Models& models,
    const Vector3& blockScale,
    const StageMap& stageMap,
    const Int3& cursorIndex,
    BlockType type,
    int customId,
    float rotationY) {

    // プレビューはカーソル移動や選択変更のたびに作り直すため、前回分を先に消す。
    previewObjects.clear();

    PreviewStyle style = ResolvePreviewStyle(stageMap, models, type, customId);
    if (!style.canPreview || !style.model) {
        // 未対応ブロックやモデル未設定の場合は、古いプレビューを消した状態で終了する。
        return;
    }

	// カスタムパーツは3x3x3の形状を回転させて表示する。
    if (customId >= 1 && customId <= 5) {
        const auto* part = stageMap.GetCustomPart(customId);
        if (part && !part->IsEmpty()) {
            // カスタムパーツは3x3x3のローカルセルを、90度単位の回転後座標へ変換して表示する。
            int rotationQuarterTurns = static_cast<int>(std::round(rotationY / 1.5707963f)) % 4;
            if (rotationQuarterTurns < 0) {
                rotationQuarterTurns += 4;
            }

			// 3x3x3の各セルを回転後の座標に変換して、半透明プレビューを作る。
            for (int localY = 0; localY < 3; ++localY) {
                for (int localZ = 0; localZ < 3; ++localZ) {
                    for (int localX = 0; localX < 3; ++localX) {
                        const auto& cell = part->cells[localY][localZ][localX];
                        if (cell.type == BlockType::None) {
                            continue;
                        }

                        // パーツセルごとに回転後の表示位置と向きを決定する。
                        int rotatedX = localX;
                        int rotatedZ = localZ;
                        float cellRotationY = 0.0f;
                        if (rotationQuarterTurns == 1) {
                            rotatedX = 2 - localZ;
                            rotatedZ = localX;
                            cellRotationY = 1.5707963f;
                        } else if (rotationQuarterTurns == 2) {
                            rotatedX = 2 - localX;
                            rotatedZ = 2 - localZ;
                            cellRotationY = 3.1415927f;
                        } else if (rotationQuarterTurns == 3) {
                            rotatedX = localZ;
                            rotatedZ = 2 - localX;
                            cellRotationY = 4.712389f;
                        }

						// カスタムパーツのセルを回転後の座標に配置する。
                        Vector3 previewPosition = {
                            static_cast<float>(cursorIndex.x + rotatedX),
                            static_cast<float>(cursorIndex.y + localY),
                            static_cast<float>(cursorIndex.z + rotatedZ)
                        };

						// セルの種類に応じて、壁かハシゴのモデルを選択する。
                        Model* cellModel = (cell.type == BlockType::Ladder) ? models.ladder : models.wall;
                        if (!cellModel) {
                            continue;
                        }

						// プレビュー用の半透明オブジェクトを作成してリストに追加する。
                        previewObjects.push_back(CreatePreviewObject(
                            object3dCommon,
                            cellModel,
                            previewPosition,
                            { 1.57f, cellRotationY, 0.0f },
                            blockScale,
                            style.color));
                    }
                }
            }
            return;
        }
    }

    // 通常ブロックはカーソル位置に1つだけ半透明プレビューを出す。
    Vector3 previewPosition = {
        static_cast<float>(cursorIndex.x),
        static_cast<float>(cursorIndex.y),
        static_cast<float>(cursorIndex.z)
    };

	// プレビュー用の半透明オブジェクトを作成してリストに追加する。
    previewObjects.push_back(CreatePreviewObject(
        object3dCommon,
        style.model,
        previewPosition,
        { 1.57f, rotationY, 0.0f },
        blockScale,
        style.color));
}
