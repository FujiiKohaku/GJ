/**
 * @file GearParam.cpp
 * @brief 歯車（Gear）ギミックの設定パラメータ実装
 */
#include "GearParam.h"
#include "Engine/LevelEditor/GimmickParamFactory.h"
#include "externals/imgui/imgui.h"

namespace {
    struct Registrar {
        Registrar() {
            GimmickParamFactory::GetInstance()->Register("Gear", [] {
                return std::make_unique<GearParam>();
            });
        }
    } registrar;
}

GearParam::GearParam()
{
}

void GearParam::Parse(const nlohmann::json& json)
{
    if (json.contains("rotationSpeed")) {
        rotationSpeed_ = json["rotationSpeed"].get<float>();
    }
    if (json.contains("scale")) {
        scale_ = json["scale"].get<float>();
    }
    if (json.contains("collisionRadius")) {
        collisionRadius_ = json["collisionRadius"].get<float>();
    }
}

void GearParam::DrawImGui()
{
    if (ImGui::CollapsingHeader("Gear Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Gear obstacle properties");
        ImGui::DragFloat("Rotation Speed (deg/s)", &rotationSpeed_, 1.0f, -720.0f, 720.0f);
        ImGui::DragFloat("Scale", &scale_, 0.05f, 0.1f, 10.0f);
        ImGui::DragFloat("Collision Radius", &collisionRadius_, 0.01f, 0.1f, 5.0f);
    }
}

nlohmann::json GearParam::Serialize() const
{
    nlohmann::json j;
    j["rotationSpeed"] = rotationSpeed_;
    j["scale"] = scale_;
    j["collisionRadius"] = collisionRadius_;
    return j;
}

std::unique_ptr<BaseGimmickParam> GearParam::Clone() const
{
    auto clone = std::make_unique<GearParam>();
    clone->rotationSpeed_ = this->rotationSpeed_;
    clone->scale_ = this->scale_;
    clone->collisionRadius_ = this->collisionRadius_;
    return clone;
}
