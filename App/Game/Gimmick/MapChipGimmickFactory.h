#pragma once

#include "App/Game/Map/MapChipField.h"
#include "Engine/Math/MathStruct.h"
#include <memory>
#include <string>

class BaseMapChipGimmick;

class MapChipGimmickFactory {
public:
    static std::unique_ptr<BaseMapChipGimmick> Create(
        MapChipType type,
        const Vector3& position,
        const std::string& texturePath);
};
