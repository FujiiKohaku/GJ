#pragma once

#include "BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <array>
#include <memory>

// 一度踏むと震えた後、複数の岩片に分かれて崩落する床。
class CrumblingFloorGimmick final : public BaseMapChipGimmick {
public:
    enum class State {
        Idle,
        Shaking,
        Falling,
        Gone,
    };

    struct CrumblingFloorState : public IGimmickState {
        State state;
        float stateTime;
    };

    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;
    void Update() override;
    void Draw() override;
    void EnableToonLighting() override;
    void SetEditorMode(bool isEditorMode) override;
    AABB GetAABB() const override;
    bool IsSolid() const override;
    void OnPlayerStepped() override;

    std::shared_ptr<IGimmickState> CreateSnapshot() const override;
    void RestoreFromSnapshot(const IGimmickState* state) override;

private:
    static constexpr size_t kPieceCount = 6;

    void ApplyRockMaterial(size_t index);
    Vector3 GetPieceBasePosition(size_t index) const;

    std::array<std::unique_ptr<Object3d>, kPieceCount> pieces_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    State state_ = State::Idle;
    float stateTime_ = 0.0f;
    bool isEditorMode_ = false;
};
