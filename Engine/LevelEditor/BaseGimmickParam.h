#pragma once
#include <string>
#include <memory>
#include "externals/json.hpp"

class BaseGimmickParam {
public:
    virtual ~BaseGimmickParam() = default;

    // JSONからパラメータを読み込む
    virtual void Parse(const nlohmann::json& json) = 0;

    // ImGui でパラメータ設定UIを描画する
    virtual void DrawImGui() = 0;

    // 現在のパラメータをJSONにシリアライズする
    virtual nlohmann::json Serialize() const = 0;
    
    // 自身と同じ型のコピーを生成する (Prototypeパターン)
    virtual std::unique_ptr<BaseGimmickParam> Clone() const = 0;
};
