#pragma once

#include "BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <memory>

class MapChipStage;

// 着地すると圧縮され、スライムを上方向へ強く打ち出すばね床。
class SpringGimmick final : public BaseMapChipGimmick {
public:
    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;
    void Update() override;
    void Draw() override;
    void EnableToonLighting() override;
    void SetEditorMode(bool isEditorMode) override;
    void SetStage(MapChipStage* stage) override;
    AABB GetAABB() const override;
    bool IsSolid() const override { return true; }
    void OnPlayerStepped() override;

private:
    void ApplyTransform(float scaleY);

    std::unique_ptr<Object3d> object_;
    MapChipStage* stage_ = nullptr;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    float animationTime_ = 0.0f;
    bool isActivated_ = false;
    bool hasLaunchedPlayer_ = false;
    bool isEditorMode_ = false;
};
