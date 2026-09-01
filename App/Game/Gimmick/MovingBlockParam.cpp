#include "MovingBlockParam.h"
#include "Engine/LevelEditor/GimmickParamFactory.h"
#include "externals/imgui/imgui.h"

namespace {
    struct Registrar {
        Registrar() {
            GimmickParamFactory::GetInstance()->Register("MovingBlock", [] {
                return std::make_unique<MovingBlockParam>();
            });
        }
    } registrar;
}

MovingBlockParam::MovingBlockParam()
    : speed_(2.0f)
    , range_({ 2.0f, 0.0f, 0.0f })
    , axis_({ 0.0f, 1.0f, 0.0f })
{
}

void MovingBlockParam::Parse(const nlohmann::json& json)
{
    if (json.contains("speed")) {
        speed_ = json["speed"].get<float>();
    }
    if (json.contains("range")) {
        range_.x = json["range"][0].get<float>();
        range_.y = json["range"][2].get<float>();
        range_.z = json["range"][1].get<float>();
    }
    if (json.contains("axis")) {
        axis_.x = json["axis"][0].get<float>();
        axis_.y = json["axis"][2].get<float>();
        axis_.z = json["axis"][1].get<float>();
    }
}

void MovingBlockParam::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Text("Type: MovingBlock");
    ImGui::DragFloat("Speed", &speed_, 0.1f, 0.0f, 20.0f);
    
    // UI上はRangeを整数（マス数）として1つだけ表示する
    int moveDistanceInt = static_cast<int>(std::round(range_.x));
    if (ImGui::DragInt("Move Range (Blocks)", &moveDistanceInt, 1.0f, 0, 20)) {
        range_.x = static_cast<float>(moveDistanceInt);
    }
    
    // 移動方向(Axis)をコンボボックスで選択
    const char* axisItems[] = { "Up (+Y)", "Down (-Y)", "Left (-X)", "Right (+X)" };
    int currentItem = -1;
    
    if (axis_.y > 0.5f) currentItem = 0;
    else if (axis_.y < -0.5f) currentItem = 1;
    else if (axis_.x < -0.5f) currentItem = 2;
    else if (axis_.x > 0.5f) currentItem = 3;
    else currentItem = 0; // フォールバック
    
    if (ImGui::Combo("Move Direction", &currentItem, axisItems, IM_ARRAYSIZE(axisItems))) {
        if (currentItem == 0) axis_ = { 0.0f, 1.0f, 0.0f };
        else if (currentItem == 1) axis_ = { 0.0f, -1.0f, 0.0f };
        else if (currentItem == 2) axis_ = { -1.0f, 0.0f, 0.0f };
        else if (currentItem == 3) axis_ = { 1.0f, 0.0f, 0.0f };
    }
#endif
}

nlohmann::json MovingBlockParam::Serialize() const
{
    nlohmann::json json;
    json["type"] = "MovingBlock";
    json["exists"] = true;
    json["speed"] = speed_;
    // BlenderのZ-up座標系への変換
    json["range"] = { range_.x, range_.z, range_.y };
    json["axis"] = { axis_.x, axis_.z, axis_.y };
    return json;
}

std::unique_ptr<BaseGimmickParam> MovingBlockParam::Clone() const
{
    auto clone = std::make_unique<MovingBlockParam>();
    clone->speed_ = speed_;
    clone->range_ = range_;
    clone->axis_ = axis_;
    return clone;
}
