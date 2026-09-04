#include "../Common/ParticleRenderCommon.hlsli"

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    ParticleCS particle = gParticles[instanceId];
    // Anchor the flame at the bottom of its billboard, above the emitter plane.
    VertexShaderInput anchored = input;
    anchored.position.y += 0.5f;
    VertexShaderOutput output = BuildBillboardParticleVertex(anchored, instanceId);
    float age = saturate(particle.currentTime / max(particle.lifeTime, 0.01f));
    // Unlit particles do not need a normal: carry animation time, seed and age.
    output.normal = float3(particle.currentTime, particle.padding.y, age);
    output.color.a *= smoothstep(0.0f, 0.08f, age) * (1.0f - smoothstep(0.65f, 1.0f, age));
    return output;
}
