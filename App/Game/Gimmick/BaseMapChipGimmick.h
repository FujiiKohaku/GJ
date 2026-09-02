#pragma once

#include "Engine/Math/MathStruct.h"
#include "Engine/LevelEditor/LevelData.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include <string>

class BaseMapChipGimmick {
public:
    virtual ~BaseMapChipGimmick() = default;

    virtual bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void SetEditorMode(bool isEditorMode) {}
    
    virtual AABB GetAABB() const { return AABB(); }
    virtual Vector3 GetDeltaPosition() const { return {0.0f, 0.0f, 0.0f}; }
    // ゴール判定用フラグ（デフォルトは偽）
    virtual bool IsGoal() const { return false; }
};
