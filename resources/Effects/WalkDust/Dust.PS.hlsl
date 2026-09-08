#include "../Common/ParticlePixelCommon.hlsli"

float Hash21(float32_t2 value)
{
    return frac(sin(dot(value, float32_t2(127.1f, 311.7f))) * 43758.5453f);
}

float Noise(float32_t2 value)
{
    float32_t2 cell = floor(value);
    float32_t2 local = frac(value);
    local = local * local * (3.0f - 2.0f * local);
    return lerp(
        lerp(Hash21(cell), Hash21(cell + float32_t2(1.0f, 0.0f)), local.x),
        lerp(Hash21(cell + float32_t2(0.0f, 1.0f)), Hash21(cell + float32_t2(1.0f, 1.0f)), local.x),
        local.y);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 uv = GetParticleTextureCoordinate(input);
    float mask = SampleParticleTexture(uv).a;
    float32_t2 centered = uv - 0.5f;
    centered.x *= 1.25f;
    float edge = 1.0f - smoothstep(0.24f, 0.54f, length(centered));
    float noiseValue = Noise(uv * 8.0f + input.worldPosition.xz);
    float grain = 0.72f + noiseValue * 0.38f;
    float alpha = mask * edge * grain * input.color.a;
    // 砂煙は半透明なので、共通マテリアルの0.1判定では細部が消え過ぎる。
    if (alpha <= 0.015f)
    {
        discard;
    }

    // 青緑のスライム光を基調に、粒の一部へ紫の魔法感を混ぜる。
    float32_t3 fantasyShift = lerp(
        float32_t3(0.76f, 1.0f, 0.92f),
        float32_t3(0.92f, 0.68f, 1.0f),
        noiseValue);
    return MakeParticlePixelOutput(
        gMaterial.color * float32_t4(input.color.rgb * fantasyShift * grain, alpha));
}
