/**
 * @file GasEmitterParam.cpp
 * @brief ガス発生装置ギミックの設定パラメータの実装
 */
#include "GasEmitterParam.h"
#include "Engine/LevelEditor/GimmickParamFactory.h"
#include "externals/imgui/imgui.h"

namespace {
    struct Registrar {
        Registrar() {
            GimmickParamFactory::GetInstance()->Register("GasEmitter", [] {
                return std::make_unique<GasEmitterParam>();
            });
        }
    } registrar;
}

GasEmitterParam::GasEmitterParam()
    : listenEventName_("EventName")
    , leftBlocks_(1)
    , rightBlocks_(1)
    , upBlocks_(1)
    , downBlocks_(1)
{
}

void GasEmitterParam::Parse(const nlohmann::json& json)
{
    if (json.contains("listenEventName")) {
        listenEventName_ = json["listenEventName"].get<std::string>();
    }
    
    if (json.contains("leftBlocks")) leftBlocks_ = json["leftBlocks"].get<uint32_t>();
    if (json.contains("rightBlocks")) rightBlocks_ = json["rightBlocks"].get<uint32_t>();
    if (json.contains("upBlocks")) upBlocks_ = json["upBlocks"].get<uint32_t>();
    if (json.contains("downBlocks")) downBlocks_ = json["downBlocks"].get<uint32_t>();
    
    // 古いフォーマットの互換性維持
    if (json.contains("blockRange")) {
        uint32_t br = json["blockRange"].get<uint32_t>();
        leftBlocks_ = rightBlocks_ = upBlocks_ = downBlocks_ = br;
    } else if (json.contains("range")) { 
        uint32_t br = static_cast<uint32_t>(json["range"][0].get<float>());
        leftBlocks_ = rightBlocks_ = upBlocks_ = downBlocks_ = br;
    }
}

void GasEmitterParam::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Type: GasEmitter");

    // イベント名入力
    char buffer[256];
    strncpy_s(buffer, listenEventName_.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    if (ImGui::InputText("Listen Event Name", buffer, sizeof(buffer))) {
        listenEventName_ = buffer;
    }

    // ガス範囲の入力
    int left = static_cast<int>(leftBlocks_);
    int right = static_cast<int>(rightBlocks_);
    int up = static_cast<int>(upBlocks_);
    int down = static_cast<int>(downBlocks_);
    
    ImGui::Text("Gas Range (Blocks)");
    if (ImGui::InputInt("Left", &left)) leftBlocks_ = static_cast<uint32_t>((std::max)(0, left));
    if (ImGui::InputInt("Right", &right)) rightBlocks_ = static_cast<uint32_t>((std::max)(0, right));
    if (ImGui::InputInt("Up", &up)) upBlocks_ = static_cast<uint32_t>((std::max)(0, up));
    if (ImGui::InputInt("Down", &down)) downBlocks_ = static_cast<uint32_t>((std::max)(0, down));
#endif
}

nlohmann::json GasEmitterParam::Serialize() const
{
    nlohmann::json json;
    json["type"] = "GasEmitter";
    json["exists"] = true;
    json["listenEventName"] = listenEventName_;
    json["leftBlocks"] = leftBlocks_;
    json["rightBlocks"] = rightBlocks_;
    json["upBlocks"] = upBlocks_;
    json["downBlocks"] = downBlocks_;
    return json;
}

std::unique_ptr<BaseGimmickParam> GasEmitterParam::Clone() const
{
    auto clone = std::make_unique<GasEmitterParam>();
    clone->listenEventName_ = listenEventName_;
    clone->leftBlocks_ = leftBlocks_;
    clone->rightBlocks_ = rightBlocks_;
    clone->upBlocks_ = upBlocks_;
    clone->downBlocks_ = downBlocks_;
    return clone;
}
