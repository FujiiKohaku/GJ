#pragma once

#include "BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <memory>

// 上面に乗ったプレイヤーを別の奥行きレーンへ射出する大砲。
class CannonGimmick final : public BaseMapChipGimmick {
public:
    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;
    void Update() override;
    void Draw() override;
    void EnableToonLighting() override;
    void SetEditorMode(bool isEditorMode) override;
    AABB GetAABB() const override;
    bool IsSolid() const override { return true; }
    void OnPlayerStepped() override;
    bool ConsumeCannonLaunchRequest(Vector3& outPosition) override;

private:
    std::unique_ptr<Object3d> object_;
    Vector3 position_ = {0.0f, 0.0f, 0.0f};
    float recoilTime_ = 0.0f;
    bool launchRequested_ = false;
    bool isEditorMode_ = false;
};
