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
    float32_t2 local = kQuadPositions[vertexId];
    float32_t3 worldPosition =
        particle.position +
        cameraRight * (local.x * particleRadius) +
        cameraUp * (local.y * particleRadius);

    float32_t4 centerClip =
        mul(float32_t4(particle.position, 1.0f), viewProjection);

    SlimeDepthVertexOutput output;
    output.position = mul(float32_t4(worldPosition, 1.0f), viewProjection);
    output.localPosition = local;
    output.centerDepth = centerClip.z / max(centerClip.w, 0.0001f);
    return output;
}
