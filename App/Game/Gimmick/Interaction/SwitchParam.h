/**
 * @file SwitchParam.h
 * @brief スイッチ・着火ギミックの設定パラメータ
 */
#pragma once
#include "Engine/LevelEditor/BaseGimmickParam.h"
#include <string>
#include <memory>

/**
 * @brief スイッチ・着火ギミック用パラメータ
 */
class SwitchParam : public BaseGimmickParam {
public:
    SwitchParam();
    ~SwitchParam() override = default;

    void Parse(const nlohmann::json& json) override;
    void DrawImGui() override;
    nlohmann::json Serialize() const override;
    std::unique_ptr<BaseGimmickParam> Clone() const override;

public:
    std::string fireEventName_; ///< 発火するイベント名
    int switchType_;            ///< 種類 (0: 感圧盤, 2: 篝火/着火源)
    int requiredWeight_;        ///< 作動に必要な重さ (感圧盤用)
};
