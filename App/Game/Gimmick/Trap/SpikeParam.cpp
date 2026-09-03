#include "SpikeParam.h"
#include "SpikeGimmick.h"
#include "Engine/LevelEditor/GimmickParamFactory.h"
#include "externals/imgui/imgui.h"

namespace {
    struct Registrar {
        Registrar() {
            GimmickParamFactory::GetInstance()->Register("Spike", [] {
                return std::make_unique<SpikeParam>();
            });
        }
    } registrar;
}

SpikeParam::SpikeParam()
{
}

void SpikeParam::Parse(const nlohmann::json& json)
{
    // 現在のところ固有パラメータなし
    (void)json;
}

void SpikeParam::DrawImGui()
{
    if (ImGui::CollapsingHeader("Spike Common Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("These settings apply to all spikes.");
        ImGui::DragFloat3("AABB Size", &SpikeGimmick::s_spikeAABBSize.x, 0.01f);
        ImGui::DragFloat3("AABB Offset", &SpikeGimmick::s_spikeAABBOffset.x, 0.01f);
    }
}

nlohmann::json SpikeParam::Serialize() const
{
    nlohmann::json j;
    // 現在のところ固有パラメータなし
    return j;
}

std::unique_ptr<BaseGimmickParam> SpikeParam::Clone() const
{
    auto clone = std::make_unique<SpikeParam>();
    return clone;
}
