/**
 * @file DoorParam.h
 * @brief ドアギミックの設定パラメータ
 */
#pragma once
#include "Engine/LevelEditor/BaseGimmickParam.h"
#include <string>
#include <memory>

/**
 * @brief ドアギミック用パラメータ
 */
class DoorParam : public BaseGimmickParam {
public:
    DoorParam();
    ~DoorParam() override = default;

    void Parse(const nlohmann::json& json) override;
    void DrawImGui() override;
    nlohmann::json Serialize() const override;
    std::unique_ptr<BaseGimmickParam> Clone() const override;

public:
    std::string listenEventName_; ///< 開閉のリンク対象となるイベント名
    int heightBlocks_;            ///< ドアの高さ（マス数）。1マスにつき2パーツ生成されます
};
