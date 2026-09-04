#include "SwingingBridgeParam.h"
#include "Engine/LevelEditor/GimmickParamFactory.h"
#include "externals/imgui/imgui.h"

namespace {
    struct Registrar {
        Registrar() {
            GimmickParamFactory::GetInstance()->Register("SwingingBridge", [] {
                return std::make_unique<SwingingBridgeParam>();
            });
        }
    } registrar;
}

SwingingBridgeParam::SwingingBridgeParam()
    : length_(5.0f)
    , swingRange_(2)
    , speed_(2.0f)
    , phase_(0.0f)
{
}

void SwingingBridgeParam::Parse(const nlohmann::json& json)
{
    if (json.contains("length")) {
        length_ = json["length"].get<float>();
    }
    if (json.contains("swingRange")) {
        swingRange_ = json["swingRange"].get<int>();
    }
    if (json.contains("speed")) {
        speed_ = json["speed"].get<float>();
    }
    if (json.contains("phase")) {
        phase_ = json["phase"].get<float>();
    }
}

void SwingingBridgeParam::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Type: SwingingBridge");
    ImGui::DragFloat("Rope Length", &length_, 0.1f, 1.0f, 20.0f);
    ImGui::DragInt("Swing Range (Blocks)", &swingRange_, 1, 0, 10);
    ImGui::DragFloat("Swing Speed", &speed_, 0.1f, 0.1f, 10.0f);
    ImGui::DragFloat("Initial Phase", &phase_, 0.05f, 0.0f, 1.0f);
#endif
}

nlohmann::json SwingingBridgeParam::Serialize() const
{
    nlohmann::json json;
    json["type"] = "SwingingBridge";
    json["exists"] = true;
    json["length"] = length_;
    json["swingRange"] = swingRange_;
    json["speed"] = speed_;
    json["phase"] = phase_;
    return json;
}

std::unique_ptr<BaseGimmickParam> SwingingBridgeParam::Clone() const
{
    auto clone = std::make_unique<SwingingBridgeParam>();
    clone->length_ = length_;
    clone->swingRange_ = swingRange_;
    clone->speed_ = speed_;
    clone->phase_ = phase_;
    return clone;
}
