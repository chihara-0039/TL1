#pragma once

#include "MyMath.h"

struct SceneUpdateContext {
    const Matrix4x4& lightViewProjection;
    bool isGuiCaptured = false;
};
