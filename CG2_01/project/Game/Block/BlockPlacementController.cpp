#include "BlockPlacementController.h"

#include <Windows.h>
#include <cassert>
#include <cmath>

void BlockPlacementController::Initialize(
    StageMap* stageMap,
    StageRenderer* stageRenderer,
    BlockInventory* inventory
) {
    assert(stageMap);
    assert(stageRenderer);
    assert(inventory);

    stageMap_ = stageMap;
    stageRenderer_ = stageRenderer;
    inventory_ = inventory;
    placeCustomId_ = 0;
}

// 現在選択中のブロックまたはカスタムパーツを、指定グリッド位置へ配置する。
bool BlockPlacementController::TryPlace(const Int3& index, float rotationY) {
    if (!stageMap_ || !stageRenderer_ || !inventory_) {
        return false;
    }

    // カスタムパーツは3x3x3の複合ブロックとして、空いているセルへ一括配置する。
    if (placeCustomId_ >= 1 && placeCustomId_ <= 5) {
        const auto* part = stageMap_->GetCustomPart(placeCustomId_);
        if (part && !part->IsEmpty()) {
            if (!inventory_->HasBlock(placeBlockType_, placeCustomId_)) {
                OutputDebugStringA("[BlockPlacementController] No custom assembly in inventory.\n");
                return false;
            }

            int rotationQuarterTurns = static_cast<int>(std::round(rotationY / 1.5707963f)) % 4;
            if (rotationQuarterTurns < 0) {
                rotationQuarterTurns += 4;
            }

            bool placedAnyCell = false;
            for (int localY = 0; localY < 3; ++localY) {
                for (int localZ = 0; localZ < 3; ++localZ) {
                    for (int localX = 0; localX < 3; ++localX) {
                        const auto& cell = part->cells[localY][localZ][localX];
                        if (cell.type == BlockType::None) {
                            continue;
                        }

                        // 3x3パーツのローカル座標を、90度単位の回転後座標へ変換する。
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

                        // 既存ブロックを上書きせず、空いているセルだけ配置する。
                        Int3 targetIndex = { index.x + rotatedX, index.y + localY, index.z + rotatedZ };
                        if (stageMap_->IsInside(targetIndex)) {
                            const MapCell* targetCell = stageMap_->GetCell(targetIndex);
                            if (targetCell && targetCell->type == BlockType::None) {
                                stageMap_->SetBlock(targetIndex, cell.type, placeCustomId_);
                                MapCell* placedCell = stageMap_->GetCell(targetIndex);
                                if (placedCell) {
                                    placedCell->rotationY = cellRotationY;
                                }
                                placedAnyCell = true;
                            }
                        }
                    }
                }
            }

            if (!placedAnyCell) {
                OutputDebugStringA("[BlockPlacementController] Assembly placement failed: no available empty spaces.\n");
                return false;
            }

            inventory_->ConsumeBlock(placeBlockType_, 1, placeCustomId_);
            stageRenderer_->BuildFromStageMap(*stageMap_);
            OutputDebugStringA("[BlockPlacementController] Custom assembly placed successfully.\n");
            return true;
        }
    }

    // 通常ブロックは1マスだけ配置する。Groundは基本ブロック扱いなので在庫消費しない。
    if (placeBlockType_ != BlockType::Ground) {
        if (!inventory_->HasBlock(placeBlockType_, placeCustomId_)) {
            OutputDebugStringA("[BlockPlacementController] No block of this type/customId in inventory.\n");
            return false;
        }
    }

    if (!CanPlaceAt(index)) {
        OutputDebugStringA("[BlockPlacementController] Cannot place here.\n");
        return false;
    }

    // 通常配置のWall/Ladderは、プレイヤー配置由来だと分かる専用variantを付ける。
    int finalVariant = placeCustomId_;
    if (placeCustomId_ == 0) {
        if (placeBlockType_ == BlockType::Wall) {
            finalVariant = 6;
        } else if (placeBlockType_ == BlockType::Ladder) {
            finalVariant = 7;
        }
    }

    if (!stageMap_->SetBlock(index, placeBlockType_, finalVariant)) {
        return false;
    }

    MapCell* placedCell = stageMap_->GetCell(index);
    if (placedCell) {
        placedCell->rotationY = rotationY;
    }

    if (placeBlockType_ != BlockType::Ground) {
        inventory_->ConsumeBlock(placeBlockType_, 1, placeCustomId_);
    }

    stageRenderer_->BuildFromStageMap(*stageMap_);
    OutputDebugStringA("[BlockPlacementController] Block placed.\n");
    return true;
}

// 指定セルがステージ範囲内かつ空セルなら配置可能とする。
bool BlockPlacementController::CanPlaceAt(const Int3& index) const {
    const MapCell* cell = stageMap_->GetCell(index);
    if (!cell) {
        return false;
    }

    if (cell->type != BlockType::None) {
        return false;
    }

    return true;
}
