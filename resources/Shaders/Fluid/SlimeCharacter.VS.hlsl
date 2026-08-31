#include "SlimeCharacter.hlsli"

SlimeCharacterVertexOutput main(uint vertexId : SV_VertexID)
{
    static const float2 quad[6] = {
        float2(-1.08f, -1.02f),
        float2(-1.08f, 1.06f),
        float2(1.08f, -1.02f),
        float2(1.08f, -1.02f),
        float2(-1.08f, 1.06f),
        float2(1.08f, 1.06f)
    };

    float time = gSlime.cameraPositionAndTime.w;
    float wobble = gSlime.radiiAndWobble.w;
    float2 local = quad[vertexId];

    float3 right = normalize(gSlime.cameraRightAndRadius.xyz);
    float3 up = normalize(gSlime.cameraUpAndHeight.xyz);
    float3 forward = normalize(gSlime.forwardAndSpeed.xyz);

    float zStretch = saturate((gSlime.radiiAndWobble.z - 0.95f) / 1.25f);
    float radius = max(
        gSlime.cameraRightAndRadius.w,
        lerp(gSlime.cameraRightAndRadius.w, gSlime.radiiAndWobble.z * 0.78f, zStretch));
    radius *= 1.0f + wobble * 0.10f;
    float height = gSlime.cameraUpAndHeight.w * (1.0f - wobble * 0.05f - zStretch * 0.32f);
    float wave =
        sin(time * 7.0f + local.y * 2.4f) *
        (1.0f - saturate(local.y * 0.5f + 0.5f)) *
        wobble * 0.035f;
    float footSlide = (1.0f - saturate(local.y + 1.0f)) * wobble * 0.08f;

    float3 center =
        gSlime.positionAndGround.xyz +
        up * (height * 0.55f) +
        forward * (gSlime.radiiAndWobble.z * 0.07f);
    float3 worldPosition =
        center +
        right * ((local.x + wave) * radius) +
        up * (local.y * height) +
        forward * footSlide;

    SlimeCharacterVertexOutput output;
    output.position = mul(float4(worldPosition, 1.0f), gSlime.viewProjection);
    output.worldPosition = worldPosition;
    output.normal = float3(local.x, local.y, 1.0f);
    output.localPosition = float3(local, 0.0f);
    output.uv = local * 0.5f + 0.5f;
    return output;
}
