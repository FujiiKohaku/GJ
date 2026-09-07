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
    bool isSpark = particle.padding.x > 0.5f;
    if (isSpark)
    {
        particle.velocity.y += gEffectSettings.gravity * deltaTime;
    }
    float drag = pow(max(gEffectSettings.drag, 0.0f), deltaTime * 60.0f);
    particle.velocity *= drag;
    particle.translate += particle.velocity * deltaTime;
    particle.currentTime += deltaTime;
    particle.rotation += particle.rotationSpeed * deltaTime;

    float progress = saturate(particle.currentTime / particle.lifeTime);
    float baseScale = max(particle.padding.y, 0.01f);
    if (isSpark)
    {
        float scale = lerp(baseScale, baseScale * 0.08f, progress);
        particle.scale = float32_t3(scale, scale, scale);
        particle.color.rgb = lerp(
            float32_t3(1.0f, 0.68f, 0.08f),
            float32_t3(0.82f, 0.025f, 0.0f),
            progress);
        particle.color.a = 1.0f - smoothstep(0.55f, 1.0f, progress);
    }
    else
    {
        float expansion = smoothstep(0.0f, 0.28f, progress);
        float collapse = 1.0f - smoothstep(0.42f, 1.0f, progress);
        float scale = baseScale * lerp(0.28f, 1.65f, expansion) * collapse;
        particle.scale = float32_t3(scale, scale, scale);
        particle.color.rgb = lerp(
            float32_t3(1.0f, 0.96f, 0.58f),
            float32_t3(1.0f, 0.12f, 0.005f),
            progress);
        particle.color.a = 1.0f - smoothstep(0.36f, 1.0f, progress);
    }

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
