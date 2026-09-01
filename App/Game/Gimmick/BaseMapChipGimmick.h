#pragma once

#include "Engine/Math/MathStruct.h"
#include "Engine/LevelEditor/LevelData.h"
#include <string>

class BaseMapChipGimmick {
public:
    virtual ~BaseMapChipGimmick() = default;

    virtual bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const LevelData::ObjectData::GimmickData* gimmickData = nullptr) = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void SetEditorMode(bool isEditorMode) {}
};
