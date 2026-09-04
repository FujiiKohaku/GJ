#include "LaserParam.h"
#include "Engine/LevelEditor/GimmickParamFactory.h"
#include "externals/imgui/imgui.h"

namespace {
    struct LaserParamRegister {
        LaserParamRegister() {
            GimmickParamFactory::GetInstance()->Register("LaserEmitter", [] {
                return std::make_unique<LaserParam>();
            });
        }
    } registerer;
}

LaserParam::LaserParam()
    : direction_(2) // デフォルトは左向き(2)
    , maxDistance_(100)
{
}

void LaserParam::DrawImGui()
{
    // コンボボックスで方向を選択
    const char* directions[] = { "Up", "Down", "Left", "Right" };
    ImGui::Combo("Direction", &direction_, directions, IM_ARRAYSIZE(directions));

    ImGui::DragInt("Max Distance", &maxDistance_, 1.0f, 1, 100);
}

nlohmann::json LaserParam::Serialize() const
{
    nlohmann::json json;
    json["direction"] = direction_;
    json["maxDistance"] = maxDistance_;
    return json;
}

void LaserParam::Parse(const nlohmann::json& json)
{
    if (json.contains("direction") && json["direction"].is_number()) {
        direction_ = json["direction"];
    }
    if (json.contains("maxDistance") && json["maxDistance"].is_number()) {
        maxDistance_ = json["maxDistance"];
    }
}
