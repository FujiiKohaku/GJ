#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

float SampleAlpha(float2 sourceUv)
{
    float2 uv = mul(
        float4(sourceUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    return gTexture.Sample(gSampler, uv).a;
}

void LayerOver(inout float3 premultipliedColor, inout float alpha,
               float3 layerColor, float layerAlpha)
{
    premultipliedColor = layerColor * layerAlpha +
        premultipliedColor * (1.0f - layerAlpha);
    alpha = layerAlpha + alpha * (1.0f - layerAlpha);
}

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 sourceUv = input.texcoord;
    float2 uv = mul(
        float4(sourceUv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 source = gTexture.Sample(gSampler, uv);
    float2 texel = 1.0f / max(gEffect.spriteSize, float2(1.0f, 1.0f));

    float nearAlpha = source.a;
    float farAlpha = source.a;
    static const float kTwoPi = 6.28318530718f;
    [unroll]
    for (int index = 0; index < 12; ++index) {
        float angle = kTwoPi * float(index) / 12.0f;
        float2 direction = float2(cos(angle), sin(angle));
        nearAlpha = max(nearAlpha, SampleAlpha(sourceUv + direction * texel * 3.5f));
        farAlpha = max(farAlpha, SampleAlpha(sourceUv + direction * texel * 8.0f));
    }

    float2 shadowOffset = gEffect.direction * texel * gEffect.amplitude;
    float shadowAlpha = SampleAlpha(sourceUv - shadowOffset) * 0.68f;
    float outlineAlpha = saturate(nearAlpha - source.a) * 0.98f;
    float glowAlpha = saturate(farAlpha - nearAlpha * 0.72f) * 0.42f;

    float shimmer = 0.92f + 0.08f * sin(
        gFrame.elapsedTime * gEffect.speed + sourceUv.x * 5.5f);
    float3 shadowColor = float3(0.008f, 0.020f, 0.016f);
    float3 glowColor = float3(0.20f, 0.50f, 0.18f) * shimmer;
    float3 outlineColor = float3(0.82f, 0.69f, 0.38f) * shimmer;

    float3 result = shadowColor * shadowAlpha;
    float resultAlpha = shadowAlpha;
    LayerOver(result, resultAlpha, glowColor, glowAlpha * gEffect.strength);
    LayerOver(result, resultAlpha, outlineColor, outlineAlpha * gEffect.strength);

    // Lift the dark source colors while retaining the original hand-drawn palette.
    float3 liftedSource = saturate(
        source.rgb * 1.34f + float3(0.075f, 0.080f, 0.045f));
    float greenDominance = saturate(
        (source.g - max(source.r, source.b)) * 2.5f);
    liftedSource += float3(0.025f, 0.105f, 0.010f) * greenDominance;
    LayerOver(result, resultAlpha, saturate(liftedSource), source.a);

    resultAlpha *= gMaterial.color.a;
    result *= gMaterial.color.a;
    output.color = float4(
        result / max(resultAlpha, 0.0001f),
        resultAlpha);
    return output;
}
