/**
 * @file GearParam.h
 * @brief 歯車（Gear）ギミックの設定パラメータ
 */
#pragma once
#include "Engine/LevelEditor/BaseGimmickParam.h"
#include <memory>

/**
 * @brief 歯車ギミック用パラメータ
 */
class GearParam : public BaseGimmickParam {
public:
    GearParam();
    ~GearParam() override = default;

    void Parse(const nlohmann::json& json) override;
    void DrawImGui() override;
    nlohmann::json Serialize() const override;
    std::unique_ptr<BaseGimmickParam> Clone() const override;

public:
    float rotationSpeed_ = -90.0f; // 回転速度（度/秒）。マイナスは左回転
    float scale_ = 1.0f;           // 表示スケール
    float collisionRadius_ = 0.4f; // 当たり判定（球）の半径。1ブロックは1.0x1.0なので、少し小さめにする
};
