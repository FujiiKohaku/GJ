#include "../Common/ParticleCommon.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

float Hash(float seed)
{
    return frac(sin(seed) * 43758.5453f);
}

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t emitIndex = dispatchThreadId.x;
    if (gEmitter.emit == 0 || emitIndex >= gEmitter.count)
    {
        return;
    }

    int32_t freeListIndex = -1;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0)
    {
        InterlockedAdd(gFreeListIndex[0], 1);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];
    float seed = gPerFrame.time * 71.13f + particleIndex * 19.91f;
    float32_t3 random = float32_t3(
        Hash(seed + 1.0f), Hash(seed + 7.0f), Hash(seed + 13.0f));
    float32_t2 radial = (random.xz - 0.5f) * gEmitter.radius * 2.0f;

    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate + float32_t3(radial.x, random.y * 0.035f, radial.y);
    particle.scale = gEffectSettings.startScale * (0.74f + random.z * 0.50f);
    particle.lifeTime = gEffectSettings.lifeTime * (0.78f + random.x * 0.34f);
    particle.velocity = gEffectSettings.velocity + float32_t3(
        (random.x - 0.5f) * 0.20f,
        0.11f + random.y * 0.18f,
        (random.z - 0.5f) * 0.10f);
    particle.currentTime = 0.0f;
    particle.color = gEffectSettings.startColor;
    particle.color.a *= 0.72f + random.y * 0.28f;
    particle.rotation = random.x * 6.2831853f;
    particle.rotationSpeed = gEffectSettings.rotationSpeed * (0.65f + random.z * 0.7f);
    particle.padding.x = particle.scale.x;
    gParticles[particleIndex] = particle;
}
