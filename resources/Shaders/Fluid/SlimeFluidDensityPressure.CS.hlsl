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

    density = max(density, 0.01f);
    particle.density = density;
    particle.pressure = max(stiffness * (density - restDensity), 0.0f);
    gParticles[index] = particle;
}
