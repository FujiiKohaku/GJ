#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

float Hash21(float32_t2 position)
{
    return frac(
        sin(dot(position, float32_t2(127.1f, 311.7f))) *
        43758.5453f);
}

float ValueNoise(float32_t2 position)
{
    float32_t2 cell = floor(position);
    float32_t2 localPosition = frac(position);
    float32_t2 blend =
        localPosition * localPosition *
        (3.0f - 2.0f * localPosition);

    float bottomLeft = Hash21(cell);
    float bottomRight = Hash21(cell + float32_t2(1.0f, 0.0f));
    float topLeft = Hash21(cell + float32_t2(0.0f, 1.0f));
    float topRight = Hash21(cell + float32_t2(1.0f, 1.0f));

    float bottom = lerp(bottomLeft, bottomRight, blend.x);
    float top = lerp(topLeft, topRight, blend.x);
    return lerp(bottom, top, blend.y);
}

float FractalNoise(float32_t2 position)
{
    float result = 0.0f;
    float amplitude = 0.55f;
    float32_t2 samplePosition = position;

    for (int32_t octave = 0; octave < 4; ++octave)
    {
        result += ValueNoise(samplePosition) * amplitude;
        samplePosition =
            samplePosition * 2.03f + float32_t2(13.7f, 7.9f);
        amplitude *= 0.5f;
    }

    return result;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float edgeNoise = FractalNoise(texcoord * 3.2f);
    float detailNoise = FractalNoise(
        texcoord * 7.4f + float32_t2(5.3f, 9.1f));

    float32_t2 warp = float32_t2(
        ValueNoise(texcoord * 4.7f + 2.1f),
        ValueNoise(texcoord * 4.3f + 8.4f));
    warp = (warp - 0.5f) * 0.075f;

    float32_t2 warpedTexcoord = saturate(texcoord + warp);
    float sourceMask = SampleParticleTexture(warpedTexcoord).a;

    float32_t2 centered = texcoord - 0.5f;
    centered.x *= 1.3333333f;
    float distanceFromCenter = length(centered);
    float irregularRadius =
        0.345f + (edgeNoise - 0.5f) * 0.16f;
    float softShape = 1.0f - smoothstep(
        irregularRadius - 0.11f,
        irregularRadius,
        distanceFromCenter);

    float density = 0.42f + detailNoise * 0.68f;
    density *= 0.78f + edgeNoise * 0.32f;

    float alpha =
        sourceMask * softShape * density * input.color.a;
    if (alpha <= gMaterial.alphaReference)
    {
        discard;
    }

    float brightnessVariation = 0.68f + detailNoise * 0.40f;
    float32_t3 smokeColor =
        input.color.rgb * brightnessVariation;
    float32_t4 color =
        gMaterial.color * float32_t4(smokeColor, alpha);
    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
