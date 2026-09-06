#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

float Hash21(float2 value)
{
    value = frac(value * float2(123.34f, 456.21f));
    value += dot(value, value + 45.32f);
    return frac(value.x * value.y);
}

float ValueNoise(float2 value)
{
    float2 cell = floor(value);
    float2 local = frac(value);
    local = local * local * (3.0f - 2.0f * local);
    float a = Hash21(cell);
    float b = Hash21(cell + float2(1.0f, 0.0f));
    float c = Hash21(cell + float2(0.0f, 1.0f));
    float d = Hash21(cell + 1.0f);
    return lerp(lerp(a, b, local.x), lerp(c, d, local.x), local.y);
}

float RoundedRectangleDistance(float2 uv, float2 spriteSize)
{
    float2 local = (uv - 0.5f) * spriteSize;
    float2 halfSize = spriteSize * 0.5f - 1.5f;
    float radius = min(min(spriteSize.x, spriteSize.y) * 0.15f, 18.0f);
    float2 corner = abs(local) - (halfSize - radius);
    return length(max(corner, 0.0f)) +
        min(max(corner.x, corner.y), 0.0f) - radius;
}

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 uv = input.texcoord;
    float2 transformedUv = mul(
        float4(uv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 textureColor = gTexture.Sample(gSampler, transformedUv);
    float2 spriteSize = max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float distanceToEdge = RoundedRectangleDistance(uv, spriteSize);
    float coverage = 1.0f - smoothstep(-1.0f, 1.0f, distanceToEdge);
    clip(coverage - 0.001f);

    float2 grainPosition = uv * spriteSize * 0.075f;
    float broadNoise = ValueNoise(grainPosition * 0.18f + 7.3f);
    float fineNoise = ValueNoise(grainPosition * 1.7f + 31.1f);
    float stoneGrain = (broadNoise - 0.5f) * 0.16f +
        (fineNoise - 0.5f) * 0.055f;

    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;
    baseColor *= 0.88f + uv.y * 0.13f + stoneGrain;

    float insideDistance = max(-distanceToEdge, 0.0f);
    float outerBorder = 1.0f - smoothstep(2.0f, 6.0f, insideDistance);
    float engravedLine = 1.0f - smoothstep(
        0.8f, 2.0f, abs(insideDistance - 9.0f));

    float time = gFrame.elapsedTime * gEffect.speed + gEffect.phase;
    float shimmer = 0.82f + 0.18f * sin(time + uv.x * 5.0f);
    float3 brass = float3(0.66f, 0.43f, 0.15f) * shimmer;
    brass += fineNoise * float3(0.075f, 0.045f, 0.008f);
    float borderMask = saturate(outerBorder + engravedLine * 0.46f);
    baseColor = lerp(baseColor, brass, borderMask);

    // Moss gathers irregularly along the upper and side edges of the stone.
    float edgeBand = 1.0f - smoothstep(5.0f, 17.0f, insideDistance);
    float upperBias = 1.0f - smoothstep(0.08f, 0.58f, uv.y);
    float sideBias = 1.0f - smoothstep(
        0.03f, 0.34f, min(uv.x, 1.0f - uv.x));
    float mossNoise = ValueNoise(grainPosition * 0.72f + float2(4.0f, 19.0f));
    float mossMask = edgeBand * saturate(upperBias + sideBias * 0.38f) *
        smoothstep(gEffect.threshold, 0.82f, mossNoise);
    float3 moss = lerp(
        float3(0.08f, 0.20f, 0.09f),
        float3(0.25f, 0.42f, 0.13f),
        broadNoise);
    baseColor = lerp(baseColor, moss, mossMask * gEffect.strength);

    // Small brass studs suggest the corners of an old spellbook cover.
    float2 cornerUv = abs(uv - 0.5f);
    float2 studCenter = float2(0.455f, 0.405f);
    float studDistance = length((cornerUv - studCenter) *
        float2(spriteSize.x / spriteSize.y, 1.0f));
    float stud = 1.0f - smoothstep(0.020f, 0.035f, studDistance);
    baseColor = lerp(baseColor, brass * 1.18f, stud);

    output.color = float4(
        saturate(baseColor),
        gMaterial.color.a * textureColor.a * coverage);
    return output;
}
