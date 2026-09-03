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
    float a = Hash(float(emitIndex) * 13.1f + gPerFrame.time) * 6.2831853f;
    float y = Hash(float(emitIndex) * 31.7f) * 1.55f - 0.35f;
    float radial = sqrt(saturate(1.0f - y * y));
    float3 direction = normalize(float3(cos(a) * radial, y, sin(a) * radial));
    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate;
    particle.scale = gEffectSettings.startScale * (0.75f + Hash(float(emitIndex) * 7.3f) * 0.5f);
    particle.lifeTime = gEffectSettings.lifeTime * (0.85f + Hash(float(emitIndex) * 19.9f) * 0.3f);
    particle.velocity = direction * (5.0f + Hash(float(emitIndex) * 43.2f) * 3.2f);
    particle.color = gEffectSettings.startColor;
    gParticles[particleIndex] = particle;
}
