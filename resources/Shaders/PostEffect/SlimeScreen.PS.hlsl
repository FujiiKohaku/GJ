#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

static const uint kSplatCount = 12;
static const float2 kCenters[kSplatCount] = {
    float2(0.18f, 0.25f), float2(0.78f, 0.20f),
    float2(0.48f, 0.38f), float2(0.90f, 0.55f),
    float2(0.08f, 0.63f), float2(0.66f, 0.72f),
    float2(0.31f, 0.82f), float2(0.57f, 0.12f),
    float2(0.24f, 0.51f), float2(0.82f, 0.86f),
    float2(0.52f, 0.67f), float2(0.50f, 0.48f)
};

float Hash(float value)
{
    return frac(sin(value * 91.345f) * 47453.5453f);
}

float3 RandomSlimeColor(uint index)
{
    float hue = Hash(float(index) * 2.17f + paintSeed * 13.31f);
    float3 rgb = saturate(abs(frac(hue + float3(0.0f, 0.6667f, 0.3333f)) * 6.0f - 3.0f) - 1.0f);
    rgb = rgb * rgb * (3.0f - 2.0f * rgb);
    return lerp(float3(1.0f, 1.0f, 1.0f), rgb, 0.78f) * 0.92f;
}

float SlimeSplat(float2 uv, uint index, float growth)
{
    if (growth <= 0.0f) return 0.0f;
    float2 delta = uv - kCenters[index];
    delta.x *= 16.0f / 9.0f;

    const float angle = atan2(delta.y, delta.x);
    const float wobble =
        sin(angle * (5.0f + float(index % 3)) + float(index) * 1.7f) * 0.055f +
        sin(angle * 11.0f - float(index) * 0.8f) * 0.025f;
    const float baseRadius = lerp(0.24f, 0.36f, Hash(float(index) + 2.0f));
    const float impact = sin(saturate(growth) * 3.14159265f);
    float2 squashed = delta;
    squashed.x /= 1.0f + impact * 0.28f;
    squashed.y /= 1.0f - impact * 0.12f;

    float distanceToCenter = length(squashed);
    float radius = baseRadius * smoothstep(0.0f, 0.72f, growth);
    float blob = 1.0f - smoothstep(radius * (0.88f + wobble), radius, distanceToCenter);

    // 衝突後、下側だけをゆっくり垂らす。
    float dripAge = saturate((growth - 0.48f) * 1.9f);
    float dripX = abs(delta.x + sin(float(index) * 2.3f) * radius * 0.28f);
    float dripWidth = radius * 0.20f;
    float dripTop = radius * 0.15f;
    float dripBottom = radius * (0.35f + dripAge * 1.75f);
    float verticalDrip = smoothstep(dripBottom, dripBottom - 0.045f, delta.y) *
        smoothstep(-dripTop, -dripTop + 0.04f, delta.y);
    float drip = (1.0f - smoothstep(dripWidth * 0.55f, dripWidth, dripX)) *
        verticalDrip * dripAge;
    return saturate(blob + drip);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 scene = gTexture.Sample(gSampler, input.texcoord);
    // During the initial flight the lens is still clean.
    if (slimeScreenProgress <= 0.0f) return scene;
    float3 slimeColor = RandomSlimeColor(0);
    float coverage = 0.0f;
    float highlight = 0.0f;

    [unroll]
    for (uint index = 0; index < kSplatCount; ++index) {
        float start = float(index) * 0.045f;
        float growth = saturate((slimeScreenProgress - start) / 0.19f);
        float splat = SlimeSplat(input.texcoord, index, growth);
        if (splat > coverage) {
            coverage = splat;
            slimeColor = RandomSlimeColor(index);
        }
        highlight = max(highlight, splat * (1.0f - smoothstep(0.0f, 0.07f, length(input.texcoord - kCenters[index]))));
    }

    // 終盤の大きな一発で、小さな染みの隙間もスライム膜として塞ぐ。
    float finalImpact = smoothstep(0.62f, 0.92f, slimeScreenProgress);
    float2 finalDelta = input.texcoord - float2(0.50f, 0.48f);
    finalDelta.x *= 16.0f / 9.0f;
    float finalAngle = atan2(finalDelta.y, finalDelta.x);
    float finalEdge = sin(finalAngle * 7.0f + time * 0.35f) * 0.035f +
        sin(finalAngle * 13.0f) * 0.018f;
    float finalRadius = lerp(0.04f, 1.18f, finalImpact);
    float finalSplat = 1.0f - smoothstep(
        finalRadius * (0.94f + finalEdge),
        finalRadius,
        length(finalDelta));
    finalSplat *= step(0.0001f, finalImpact);
    if (finalSplat > coverage) {
        coverage = finalSplat;
        slimeColor = RandomSlimeColor(kSplatCount);
    }

    // スライム内部は完全不透明。背景色や屈折像を混ぜない。
    float3 coated = slimeColor;
    coated += float3(0.18f, 0.18f, 0.18f) * highlight;
    float3 result = lerp(scene.rgb, coated, coverage);

    // 最後の大きな衝突後に暗転し、シーン切り替えを隠す。
    float blackout = smoothstep(0.965f, 1.0f, slimeScreenProgress);
    result = lerp(result, float3(0.0f, 0.008f, 0.002f), blackout);
    return float4(result, scene.a);
}
