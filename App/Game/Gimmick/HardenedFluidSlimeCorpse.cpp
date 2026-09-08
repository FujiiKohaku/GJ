#include "HardenedFluidSlimeCorpse.h"
#include <algorithm>
#include <cmath>
#include <limits>

HardenedFluidSlimeCorpse::HardenedFluidSlimeCorpse() = default;

bool HardenedFluidSlimeCorpse::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    return true;
}

bool HardenedFluidSlimeCorpse::InitializeFromParticles(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    const std::vector<GpuSphFluid::Particle>& sourceParticles,
    const GpuSphFluid::Settings& sourceSettings)
{
    if (sourceParticles.empty() || dxCommon == nullptr || srvManager == nullptr) {
        return false;
    }

    GpuSphFluid::Settings corpseSettings = sourceSettings;
    corpseSettings.stiffness = 0.0f;
    corpseSettings.gravity = { 0.0f, 0.0f, 0.0f };
    corpseSettings.damping = 100.0f;
    corpseSettings.viscosity = 100.0f;

    // 変形中に硬化すると、操作用コアは元の位置のままでも粒子群は別の
    // 位置・形に広がっている。死体の顔は粒子群の中心を基準にする。
    std::vector<GpuSphFluid::Particle> frozenParticles = sourceParticles;
    Vector3 particleCenter {0.0f, 0.0f, 0.0f};
    for (const auto& particle : frozenParticles) {
        particleCenter.x += particle.position.x;
        particleCenter.y += particle.position.y;
        particleCenter.z += particle.position.z;
    }
    const float particleCount = static_cast<float>(frozenParticles.size());
    particleCenter.x /= particleCount;
    particleCenter.y /= particleCount;
    particleCenter.z /= particleCount;

    // Use a real particle nearest the center rather than an empty point in a
    // concave shape. The × mark is therefore always anchored inside the body.
    const GpuSphFluid::Particle* faceParticle = &frozenParticles.front();
    // Windows headers define max as a macro, so call the numeric-limits
    // function through parentheses to prevent macro expansion.
    float nearestDistanceSq = (std::numeric_limits<float>::max)();
    for (const auto& particle : frozenParticles) {
        const float dx = particle.position.x - particleCenter.x;
        const float dy = particle.position.y - particleCenter.y;
        const float dz = particle.position.z - particleCenter.z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;
        if (distanceSq < nearestDistanceSq) {
            nearestDistanceSq = distanceSq;
            faceParticle = &particle;
        }
    }
    corpseSettings.corePosition = faceParticle->position;

    fluid_ = std::make_unique<GpuSphFluid>();
    fluid_->Initialize(dxCommon, srvManager, corpseSettings);
    // 硬化した死体には、通常の目ではなく死亡を示す×印の目を描画する。
    fluid_->SetHideEyes(false);
    fluid_->SetDeathEyes(true);

    for (auto& p : frozenParticles) {
        p.velocity = { 0.0f, 0.0f, 0.0f };
    }

    fluid_->SetParticlesCPU(frozenParticles);

    BuildCollisionBoxes(frozenParticles);

    return true;
}

void HardenedFluidSlimeCorpse::Update()
{
    if (fluid_) {
        fluid_->Update(0.0f);
    }
}

void HardenedFluidSlimeCorpse::BuildCollisionBoxes(const std::vector<GpuSphFluid::Particle>& particles)
{
    collisionBoxes_.clear();
    if (particles.empty()) return;

    Vector3 minP = particles[0].position;
    Vector3 maxP = particles[0].position;

    for (const auto& p : particles) {
        minP.x = (std::min)(minP.x, p.position.x);
        minP.y = (std::min)(minP.y, p.position.y);
        minP.z = (std::min)(minP.z, p.position.z);

        maxP.x = (std::max)(maxP.x, p.position.x);
        maxP.y = (std::max)(maxP.y, p.position.y);
        maxP.z = (std::max)(maxP.z, p.position.z);
    }

    float radius = 0.15f;
    minP.x -= radius; minP.y -= radius; minP.z -= radius;
    maxP.x += radius; maxP.y += radius; maxP.z += radius;

    boundsAABB_.center = (minP + maxP) * 0.5f;
    boundsAABB_.size = maxP - minP;

    const int numSlices = 12;
    float sliceWidth = (maxP.x - minP.x) / static_cast<float>(numSlices);

    if (sliceWidth < 0.05f) {
        collisionBoxes_.push_back(boundsAABB_);
        return;
    }

    for (int i = 0; i < numSlices; ++i) {
        float sliceMinX = minP.x + static_cast<float>(i) * sliceWidth;
        float sliceMaxX = sliceMinX + sliceWidth;

        float sliceMinY = maxP.y;
        float sliceMaxY = minP.y;
        float sliceMinZ = maxP.z;
        float sliceMaxZ = minP.z;
        bool hasParticleInSlice = false;

        for (const auto& p : particles) {
            if (p.position.x >= sliceMinX - radius && p.position.x <= sliceMaxX + radius) {
                sliceMinY = (std::min)(sliceMinY, p.position.y - radius);
                sliceMaxY = (std::max)(sliceMaxY, p.position.y + radius);
                sliceMinZ = (std::min)(sliceMinZ, p.position.z - radius);
                sliceMaxZ = (std::max)(sliceMaxZ, p.position.z + radius);
                hasParticleInSlice = true;
            }
        }

        if (hasParticleInSlice) {
            AABB sliceBox;
            Vector3 sMin = { sliceMinX, sliceMinY, sliceMinZ };
            Vector3 sMax = { sliceMaxX, sliceMaxY, sliceMaxZ };
            sliceBox.center = (sMin + sMax) * 0.5f;
            sliceBox.size = sMax - sMin;
            collisionBoxes_.push_back(sliceBox);
        }
    }

    if (collisionBoxes_.empty()) {
        collisionBoxes_.push_back(boundsAABB_);
    }
}
