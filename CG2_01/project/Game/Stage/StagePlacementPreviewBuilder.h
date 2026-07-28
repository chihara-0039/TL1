#pragma once
#include <memory>
#include <vector>
#include "StageMap.h"

class Model;
class Object3d;
class Object3dCommon;

// ブロック配置時に表示する半透明プレビューオブジェクトを生成する。
class StagePlacementPreviewBuilder {
public:
    // プレビュー生成に必要なモデルをまとめて渡し、StageRendererのモデル所有は維持する。
    struct Models {
        Model* ground = nullptr;
        Model* wall = nullptr;
        Model* ladder = nullptr;
        Model* crumble = nullptr;
        Model* iceBlock = nullptr;
        Model* movingFloor = nullptr;
    };

    // cursorIndex周辺の配置可否とブロック種別に基づき、previewObjectsを作り直す。
    static void Build(
        std::vector<std::unique_ptr<Object3d>>& previewObjects,
        Object3dCommon* object3dCommon,
        const Models& models,
        const Vector3& blockScale,
        const StageMap& stageMap,
        const Int3& cursorIndex,
        BlockType type,
        int customId,
        float rotationY);
};
