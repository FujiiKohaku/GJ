#include "SlimeFluidRenderCommon.hlsli"

StructuredBuffer<SlimeFluidParticle> gParticles : register(t0);

static const float32_t2 kQuadPositions[6] =
{
    float32_t2(-1.0f, -1.0f),
    float32_t2(-1.0f, 1.0f),
    float32_t2(1.0f, -1.0f),
    float32_t2(1.0f, -1.0f),
    float32_t2(-1.0f, 1.0f),
    float32_t2(1.0f, 1.0f),
};

SlimeDepthVertexOutput main(
    uint32_t vertexId : SV_VertexID,
    uint32_t instanceId : SV_InstanceID)
{
    SlimeFluidParticle particle = gParticles[instanceId];
    if (particle.padding <= 0.0f)
    {
        SlimeDepthVertexOutput inactiveOutput;
        inactiveOutput.position = float32_t4(2.0f, 2.0f, 0.0f, 1.0f);
        inactiveOutput.localPosition = float32_t2(2.0f, 2.0f);
        inactiveOutput.centerDepth = 1.0f;
        return inactiveOutput;
    }

    float32_t renderRadius = particleRadius * 1.35f;
    float32_t2 local = kQuadPositions[vertexId];
    float32_t3 worldPos = particle.position +
        cameraRight * (local.x * renderRadius) +
        cameraUp * (local.y * renderRadius);

    float32_t4 clipPos = mul(float32_t4(worldPos, 1.0f), viewProjection);
    float32_t4 centerClip = mul(float32_t4(particle.position, 1.0f), viewProjection);

    SlimeDepthVertexOutput output;
    output.position = clipPos;
    output.localPosition = local;
    output.centerDepth = centerClip.z / max(centerClip.w, 0.0001f);
    return output;
}
