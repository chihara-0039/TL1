#pragma once

#include <memory>
#include "SceneType.h"

class BaseScene;

class SceneFactory {
public:
    std::unique_ptr<BaseScene> CreateScene(SceneType type) const;
};
