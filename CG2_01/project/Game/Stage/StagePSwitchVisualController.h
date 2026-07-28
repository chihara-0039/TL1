#pragma once
#include <vector>
#include "MyMath.h"

class Object3d;

// Pスイッチ系オブジェクトの通常時スケールを保持する表示用データ。
struct StagePSwitchVisualObject {
    Object3d* object = nullptr;
    Vector3 normalScale{ 1.0f, 1.0f, 1.0f };
};

// Pスイッチの有効/無効状態に応じた表示スケールを一括適用する。
class StagePSwitchVisualController {
public:
    // 変更したオブジェクトを返し、呼び出し側で描画インスタンスをDirty化できるようにする。
    static std::vector<Object3d*> Apply(
        bool active,
        std::vector<StagePSwitchVisualObject>& switchObjects,
        std::vector<StagePSwitchVisualObject>& blockObjects);
};
