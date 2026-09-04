/**
 * @file SpikeParam.h
 * @brief トゲ（Spike）ギミックの設定パラメータ
 */
#pragma once
#include "Engine/LevelEditor/BaseGimmickParam.h"
#include <memory>

/**
 * @brief トゲギミック用パラメータ
 */
class SpikeParam : public BaseGimmickParam {
public:
    SpikeParam();
    ~SpikeParam() override = default;

    void Parse(const nlohmann::json& json) override;
    void DrawImGui() override;
    nlohmann::json Serialize() const override;
    std::unique_ptr<BaseGimmickParam> Clone() const override;
};
