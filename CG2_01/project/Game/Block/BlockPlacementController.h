#pragma once

#include "StageMap.h"
#include "StageRenderer.h"
#include "BlockInventory.h"

// 所持ブロックをステージ上へ配置するための制御クラス。
class BlockPlacementController {
public:
    // 操作対象のマップ、描画、所持数管理を外部から注入する。
    void Initialize(
        StageMap* stageMap,
        StageRenderer* stageRenderer,
        BlockInventory* inventory
    );

    // 指定セルに現在選択中のブロックを配置する。成功時はインベントリも消費する。
    bool TryPlace(const Int3& index, float rotationY = 0.0f);

    // UI側で選択された配置ブロック種別とカスタムIDを保持する。
    void SetPlaceBlockType(BlockType type) { placeBlockType_ = type; }
    void SetPlaceCustomId(int id) { placeCustomId_ = id; }
    int GetPlaceCustomId() const { return placeCustomId_; }

private:
    // 配置先セルが空で、ステージ範囲内かを判定する。
    bool CanPlaceAt(const Int3& index) const;

private:
    StageMap* stageMap_ = nullptr;
    StageRenderer* stageRenderer_ = nullptr;
    BlockInventory* inventory_ = nullptr;

    // 初期状態では通常地面ブロックを配置対象にする。
    BlockType placeBlockType_ = BlockType::Ground;
    int placeCustomId_ = 0; // 0: 通常ブロック, 1～5: カスタムパーツ
};
