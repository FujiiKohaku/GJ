#pragma once

#include "Engine/Fluid/GpuSphFluid.h"
#include "Engine/Fluid/GpuSphFluidRenderer.h"
#include "Engine/Math/MathStruct.h"
#include <memory>

class Camera;
class DirectXCommon;
class SrvManager;

class DeathFluidSlime {
public:
    DeathFluidSlime();
    ~DeathFluidSlime();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void SpawnFromSky(const Vector3& spawnPos = { 0.0f, 8.5f, 0.0f }, bool liquidated = false);
    void Update(float deltaTime);
    void Draw3D(const Camera& camera);
    void Finalize();

    void SetLiquidated(bool liquidated);
    bool IsLiquidated() const;
    void Reset();

    GpuSphFluid* GetFluid() const { return fluid_.get(); }
    bool IsActive() const { return isActive_; }

    // 物理パラメータ
    GpuSphFluid::Settings settings {};
    Vector3 corePosition_ = { 0.0f, 0.45f, 0.0f };
    Vector3 targetVelocity_ = { 0.0f, 0.0f, 0.0f };
    bool useDirectSphereDraw = true; // 3D球体ビルボード描画も併用/選択可能

private:
    std::unique_ptr<GpuSphFluid> fluid_;
    std::unique_ptr<GpuSphFluidRenderer> sphereRenderer_;
    bool isActive_ = false;
    bool isLiquidated_ = false;
    float age_ = 0.0f;
};
