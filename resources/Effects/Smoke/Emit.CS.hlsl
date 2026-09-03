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

float32_t3 MakeRandom3(uint32_t particleIndex)
{
    float seed =
        gPerFrame.time * 91.731f +
        float(particleIndex) * 17.173f;
    return float32_t3(
        Hash(seed + 3.11f),
        Hash(seed + 19.73f),
        Hash(seed + 47.29f));
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
    float32_t3 random = MakeRandom3(particleIndex + emitIndex * 131u);
    float32_t3 emitterOffset = MakeEmitterOffset(
        gEffectSettings.emitterShape,
        random,
        gEmitter.radius);
    emitterOffset *= 0.35f + random.y * 0.65f;

    float velocityVariation = 0.78f + random.x * 0.48f;
    float32_t3 spread = float32_t3(
        (random.x - 0.5f) * 0.38f,
        random.y * 0.62f,
        (random.z - 0.5f) * 0.72f);

    float scaleVariation = 0.72f + Hash(random.x * 29.0f) * 0.56f;
    float lifeVariation = 0.84f + Hash(random.z * 41.0f) * 0.32f;
    float alphaVariation = 0.72f + Hash(random.y * 37.0f) * 0.28f;

    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate + emitterOffset;
    particle.scale = gEffectSettings.startScale * scaleVariation;
    particle.lifeTime = max(gEffectSettings.lifeTime * lifeVariation, 0.01f);
    particle.velocity =
        gEffectSettings.velocity * velocityVariation + spread;
    particle.currentTime = 0.0f;
    particle.color = gEffectSettings.startColor;
    particle.color.a *= alphaVariation;
    particle.rotation = random.x * 6.2831853f;
    particle.rotationSpeed =
        gEffectSettings.rotationSpeed + (random.z - 0.5f) * 0.34f;
    particle.padding.x = scaleVariation;

    gParticles[particleIndex] = particle;
}
