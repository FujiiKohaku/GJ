#include "../Common/ParticleCommon.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 id : SV_DispatchThreadID)
{
    uint emitIndex = id.x;
    if (gEmitter.emit == 0 || emitIndex >= gEmitter.count) return;
    int freeIndex = -1;
    InterlockedAdd(gFreeListIndex[0], -1, freeIndex);
    if (freeIndex < 0) { InterlockedAdd(gFreeListIndex[0], 1); return; }

    uint particleIndex = gFreeList[freeIndex];
    float angle = (float(emitIndex) / max(float(gEmitter.count), 1.0f)) * 6.2831853f;
    float wobble = sin(float(emitIndex) * 12.9898f + gPerFrame.time * 3.1f) * 0.12f;
    float3 direction = normalize(float3(cos(angle), sin(angle), wobble));
    ParticleCS particle = (ParticleCS)0;
    particle.translate = gEmitter.translate;
    particle.scale = gEffectSettings.startScale;
    particle.lifeTime = gEffectSettings.lifeTime * (0.9f + frac(float(emitIndex) * 0.618f) * 0.2f);
    particle.velocity = direction * (7.2f + frac(float(emitIndex) * 0.371f) * 1.2f);
    particle.color = gEffectSettings.startColor;
    particle.currentTime = 0.0f;
    gParticles[particleIndex] = particle;
}
