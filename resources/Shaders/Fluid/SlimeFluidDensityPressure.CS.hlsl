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
        particle.density = restDensity;
        particle.pressure = 0.0f;
        gParticles[index] = particle;
        return;
    }

    float32_t density = 0.0f;

    for (uint32_t otherIndex = 0; otherIndex < particleCount; ++otherIndex)
    {
        SlimeFluidParticle other = gParticles[otherIndex];
        if (other.padding <= 0.0f)
        {
            continue;
        }

        float32_t3 offset = particle.position - other.position;
        density += particleMass * Poly6Kernel(dot(offset, offset), smoothingRadius);
    }

    density = max(density, restDensity * 0.25f);
    particle.density = density;

    float32_t densityRatio = density / max(restDensity, kEpsilon);
    float32_t taitPressure =
        stiffness * (pow(max(densityRatio, 0.001f), 7.0f) - 1.0f);
    particle.pressure = clamp(taitPressure, -stiffness * 0.20f, stiffness * 4.0f);
    gParticles[index] = particle;
}
