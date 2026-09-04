#pragma once

#include "Engine/LevelEditor/BaseGimmickParam.h"

/**
 * @class LaserParam
 * @brief レーザー発射機ギミックの設定パラメータ
 */
class LaserParam : public BaseGimmickParam {
public:
    LaserParam();
    ~LaserParam() override = default;

    /**
     * @brief ImGuiによるパラメータ設定画面の描画
     */
    void DrawImGui() override;

    /**
     * @brief パラメータをJSON形式にシリアライズする
     * @return シリアライズされたJSONオブジェクト
     */
    nlohmann::json Serialize() const override;

    /**
     * @brief JSON形式からパラメータをデシリアライズする
     * @param json デシリアライズ元のJSONオブジェクト
     */
    void Parse(const nlohmann::json& json) override;

public:
    // 自身と同じ型のコピーを生成する
    std::unique_ptr<BaseGimmickParam> Clone() const override {
        return std::make_unique<LaserParam>(*this);
    }

public:
    // 照射方向 (0:上, 1:下, 2:左, 3:右)
    int direction_;
    
    // レーザーの最大到達マス数
    int maxDistance_;
};
