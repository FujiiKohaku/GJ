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
    float32_t density = 0.0f;

    for (uint32_t otherIndex = 0; otherIndex < particleCount; ++otherIndex)
    {
        SlimeFluidParticle other = gParticles[otherIndex];
        float32_t3 offset = particle.position - other.position;
        density += particleMass * Poly6Kernel(dot(offset, offset), smoothingRadius);
    }

    density = max(density, restDensity * 0.25f);
    particle.density = density;
    particle.pressure = max((density - restDensity) * stiffness, 0.0f);
    gParticles[index] = particle;
}
