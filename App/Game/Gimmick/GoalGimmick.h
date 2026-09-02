#pragma once

#include "BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <memory>

class GoalGimmick : public BaseMapChipGimmick {
public:
    bool Initialize(
        const Vector3& position,
        const std::string& modelFile = "",
        const BaseGimmickParam* gimmickParam = nullptr) override;
    void Update() override;
    void Draw() override;

    AABB GetAABB() const override;
    bool IsGoal() const override { return true; }

private:
    std::unique_ptr<Object3d> object_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 size_ = { 1.0f, 1.0f, 1.0f };
};
