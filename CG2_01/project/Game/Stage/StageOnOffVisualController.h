#pragma once
#include <memory>
#include <vector>
#include "StageMap.h"

class Object3d;

// ON/OFFスイッチ状態に応じたスイッチとブロックの表示色を決定する。
class StageOnOffVisualController {
public:
    // StageMapと描画オブジェクトの走査順が一致している前提で、色を変更したオブジェクトを返す。
    static std::vector<Object3d*> Apply(
        const StageMap& stageMap,
        const std::vector<std::unique_ptr<Object3d>>& objects);
};
