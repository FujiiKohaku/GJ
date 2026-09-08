#include "../Common/SpritePixelCommon.hlsli"

float4 main(SpriteVertexOutput input) : SV_TARGET
{
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 textureColor = gTexture.Sample(gSampler, uv);
    return textureColor * gMaterial.color;
}
