#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DeathFluidSlime.h"
#include <algorithm>
#include <cmath>

DeathFluidSlime::DeathFluidSlime() = default;
DeathFluidSlime::~DeathFluidSlime() = default;

void DeathFluidSlime::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    // 死亡落下シミュレーション用カスタム設定
    settings.particleCount = 1024;
    settings.particleRadius = 0.22f;
    settings.smoothingRadius = 0.60f;
    settings.restDensity = 3.0f;
    settings.particleMass = 1.0f;
    settings.viscosity = 15.0f;
    settings.stiffness = 50.0f;
    settings.surfaceTension = 15.0f;
    settings.gravity = { 0.0f, -9.8f, 0.0f };
    settings.damping = 0.55f;

    settings.boundsMin = { -30.0f, 0.0f, -30.0f };
    settings.boundsMax = { 30.0f, 30.0f, 30.0f };
    settings.boundaryPadding = 0.05f;

    settings.spawnColumns = 16;
    settings.spawnRows = 16;
    settings.spawnLayers = 4;
    settings.spawnSpacing = { 0.20f, 0.20f, 0.20f };
    settings.spawnOrigin = { -1.6f, 1.8f, -1.6f };

    settings.corePosition = { 0.0f, 0.5f, 0.0f };
    settings.coreForward = { 0.0f, 0.0f, 1.0f };
    settings.targetVelocity = { 0.0f, 0.0f, 0.0f };
    settings.floorHeight = 0.0f;
    settings.blobRadii = { 1.65f, 1.85f, 1.45f };
    settings.shapeAttraction = 60.0f;
    settings.velocityAttraction = 12.0f;
    settings.horizontalFriction = 0.72f;

    corePosition_ = settings.corePosition;
    targetVelocity_ = settings.targetVelocity;

    fluid_ = std::make_unique<GpuSphFluid>();
    fluid_->Initialize(dxCommon, srvManager, settings);

    sphereRenderer_ = std::make_unique<GpuSphFluidRenderer>();
    sphereRenderer_->Initialize(dxCommon);

    isActive_ = true;
    isLiquidated_ = false;
}

void DeathFluidSlime::SpawnFromSky(const Vector3& spawnPos, bool liquidated)
{
    if (!fluid_) return;

    corePosition_ = spawnPos;
    targetVelocity_ = { 0.0f, -14.0f, 0.0f };
    isLiquidated_ = liquidated;
    isActive_ = true;
    age_ = 0.0f;

    // スポーン位置を更新してリセット
    settings.spawnOrigin = {
        spawnPos.x - 1.6f,
        spawnPos.y,
        spawnPos.z - 1.6f
    };
    settings.corePosition = spawnPos;
    settings.targetVelocity = targetVelocity_;

    fluid_->Reset(settings);
    fluid_->SetLiquidated(isLiquidated_);
}

void DeathFluidSlime::Update(float deltaTime)
{
    if (!fluid_ || !isActive_) return;

    float dt = (deltaTime <= 0.0f) ? (1.0f / 60.0f) : deltaTime;
    age_ += dt;

    // コア位置を落下・追従
    if (corePosition_.y > settings.floorHeight + 0.5f) {
        corePosition_.y += targetVelocity_.y * dt;
        targetVelocity_.y += settings.gravity.y * dt * 0.4f;
    } else {
        corePosition_.y = settings.floorHeight + 0.35f;
        targetVelocity_.y = 0.0f;
        targetVelocity_.x *= (std::pow)(0.88f, dt * 60.0f);
        targetVelocity_.z *= (std::pow)(0.88f, dt * 60.0f);
    }

    fluid_->SetLiquidated(isLiquidated_);
    fluid_->SetControlState(corePosition_, targetVelocity_, { 0.0f, 0.0f, 1.0f });
    fluid_->Update(dt);
}

void DeathFluidSlime::Draw3D(const Camera& camera)
{
    if (!fluid_ || !isActive_) return;

    if (useDirectSphereDraw && sphereRenderer_) {
        sphereRenderer_->Draw(*fluid_, camera);
    }
}

void DeathFluidSlime::SetLiquidated(bool liquidated)
{
    isLiquidated_ = liquidated;
    if (fluid_) {
        fluid_->SetLiquidated(liquidated);
    }
}

bool DeathFluidSlime::IsLiquidated() const
{
    return isLiquidated_;
}

void DeathFluidSlime::Reset()
{
    if (fluid_) {
        fluid_->Reset(settings);
    }
}

void DeathFluidSlime::Finalize()
{
    if (sphereRenderer_) {
        sphereRenderer_.reset();
    }
    if (fluid_) {
        fluid_->Finalize();
        fluid_.reset();
    }
    isActive_ = false;
}
