#include "../Common/ParticleCommon.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gEmitter.maxParticles) {
        return;
    }
    ParticleCS particle = gParticles[id.x];
    if (particle.color.a <= 0.0f) {
        return;
    }
    float dt = gPerFrame.deltaTime;
    particle.currentTime += dt;
    if (particle.currentTime >= particle.lifeTime) {
        gParticles[id.x] = (ParticleCS)0;
        int freeIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeIndex);
        freeIndex += 1;
        if (freeIndex < int(gEmitter.maxParticles)) {
            gFreeList[freeIndex] = id.x;
        } else {
            InterlockedAdd(gFreeListIndex[0], -1);
        }
        return;
    }
    float age = saturate(particle.currentTime / particle.lifeTime);
    if (gEffectSettings.enableGravity != 0) {
        particle.velocity.y += gEffectSettings.gravity * dt;
    }
    if (gEffectSettings.enableDrag != 0) {
        particle.velocity *= pow(max(gEffectSettings.drag, 0.001f), dt * 60.0f);
    }
    float3 drift = 0.0f;
    if (gEffectSettings.enableNoise != 0) {
        float phase = particle.padding.y + particle.currentTime * 4.0f;
        drift = float3(sin(phase), 0.0f, cos(phase * 0.83f)) * gEffectSettings.noiseStrength;
    }
#if FLAME_LAYER == 0 || FLAME_LAYER == 1
    // Reduce lateral wandering toward the tip while retaining a broad base.
    drift.xz *= 1.0f - smoothstep(0.15f, 1.0f, age) * 0.8f;
#endif
    particle.translate += (particle.velocity + drift) * dt;
#if FLAME_LAYER == 0 || FLAME_LAYER == 1
    if (gEffectSettings.enableAttraction != 0) {
        float taper = smoothstep(0.1f, 0.9f, age);
        float convergence = 1.0f - exp(-max(gEffectSettings.attractionStrength, 0.0f) * taper * dt);
        particle.translate.xz = lerp(particle.translate.xz, gEmitter.translate.xz, convergence);
    }
#endif
    particle.rotation += particle.rotationSpeed * dt;
    float size = lerp(gEffectSettings.startScale, gEffectSettings.endScale, age) * particle.padding.x;
    // Equal width and height keep the radial PS mask circular.
    particle.scale = size;
#if FLAME_LAYER == 3
    particle.scale = float3(size, size * 2.8f, size);
#endif
    particle.color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, age);
    particle.color.a = max(particle.color.a, 0.00001f);
    gParticles[id.x] = particle;
}
