#include "../Common/ParticleCommon.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

float Hash(float n) { return frac(sin(n) * 43758.5453f); }

[numthreads(256, 1, 1)]
void main(uint32_t3 id : SV_DispatchThreadID)
{
    uint emitIndex = id.x;
    if (gEmitter.emit == 0 || emitIndex >= gEmitter.count) return;
    int freeIndex = -1;
    InterlockedAdd(gFreeListIndex[0], -1, freeIndex);
    if (freeIndex < 0) { InterlockedAdd(gFreeListIndex[0], 1); return; }

    uint particleIndex = gFreeList[freeIndex];
    uint ray = emitIndex % 9u;
    float angle = float(ray) / 9.0f * 6.2831853f;
    float spread = (Hash(float(emitIndex) * 17.2f) - 0.5f) * 0.20f;
    float3 direction = normalize(float3(cos(angle) * (0.68f + spread), 0.72f, sin(angle) * (0.68f + spread)));
    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate;
    particle.scale = gEffectSettings.startScale;
    particle.lifeTime = gEffectSettings.lifeTime * (0.9f + Hash(float(emitIndex) * 3.7f) * 0.2f);
    particle.velocity = direction * (6.0f + Hash(float(emitIndex) * 23.4f) * 3.0f);
    particle.color = gEffectSettings.startColor;
    gParticles[particleIndex] = particle;
}
