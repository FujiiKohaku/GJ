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
    float seed = gPerFrame.time * 77.31f + float(particleIndex) * 23.47f;
    return float32_t3(Hash(seed + 2.3f), Hash(seed + 17.1f), Hash(seed + 41.5f));
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

    // 6 primary directions with slight conical spread
    float32_t3 baseDirections[6] = {
        float32_t3( 1.0f,  0.0f,  0.0f),
        float32_t3(-1.0f,  0.0f,  0.0f),
        float32_t3( 0.0f,  1.0f,  0.0f),
        float32_t3( 0.0f, -1.0f,  0.0f),
        float32_t3( 0.0f,  0.0f,  1.0f),
        float32_t3( 0.0f,  0.0f, -1.0f)
    };

    uint32_t dirIndex = emitIndex % 6u;
    float32_t3 baseDir = baseDirections[dirIndex];
    float32_t3 spread = (random - 0.5f) * 0.25f;
    float32_t3 dir = normalize(baseDir + spread);
    float speed = 5.5f + random.z * 4.0f;

    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate;
    particle.scale = gEffectSettings.startScale * (0.85f + random.y * 0.3f);
    particle.lifeTime = max(gEffectSettings.lifeTime * (0.85f + random.x * 0.3f), 0.01f);
    particle.velocity = dir * speed;
    particle.currentTime = 0.0f;
    particle.color = gEffectSettings.startColor;

    gParticles[particleIndex] = particle;
}
