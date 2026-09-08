#pragma once

#include "BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <array>
#include <memory>

// 通過すると次の命のリレー時の復帰地点を更新する中間地点。
class CheckpointGimmick final : public BaseMapChipGimmick {
public:
    bool Initialize(const Vector3& position, const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;
    void Update() override;
    void Draw() override;
    void EnableToonLighting() override;
    AABB GetAABB() const override;
    bool IsCheckpoint() const override { return true; }
    bool IsSolid() const override { return false; }
    bool TryActivateCheckpoint(const AABB& playerAABB) override;
    const Vector3& GetPosition() const { return position_; }

private:
    static constexpr size_t kFlagSegmentCount = 12;

    std::unique_ptr<Object3d> standObject_;
    std::array<std::unique_ptr<Object3d>, kFlagSegmentCount> flagObjects_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 size_ = { 0.72f, 1.25f, 0.72f };
    float time_ = 0.0f;
    bool isActivated_ = false;
};
