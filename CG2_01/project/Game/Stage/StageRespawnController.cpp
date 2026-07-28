#include "StageRespawnController.h"
#include <algorithm>
#include <cmath>

namespace {
	// ステージマップ上の座標が有効なリスポーン位置かどうかを判定する。
bool IsUsableGridRespawn(const StageMap& stageMap, const Vector3& position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
        position.y < -9.0f) {
        return false;
    }

	// ステージマップの範囲内かどうかを確認する。
    const int x = static_cast<int>(std::floor(position.x + 0.5f));
    const int z = static_cast<int>(std::floor(position.z + 0.5f));
    if (x < 0 || x >= stageMap.GetWidth() || z < 0 || z >= stageMap.GetDepth()) {
        return false;
    }

    // スポーン地点の下に床があることを確認する。中継地点の高さ違いにも対応して下方向を探索する。
    const int startY = (std::min)(stageMap.GetHeight() - 1, static_cast<int>(std::floor(position.y)));
    for (int y = startY; y >= 0; --y) {
        const MapCell* cell = stageMap.GetCell(x, y, z);
        if (cell && cell->isSolid) {
            return true;
        }
    }
    return false;
}
}

// ステージの落下リスポーン処理を行う。プレイヤーが落下した場合、チェックポイント位置に戻す。
void StageRespawnController::Update(
    StageMap& stageMap,
    const StageMap& backupMap,
    StageRenderer* stageRenderer,
    Player* player,
    BlockInventory* blockInventory,
    BubblePickupController* bubblePickupController,
    BlockPlacementController* blockPlacementController,
    StageEditorController* stageEditorController,
    bool restoreStageMap
) {
	// プレイヤーが存在しない場合は何もしない。
    if (!player) {
        return;
    }

	// プレイヤーが落下していない場合は何もしない。
    if (player->GetPosition().y >= kFallY) {
        isRespawning_ = false;
        return;
    }

	// すでにリスポーン処理中の場合は何もしない。
    if (isRespawning_) {
        return;
    }

    isRespawning_ = true;

  // ==================================================
  // 今のチェックポイント位置を保存
  // ==================================================
    Vector3 currentRespawnPos = player->GetRespawnPosition();

    // 表示中のStageMap自体は差し替えない。
    // バックアップが空・古い場合にステージ全体が消える事故を防ぎ、
    // 落下時はプレイヤーと関連するゲームプレイ状態だけを戻す。
    (void)backupMap;
    if (restoreStageMap) {
        if (blockInventory) {
            blockInventory->Initialize(0);
        }

        if (bubblePickupController && stageRenderer && blockInventory) {
            bubblePickupController->Initialize(&stageMap, stageRenderer, blockInventory);
        }

        if (blockPlacementController && stageRenderer && blockInventory) {
            blockPlacementController->Initialize(&stageMap, stageRenderer, blockInventory);
        }
    }

    // ==================================================
   // チェックポイント位置は維持する
   // ただし鍵は必ずリセットする
   // ==================================================
    // 通常ステージでは、壊れた中継地点やステージ外座標を採用しない。
    if (restoreStageMap && !IsUsableGridRespawn(stageMap, currentRespawnPos)) {
        if (stageEditorController) {
            stageEditorController->ResetPlayerToStartCell(stageMap, player);
            currentRespawnPos = player->GetRespawnPosition();
        } else {
            currentRespawnPos = { 0.0f, 1.5f, 0.0f };
        }
    }
    player->SetRespawnPosition(currentRespawnPos);
    player->SetHasKey(false);
    player->Respawn();

    // 無効座標が残っても永久にisRespawning_へ閉じ込めない。
    if (player->GetPosition().y < kFallY) {
        player->SetPosition({ 0.0f, 1.5f, 0.0f });
        player->SetRespawnPosition({ 0.0f, 1.5f, 0.0f });
    }
}
