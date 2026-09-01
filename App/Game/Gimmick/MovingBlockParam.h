#pragma once
#include "Engine/LevelEditor/BaseGimmickParam.h"
#include "Engine/Math/MathStruct.h"
#include <string>

class MovingBlockParam : public BaseGimmickParam {
public:
    MovingBlockParam();
    ~MovingBlockParam() override = default;

    void Parse(const nlohmann::json& json) override;
    void DrawImGui() override;
    nlohmann::json Serialize() const override;
    std::unique_ptr<BaseGimmickParam> Clone() const override;

public:
    float speed_;
    Vector3 range_;
    Vector3 axis_;
};
