#pragma once
#include "Engine/LevelEditor/BaseGimmickParam.h"
#include <string>

class SwingingBridgeParam : public BaseGimmickParam {
public:
    SwingingBridgeParam();
    ~SwingingBridgeParam() override = default;

    void Parse(const nlohmann::json& json) override;
    void DrawImGui() override;
    nlohmann::json Serialize() const override;
    std::unique_ptr<BaseGimmickParam> Clone() const override;

public:
    float length_;
    int swingRange_;
    float speed_;
    float phase_;
};
