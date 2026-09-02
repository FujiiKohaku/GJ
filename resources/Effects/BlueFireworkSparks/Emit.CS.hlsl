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
    float seed = gPerFrame.time * 83.17f + float(particleIndex) * 19.31f;
    return float32_t3(Hash(seed + 1.1f), Hash(seed + 13.7f), Hash(seed + 31.9f));
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
    
    // 3D Sphere random direction for firework burst
    float theta = random.x * 6.2831853f;
    float phi = (random.y - 0.5f) * 3.14159265f;
    float speed = 4.5f + random.z * 5.5f;
    float32_t3 dir = float32_t3(cos(phi) * cos(theta), sin(phi), cos(phi) * sin(theta));

    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate + dir * (gEmitter.radius * random.x);
    particle.scale = gEffectSettings.startScale * (0.8f + random.y * 0.4f);
    particle.lifeTime = max(gEffectSettings.lifeTime * (0.8f + random.z * 0.4f), 0.01f);
    particle.velocity = dir * speed;
    particle.currentTime = 0.0f;
    particle.color = gEffectSettings.startColor;
    particle.rotation = random.x * 6.2831853f;
    particle.rotationSpeed = (random.y - 0.5f) * 4.0f;

    gParticles[particleIndex] = particle;
}
