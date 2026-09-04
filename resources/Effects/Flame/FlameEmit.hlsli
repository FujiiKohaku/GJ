#include "../Common/ParticleCommon.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

float FlameHash(float value)
{
    return frac(sin(value * 1.173f) * 43758.5453f);
}

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || id.x >= gEmitter.count) {
        return;
    }
    int freeIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeIndex);
    if (freeIndex < 0) {
        InterlockedAdd(gFreeListIndex[0], 1);
        return;
    }
    uint index = gFreeList[freeIndex];
    float seed = float(index) * 17.31f + gPerFrame.time * 93.17f;
    float3 random = float3(FlameHash(seed), FlameHash(seed + 19.1f), FlameHash(seed + 47.7f));
    float angle = random.x * 6.2831853f;
    float radius = sqrt(random.y) * gEmitter.radius;
    float3 offset = float3(cos(angle) * radius, 0.0f, sin(angle) * radius);
    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate + offset;
    particle.velocity = gEffectSettings.velocity * (0.8f + random.z * 0.2f);
    particle.velocity.xz += offset.xz * 0.16f;
    particle.lifeTime = max(gEffectSettings.lifeTime * (0.75f + random.y * 0.25f), 0.01f);
    particle.padding = float2(0.75f + random.z * 0.5f, random.x * 97.0f);
    particle.scale = gEffectSettings.startScale * particle.padding.x;
    particle.rotation = gEffectSettings.startRotation + (random.x - 0.5f) * 0.22f;
    particle.rotationSpeed = gEffectSettings.rotationSpeed * (random.y - 0.5f);
    particle.color = gEffectSettings.startColor;
    // Keep the allocation alive even when the visual fade-in starts at zero.
    particle.color.a = max(particle.color.a, 0.00001f);
    gParticles[index] = particle;
}
