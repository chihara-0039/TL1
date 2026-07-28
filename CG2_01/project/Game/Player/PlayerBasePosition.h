#pragma once
#include "StageMap.h"

class Player;

// StageMapからプレイヤー開始位置を解決し、Playerへ反映する。
class PlayerBasePosition {
public:
    // PlayerStartセルを探して位置とインデックスを更新する。見つからない場合はfalseを返す。
    bool ApplyFromStageMap(const StageMap& stageMap, Player* player);

    const Vector3& GetPosition() const { return position_; }
    const Int3& GetIndex() const { return index_; }

private:
    Vector3 position_{ 0.0f, 1.5f, 0.0f };
    Int3 index_{ 0, 0, 0 };
};
