#include "StageMapGimmickSystem.h"

#include "StageMap.h"

void StageMapGimmickSystem::SetPSwitchActive(StageMap& stageMap, int switchId) {
    stageMap.isPSwitchActive_ = true;
    stageMap.needsRebuild_ = true;

    // 同じvariantを持つPスイッチ/Pブロックだけを対象にして、別IDのギミックに影響させない。
    for (auto& cell : stageMap.cells_) {
        if (cell.type == BlockType::PSwitch && cell.variant == switchId) {
            cell.isSolid = false;
            cell.isHidden = true;
        }

        if (cell.type == BlockType::PBlock && cell.variant == switchId) {
            cell.isSolid = false;
            cell.isHidden = true;
        }

        if (cell.type == BlockType::PBlockAppears && cell.variant == switchId) {
            cell.isSolid = true;
        }
    }
}

// Pスイッチの状態をリセットする（再構築なし）
void StageMapGimmickSystem::ResetPSwitchStateNoRebuild(StageMap& stageMap) {
    stageMap.isPSwitchActive_ = false;

    // リトライやステージ遷移時に、Pスイッチ系セルを初期状態へ戻す。
    for (auto& cell : stageMap.cells_) {
        if (cell.type == BlockType::PSwitch) {
            cell.isSolid = false;
            cell.isHidden = false;
        }

        if (cell.type == BlockType::PBlock) {
            cell.isSolid = true;
            cell.isHidden = false;
        }

        if (cell.type == BlockType::PBlockAppears) {
            cell.isSolid = false;
        }
    }

    stageMap.needsRebuild_ = false;
}

void StageMapGimmickSystem::ResetPSwitchState(StageMap& stageMap) {
    // 見た目側の再構築を伴わない、フラグだけの軽量リセット。
    stageMap.isPSwitchActive_ = false;
}

void StageMapGimmickSystem::ToggleOnState(StageMap& stageMap) {
    stageMap.isOnState_ = !stageMap.isOnState_;

    // ON/OFFブロックは片方だけが当たり判定を持つよう、状態を対で切り替える。
    for (auto& cell : stageMap.cells_) {
        if (cell.type == BlockType::OnBlock) {
            cell.isSolid = stageMap.isOnState_;
        }
        if (cell.type == BlockType::OffBlock) {
            cell.isSolid = !stageMap.isOnState_;
        }
    }

    // 当たり判定と見た目の反映が必要なので、StageRenderer側の再構築対象にする。
    stageMap.needsRebuild_ = true;
}
