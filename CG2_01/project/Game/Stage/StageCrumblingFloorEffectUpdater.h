#pragma once

#include <memory>
#include <vector>

class Object3d;
class StageMap;

// StageMap上の崩れる床セルを参照し、対応する描画オブジェクトの色と透明度を更新する。
class StageCrumblingFloorEffectUpdater {
public:
    // 変更したオブジェクトのみ返し、StageRenderer側の描画データ更新範囲を最小化する。
    static std::vector<Object3d*> Apply(
        const StageMap& stageMap,
        bool isEditorMode,
        const std::vector<std::unique_ptr<Object3d>>& objects);
};
