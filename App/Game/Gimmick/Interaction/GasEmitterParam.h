/**
 * @file GasEmitterParam.h
 * @brief ガス発生装置ギミックの設定パラメータ
 */
#pragma once
#include "Engine/LevelEditor/BaseGimmickParam.h"
#include "Engine/Math/MathStruct.h"
#include <string>
#include <memory>

/**
 * @brief ガス発生装置用パラメータ
 */
class GasEmitterParam : public BaseGimmickParam {
public:
    GasEmitterParam();
    ~GasEmitterParam() override = default;

    void Parse(const nlohmann::json& json) override;
    void DrawImGui() override;
    nlohmann::json Serialize() const override;
    std::unique_ptr<BaseGimmickParam> Clone() const override;

public:
    std::string listenEventName_; ///< 待機するイベント名（受信でガス放出）
    
    // ガス範囲（発生位置からのマス数）
    uint32_t leftBlocks_;
    uint32_t rightBlocks_;
    uint32_t upBlocks_;
    uint32_t downBlocks_;
};
