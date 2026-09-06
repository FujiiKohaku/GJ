#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float Luminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float strength = saturate(fantasyMenuStrength);
    float2 uv = input.texcoord;
    float4 source = gTexture.Sample(gSampler, uv);

    uint width;
    uint height;
    gTexture.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);

    // A compact circular blur keeps the paused world recognizable.
    float3 blurred = source.rgb * 2.0f;
    float totalWeight = 2.0f;
    static const float2 directions[8] = {
        float2(1.0f, 0.0f), float2(-1.0f, 0.0f),
        float2(0.0f, 1.0f), float2(0.0f, -1.0f),
        float2(0.707f, 0.707f), float2(-0.707f, 0.707f),
        float2(0.707f, -0.707f), float2(-0.707f, -0.707f)
    };
    [unroll]
    for (int index = 0; index < 8; ++index) {
        float2 offset = directions[index] * texel * 4.5f * strength;
        blurred += gTexture.Sample(gSampler, saturate(uv + offset)).rgb;
        totalWeight += 1.0f;
    }
    blurred /= totalWeight;

    // Cool green shadows and warm highlights echo moss, stone and old brass.
    float luminance = Luminance(blurred);
    float3 graded = blurred;
    float shadowMask = 1.0f - smoothstep(0.12f, 0.55f, luminance);
    float highlightMask = smoothstep(0.48f, 0.92f, luminance);
    graded *= lerp(float3(1.0f, 1.0f, 1.0f),
        float3(0.72f, 0.94f, 0.82f), shadowMask * 0.34f);
    graded += float3(0.13f, 0.085f, 0.025f) * highlightMask * 0.24f;
    graded = lerp(graded, luminance.xxx, 0.08f);

    float2 centered = (uv - 0.5f) * float2(1.12f, 1.0f);
    float edge = smoothstep(0.36f, 0.90f, length(centered));
    float3 forestEdge = float3(0.025f, 0.085f, 0.060f);
    graded = lerp(graded, forestEdge, edge * 0.48f);

    // A very soft central gold lift guides the eye toward the menu panel.
    float centerGlow = exp(-dot(centered, centered) * 3.4f);
    graded += float3(0.10f, 0.075f, 0.025f) * centerGlow;

    return float4(lerp(source.rgb, saturate(graded), strength), source.a);
}
