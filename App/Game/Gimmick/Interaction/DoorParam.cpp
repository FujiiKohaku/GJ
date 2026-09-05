/**
 * @file DoorParam.cpp
 * @brief ドアギミックの設定パラメータの実装
 */
#include "DoorParam.h"
#include "Engine/LevelEditor/GimmickParamFactory.h"
#include "externals/imgui/imgui.h"

namespace {
    struct Registrar {
        Registrar() {
            GimmickParamFactory::GetInstance()->Register("Door", [] {
                return std::make_unique<DoorParam>();
            });
        }
    } registrar;
}

DoorParam::DoorParam()
    : listenEventName_("EventName")
    , heightBlocks_(1)
{
}

void DoorParam::Parse(const nlohmann::json& json)
{
    if (json.contains("listenEventName")) {
        listenEventName_ = json["listenEventName"].get<std::string>();
    } else if (json.contains("receiveEventName")) {
        // 後方互換性
        listenEventName_ = json["receiveEventName"].get<std::string>();
    }

    if (json.contains("heightBlocks")) {
        heightBlocks_ = json["heightBlocks"].get<int>();
    } else if (json.contains("height")) {
        // 後方互換性 (float -> int blocks)
        float h = json["height"].get<float>();
        heightBlocks_ = static_cast<int>(std::round(h));
        if (heightBlocks_ < 1) heightBlocks_ = 1;
    }
}

void DoorParam::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Type: Door");

    // イベント名入力
    char buffer[256];
    strncpy_s(buffer, listenEventName_.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    if (ImGui::InputText("Listen Event Name", buffer, sizeof(buffer))) {
        listenEventName_ = buffer;
    }

    // 高さのマス数入力
    if (ImGui::InputInt("Height (Blocks)", &heightBlocks_)) {
        if (heightBlocks_ < 1) heightBlocks_ = 1;
    }
#endif
}

nlohmann::json DoorParam::Serialize() const
{
    nlohmann::json json;
    json["type"] = "Door";
    json["exists"] = true;
    json["listenEventName"] = listenEventName_;
    json["heightBlocks"] = heightBlocks_;
    return json;
}

std::unique_ptr<BaseGimmickParam> DoorParam::Clone() const
{
    auto clone = std::make_unique<DoorParam>();
    clone->listenEventName_ = listenEventName_;
    clone->heightBlocks_ = heightBlocks_;
    return clone;
}
