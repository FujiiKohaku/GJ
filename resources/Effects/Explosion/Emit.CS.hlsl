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
    float seed = gPerFrame.time * 89.17f + float(particleIndex) * 21.41f;
    return float32_t3(
        Hash(seed + 2.13f),
        Hash(seed + 17.71f),
        Hash(seed + 43.37f));
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
    float32_t3 random = MakeRandom3(particleIndex + emitIndex * 137u);
    float theta = random.x * 6.2831853f;
    float z = random.y * 2.0f - 1.0f;
    float radial = sqrt(max(1.0f - z * z, 0.0f));
    float32_t3 direction = float32_t3(
        radial * cos(theta),
        z,
        radial * sin(theta));

    bool isFireball = (emitIndex % 8u) < 2u;
    float speed = isFireball
        ? 0.65f + random.z * 1.8f
        : 5.0f + random.z * 8.0f;
    float baseScale = isFireball
        ? 0.75f + random.x * 0.70f
        : 0.09f + random.y * 0.13f;
    float lifeTime = isFireball
        ? 0.48f + random.y * 0.32f
        : 0.72f + random.x * 0.55f;

    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate + direction * gEmitter.radius * random.z;
    particle.scale = float32_t3(baseScale, baseScale, baseScale);
    particle.lifeTime = lifeTime;
    particle.velocity = direction * speed;
    particle.currentTime = 0.0f;
    particle.color = isFireball
        ? float32_t4(1.0f, 0.92f, 0.38f, 1.0f)
        : float32_t4(1.0f, 0.46f, 0.035f, 1.0f);
    particle.rotation = random.x * 6.2831853f;
    particle.rotationSpeed = (random.y - 0.5f) * 7.0f;
    particle.padding.x = isFireball ? 0.0f : 1.0f;
    particle.padding.y = baseScale;

    gParticles[particleIndex] = particle;
}
