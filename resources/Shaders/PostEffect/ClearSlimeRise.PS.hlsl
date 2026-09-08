#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float Hash(float value)
{
    return frac(sin(value * 127.1f) * 43758.5453f);
}

float Circle(float2 uv, float2 center, float radius)
{
    float2 delta = uv - center;
    delta.x *= 16.0f / 9.0f;
    return 1.0f - smoothstep(radius * 0.82f, radius, length(delta));
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 scene = gTexture.Sample(gSampler, input.texcoord);
    float progress = saturate(slimeScreenProgress);
    if (progress <= 0.0f) {
        return scene;
    }

    float2 uv = input.texcoord;
    float surface = lerp(1.10f, -0.12f, progress);
    float wave =
        sin(uv.x * 15.0f + time * 2.1f) * 0.018f +
        sin(uv.x * 31.0f - time * 1.35f) * 0.009f;
    float coverage = smoothstep(
        surface + wave - 0.012f, surface + wave + 0.012f, uv.y);

    [unroll]
    for (uint index = 0; index < 9; ++index) {
        float centerX = (float(index) + 0.5f) / 9.0f;
        centerX += sin(time * (0.55f + Hash(float(index)) * 0.35f) +
                       float(index) * 1.7f) * 0.018f;
        float radius = 0.035f + Hash(float(index) + 4.0f) * 0.035f;
        float centerY = surface + wave - radius * 0.30f;
        coverage = max(
            coverage, Circle(uv, float2(centerX, centerY), radius));
    }

    float edgeDistance = abs(uv.y - (surface + wave));
    float edgeHighlight = coverage *
        (1.0f - smoothstep(0.0f, 0.045f, edgeDistance));
    float depth = saturate((uv.y - surface) * 0.85f);
    float3 slimeColor = lerp(
        float3(0.12f, 0.70f, 1.00f),
        float3(0.02f, 0.28f, 0.66f),
        depth);
    slimeColor += float3(0.20f, 0.30f, 0.38f) * edgeHighlight;

    float bubbles = 0.0f;
    [unroll]
    for (uint bubble = 0; bubble < 7; ++bubble) {
        float seed = float(bubble);
        float x = Hash(seed + 11.0f);
        float speed = 0.035f + Hash(seed + 19.0f) * 0.045f;
        float y = frac(Hash(seed + 23.0f) + time * speed);
        float radius = 0.008f + Hash(seed + 31.0f) * 0.010f;
        float ring = Circle(uv, float2(x, y), radius) -
            Circle(uv, float2(x, y), radius * 0.58f);
        bubbles = max(bubbles, saturate(ring));
    }
    slimeColor += bubbles * coverage * float3(0.16f, 0.28f, 0.36f);

    return float4(lerp(scene.rgb, slimeColor, coverage), scene.a);
}
