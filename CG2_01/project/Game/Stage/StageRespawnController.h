#pragma once

#include "StageMap.h"
#include "StageRenderer.h"
#include "Player.h"
#include "../Block/BlockInventory.h"
#include "../Block/BubblePickupController.h"
#include "../Block/BlockPlacementController.h"
#include "StageEditorController.h"

// 落下などでリスポーンが必要になったとき、表示中のステージを維持してプレイヤー状態を戻す。
class StageRespawnController
{
public:
    // プレイヤーの落下を監視し、必要ならバックアップMapからステージを復元する。
    void Update(
        StageMap& stageMap,
        const StageMap& backupMap,
        StageRenderer* stageRenderer,
        Player* player,
        BlockInventory* blockInventory,
        BubblePickupController* bubblePickupController,
        BlockPlacementController* blockPlacementController,
        StageEditorController* stageEditorController,
        bool restoreStageMap = true
    );

private:
    // このY座標を下回ったら落下扱いにする。
    static constexpr float kFallY = -10.0f;

    // 連続フレームで二重にリスポーン処理が走るのを防ぐ。
    bool isRespawning_ = false;
};

