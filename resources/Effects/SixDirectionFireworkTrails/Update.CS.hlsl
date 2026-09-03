#include "../Common/ParticleCommon.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t particleIndex = dispatchThreadId.x;
    if (particleIndex >= gEmitter.maxParticles)
    {
        return;
    }

    ParticleCS particle = gParticles[particleIndex];
    if (particle.color.a <= 0.0f)
    {
        return;
    }

    float deltaTime = gPerFrame.deltaTime;

    if (gEffectSettings.enableGravity != 0)
    {
        particle.velocity.y += gEffectSettings.gravity * deltaTime;
    }

    if (gEffectSettings.enableDrag != 0)
    {
        float drag = pow(max(gEffectSettings.drag, 0.0f), deltaTime * 60.0f);
        particle.velocity *= drag;
    }

    particle.translate += particle.velocity * deltaTime;
    particle.currentTime += deltaTime;

    float progress = saturate(particle.currentTime / particle.lifeTime);
    float scale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, progress);
    particle.scale = float32_t3(scale, scale, scale);

    particle.color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, progress);
    float fadeIn = smoothstep(0.0f, 0.05f, progress);
    float fadeOut = 1.0f - smoothstep(0.75f, 1.0f, progress);
    particle.color.a *= fadeIn * fadeOut;

    if (particle.currentTime < particle.lifeTime && particle.color.a > 0.0f)
    {
        gParticles[particleIndex] = particle;
        return;
    }

    gParticles[particleIndex] = (ParticleCS)0;

    int32_t returnedIndex = -1;
    InterlockedAdd(gFreeListIndex[0], 1, returnedIndex);
    returnedIndex += 1;
    if (returnedIndex < int32_t(gEmitter.maxParticles))
    {
        gFreeList[returnedIndex] = particleIndex;
        return;
    }

    InterlockedAdd(gFreeListIndex[0], -1);
}
