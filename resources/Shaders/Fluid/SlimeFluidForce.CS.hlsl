#include "SlimeFluidCommon.hlsli"

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t index = dispatchThreadId.x;
    if (index >= particleCount)
    {
        return;
    }

    SlimeFluidParticle particle = gParticles[index];
    if (particle.padding <= 0.0f)
    {
        gForces[index] = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float32_t3 pressureForce = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 viscosityForce = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 cohesionForce = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 surfaceNormal = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t activeSurfaceTension =
        lerp(surfaceTension * 0.45f, surfaceTension * 0.18f, liquidBlend);
    float32_t selfDensity = max(particle.density, kEpsilon);

    for (uint32_t otherIndex = 0; otherIndex < particleCount; ++otherIndex)
    {
        if (otherIndex == index)
        {
            continue;
        }

        SlimeFluidParticle other = gParticles[otherIndex];
        if (other.padding <= 0.0f)
        {
            continue;
        }

        float32_t3 offset = particle.position - other.position;
        float32_t distance = length(offset);
        if (distance >= smoothingRadius || distance <= kEpsilon)
        {
            continue;
        }

        float32_t otherDensity = max(other.density, kEpsilon);
        float32_t pressureTerm =
            particle.pressure / (selfDensity * selfDensity) +
            other.pressure / (otherDensity * otherDensity);
        pressureForce +=
            -particleMass * selfDensity * pressureTerm *
            SpikyGradientScale(distance, smoothingRadius) * offset;

        viscosityForce +=
            viscosity * particleMass *
            (other.velocity - particle.velocity) / otherDensity *
            ViscosityLaplacian(distance, smoothingRadius);

        float32_t surfaceWeight =
            saturate(1.0f - distance / smoothingRadius);
        cohesionForce +=
            activeSurfaceTension * 0.25f * surfaceWeight * surfaceWeight *
            (other.position - particle.position);

        surfaceNormal +=
            (offset / distance) * surfaceWeight * (particleMass / otherDensity);

        float32_t minSeparation = particleRadius * 1.85f;
        if (distance < minSeparation)
        {
            float32_t overlap = saturate((minSeparation - distance) / minSeparation);
            pressureForce +=
                (offset / distance) * overlap * overlap *
                stiffness * max(restDensity, 1.0f) * 0.24f;
        }
    }

    float32_t normalLength = length(surfaceNormal);
    if (normalLength > 0.08f)
    {
        cohesionForce -=
            normalize(surfaceNormal) *
            activeSurfaceTension * normalLength * 0.18f;
    }

    float32_t3 gravityForce = gravity * particle.density;
    float32_t3 forward = normalize(coreForward);
    float32_t3 up = float32_t3(0.0f, 1.0f, 0.0f);
    if (abs(forward.y) > 0.98f)
    {
        up = float32_t3(0.0f, 0.0f, 1.0f);
    }
    float32_t3 right = normalize(cross(up, forward));
    up = normalize(cross(forward, right));

    // 1. Shape Matching (記憶された初期球体相対位置 restPosition からの目標位置計算)
    float32_t3 localTarget = particle.restPosition * blobRadii;
    float32_t3 targetWorld = corePosition + right * localTarget.x + up * localTarget.y + forward * localTarget.z;
    float32_t3 toTarget = targetWorld - particle.position;

    // 滑らかなドーム球弧を保つバネ復元力（Y軸1.35倍の自然な張り）
    float32_t3 springStiffness = float32_t3(1.1f, 1.35f, 1.1f) * shapeAttraction * 14.0f * (1.0f - liquidBlend * 0.9f);
    float32_t3 springForce = toTarget * springStiffness;

    // 3. 速度ダンピング（過度な揺れ・バネ振動の減衰制御）
    float32_t3 dampingForce = (particle.velocity - targetVelocity) * damping * 16.0f * (1.0f - liquidBlend * 0.8f);

    float32_t3 shapeAcceleration = springForce - dampingForce;

    // 地面（floorHeight）付近に達した粒子に対する下向き重力の相殺と床面平坦力
    float32_t distToFloor = particle.position.y - floorHeight;
    float32_t groundProximity = saturate(1.0f - distToFloor / max(particleRadius * 2.8f, 0.1f));
    if (groundProximity > 0.0f)
    {
        // 常時流れる下向き重力(gravity.y)の過度な押し下げを物理キャンセル
        shapeAcceleration.y += abs(gravity.y) * groundProximity * 1.0f;
        
        // 地面に達した粒子を左右へ自然に押し潰す
        float32_t pushDir = particle.position.x - corePosition.x;
        float32_t signX = (abs(pushDir) > 0.001f) ? sign(pushDir) : 1.0f;
        shapeAcceleration.x += signX * 16.0f * groundProximity;
    }

    shapeAcceleration += (targetVelocity - particle.velocity) * velocityAttraction;

    if (liquidBlend > 0.001f)
    {
        float32_t3 toParticle = particle.position - corePosition;
        float32_t2 planar = float32_t2(dot(toParticle, right), dot(toParticle, forward));
        float32_t planarDistance = length(planar);
        float32_t spreadRadius = max(blobRadii.x * 1.85f, particleRadius * 4.0f);
        float32_t spread01 = saturate(1.0f - planarDistance / spreadRadius);
        if (planarDistance > kEpsilon)
        {
            float32_t3 radial =
                right * (planar.x / planarDistance) +
                forward * (planar.y / planarDistance);
            shapeAcceleration +=
                radial * spread01 * spread01 * puddleSpread * liquidBlend;
        }

        float32_t shallowHeight =
            saturate(1.0f - abs(particle.position.y - (floorHeight + particleRadius * 1.05f)) / max(blobRadii.y, 0.001f));
        shapeAcceleration +=
            (targetVelocity - particle.velocity) * shallowHeight * liquidBlend * velocityAttraction * 0.35f;
    }

    float32_t3 shapeForce = shapeAcceleration * max(particle.density, restDensity);

    float32_t3 totalForce =
        pressureForce + viscosityForce + cohesionForce + gravityForce + shapeForce;
    float32_t maxForce = max(particle.density, restDensity) * 240.0f;
    float32_t forceLength = length(totalForce);
    if (forceLength > maxForce)
    {
        totalForce *= maxForce / forceLength;
    }

    gForces[index] =
        float32_t4(totalForce, 0.0f);
}
