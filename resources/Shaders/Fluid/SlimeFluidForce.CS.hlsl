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
        float32_t3 direction = offset / distance;
        float32_t kernelDistance = smoothingRadius - distance;
        float32_t spikyCoefficient =
            45.0f / (kPi * pow(smoothingRadius, 6.0f));
        float32_t pressureTerm =
            (particle.pressure + other.pressure) * 0.5f / otherDensity;
        pressureForce +=
            direction * particleMass * pressureTerm *
            spikyCoefficient * kernelDistance * kernelDistance;

        viscosityForce +=
            (other.velocity - particle.velocity) * particleMass / otherDensity *
            viscosity * ViscosityLaplacian(distance, smoothingRadius);

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

    // neo_Engine FluidSimCS と同じ Shape Matching。
    float32_t3 localTarget = particle.restPosition * blobRadii;
    float32_t3 targetWorld = corePosition + right * localTarget.x + up * localTarget.y + forward * localTarget.z;
    float32_t3 toTarget = targetWorld - particle.position;

    float32_t3 localPullStrength = float32_t3(
        max(1.0f, 1.0f / max(blobRadii.x, 0.1f)) * 3.0f,
        max(1.0f, 1.0f / max(blobRadii.y, 0.1f)) * 6.0f,
        max(1.0f, 1.0f / max(blobRadii.z, 0.1f)) * 3.0f) *
        (shapeAttraction * particleMass) * (1.0f - liquidBlend);
    float32_t3 toTargetLocal = float32_t3(
        dot(toTarget, right),
        dot(toTarget, up),
        dot(toTarget, forward));
    float32_t3 springForceLocal = toTargetLocal * localPullStrength;
    float32_t3 shapeForce =
        right * springForceLocal.x +
        up * springForceLocal.y +
        forward * springForceLocal.z;

    // neo_Engine と同じ下半分の持ち上げと、attraction比例の速度減衰。
    if (particle.position.y < corePosition.y)
    {
        shapeForce.y += 20.0f * particleMass * (1.0f - liquidBlend);
    }
    shapeForce -=
        (particle.velocity - targetVelocity) *
        (shapeAttraction * 0.15f) * (1.0f - liquidBlend);

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
            shapeForce +=
                radial * spread01 * spread01 * puddleSpread * liquidBlend;
        }

        float32_t shallowHeight =
            saturate(1.0f - abs(particle.position.y - (floorHeight + particleRadius * 1.05f)) / max(blobRadii.y, 0.001f));
        shapeForce +=
            (targetVelocity - particle.velocity) * shallowHeight * liquidBlend * velocityAttraction * 0.35f;
    }

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
