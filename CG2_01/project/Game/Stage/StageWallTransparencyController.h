#pragma once
#include <vector>
#include "MyMath.h"

class Object3d;

// カメラとプレイヤーの間にある壁だけを透過対象として制御する。
class StageWallTransparencyController {
public:
    // 透過エリア判定と視線判定を使い、壁オブジェクトのアルファ値を更新する。
    static void Apply(
        std::vector<Object3d*>& wallObjects,
        const Vector3& cameraPos,
        const Vector3& playerPos,
        bool enableTransparency,
        float transparencyAlpha,
        int currentStageIndex);
};
