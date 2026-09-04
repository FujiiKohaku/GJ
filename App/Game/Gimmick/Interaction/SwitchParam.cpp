/**
 * @file SwitchParam.cpp
 * @brief スイッチ・着火ギミックの設定パラメータの実装
 */
#include "SwitchParam.h"
#include "Engine/LevelEditor/GimmickParamFactory.h"
#include "App/Game/Gimmick/Interaction/SwitchGimmick.h"
#include "externals/imgui/imgui.h"

namespace {
    struct Registrar {
        Registrar() {
            GimmickParamFactory::GetInstance()->Register("Switch", [] {
                return std::make_unique<SwitchParam>();
            });
        }
    } registrar;
}

SwitchParam::SwitchParam()
    : fireEventName_("EventName")
    , switchType_(0)
    , requiredWeight_(1)
{
}

void SwitchParam::Parse(const nlohmann::json& json)
{
    if (json.contains("fireEventName")) {
        fireEventName_ = json["fireEventName"].get<std::string>();
    }
    if (json.contains("switchType")) {
        switchType_ = json["switchType"].get<int>();
    }
    if (json.contains("requiredWeight")) {
        requiredWeight_ = json["requiredWeight"].get<int>();
    }
}

void SwitchParam::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Type: Switch");

    // イベント名入力
    char buffer[256];
    strncpy_s(buffer, fireEventName_.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    if (ImGui::InputText("Fire Event Name", buffer, sizeof(buffer))) {
        fireEventName_ = buffer;
    }

    // スイッチの種類
    const char* typeItems[] = { "PressurePlate (0)", "Button (1)", "Bonfire (2)" };
    if (ImGui::Combo("Switch Type", &switchType_, typeItems, IM_ARRAYSIZE(typeItems))) {
        // 値が変更された場合の処理
    }

    // 感圧盤の場合のみ重さ設定とグローバルAABB設定を表示
    if (switchType_ == 0) {
        ImGui::DragInt("Required Weight", &requiredWeight_, 1, 1, 100);
        
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[Global Settings]");
        ImGui::DragFloat3("AABB Offset", &SwitchGimmick::s_pressurePlateAABBOffset.x, 0.05f);
        ImGui::DragFloat3("AABB Size", &SwitchGimmick::s_pressurePlateAABBSize.x, 0.05f, 0.01f, 5.0f);
    }
#endif
}

nlohmann::json SwitchParam::Serialize() const
{
    nlohmann::json json;
    json["type"] = "Switch";
    json["exists"] = true;
    json["fireEventName"] = fireEventName_;
    json["switchType"] = switchType_;
    json["requiredWeight"] = requiredWeight_;
    return json;
}

std::unique_ptr<BaseGimmickParam> SwitchParam::Clone() const
{
    auto clone = std::make_unique<SwitchParam>();
    clone->fireEventName_ = fireEventName_;
    clone->switchType_ = switchType_;
    clone->requiredWeight_ = requiredWeight_;
    return clone;
}
