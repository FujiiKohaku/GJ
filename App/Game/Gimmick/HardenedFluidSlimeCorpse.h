#pragma once
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "Engine/Fluid/GpuSphFluid.h"
#include <memory>
#include <vector>

class DirectXCommon;
class SrvManager;

class HardenedFluidSlimeCorpse : public BaseMapChipGimmick {
public:
    HardenedFluidSlimeCorpse();
    ~HardenedFluidSlimeCorpse() override = default;

    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;

    bool InitializeFromParticles(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        const std::vector<GpuSphFluid::Particle>& sourceParticles,
        const GpuSphFluid::Settings& sourceSettings);

    void Update() override;
    void Draw() override {}

    AABB GetAABB() const override { return boundsAABB_; }
    std::vector<AABB> GetCollisionBoxes() const override { return collisionBoxes_; }
    // A corpse remains a physical platform. Player crush handling explicitly
    // excludes it, so it can be stood on without becoming a lethal wall.
    bool IsSolid() const override { return true; }
    bool IsHardenedSlime() const override { return true; }

    GpuSphFluid* GetFluid() const { return fluid_.get(); }

private:
    void BuildCollisionBoxes(const std::vector<GpuSphFluid::Particle>& particles);

    std::unique_ptr<GpuSphFluid> fluid_;
    AABB boundsAABB_{};
    std::vector<AABB> collisionBoxes_;
};
