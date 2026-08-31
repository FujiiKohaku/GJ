#pragma once

#include "BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <memory>

class MovingBlockGimmick : public BaseMapChipGimmick {
public:
    bool Initialize(
        const Vector3& position,
        const std::string& texturePath) override;
    void Update() override;
    void Draw() override;

private:
    std::unique_ptr<Object3d> object_;
    Vector3 basePosition_ = { 0.0f, 0.0f, 0.0f };
    float elapsedTime_ = 0.0f;
};
