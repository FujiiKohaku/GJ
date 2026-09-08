#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 uv = mul(
        float4(input.texcoord, 0.0f, 1.0f),
        gMaterial.uvTransform).xy;
    float4 textureColor = gTexture.Sample(gSampler, uv) * gMaterial.color;

    // Keep every lighting layer inside the icon's original alpha silhouette.
    float alphaMask = smoothstep(0.02f, 0.30f, textureColor.a);
    float2 texel = 1.0f / max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float neighborAlpha = min(
        min(gTexture.Sample(gSampler, uv + float2(texel.x, 0.0f)).a,
            gTexture.Sample(gSampler, uv - float2(texel.x, 0.0f)).a),
        min(gTexture.Sample(gSampler, uv + float2(0.0f, texel.y)).a,
            gTexture.Sample(gSampler, uv - float2(0.0f, texel.y)).a));

    // Cyan inner rim recreates the luminous edge of the screen-space slime.
    float innerRim = saturate((textureColor.a - neighborAlpha) * 3.2f);
    float luminance = dot(textureColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float aliveAmount = smoothstep(0.28f, 0.52f, luminance);
    float3 rimColor = lerp(
        float3(0.46f, 0.50f, 0.55f),
        float3(0.24f, 0.92f, 1.0f),
        aliveAmount);

    // A broad top sheen and a small white glint give the flat icon a rounded,
    // wet surface similar to the actual fluid player.
    float2 sheenPoint = (uv - float2(0.36f, 0.27f)) / float2(0.34f, 0.20f);
    float broadSheen =
        (1.0f - smoothstep(0.35f, 1.0f, length(sheenPoint))) * alphaMask;
    float2 glintPoint = (uv - float2(0.30f, 0.22f)) / float2(0.095f, 0.070f);
    float glint =
        (1.0f - smoothstep(0.25f, 1.0f, length(glintPoint))) * alphaMask;

    float3 color = textureColor.rgb;
    color = lerp(color, rimColor, innerRim * 0.72f);
    color += lerp(float3(0.15f, 0.17f, 0.20f),
                  float3(0.42f, 0.88f, 1.0f), aliveAmount) * broadSheen * 0.28f;
    color += float3(0.92f, 0.98f, 1.0f) * glint *
             lerp(0.28f, 0.58f, aliveAmount);

    output.color = float4(color, textureColor.a);
    return output;
}
