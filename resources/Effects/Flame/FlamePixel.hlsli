#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"
ConstantBuffer<ParticleRenderParameter> gRender : register(b2);

float FlameNoise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float4 seeds = float4(dot(cell, float2(127.1f, 311.7f)),
        dot(cell + float2(1, 0), float2(127.1f, 311.7f)),
        dot(cell + float2(0, 1), float2(127.1f, 311.7f)),
        dot(cell + 1.0f, float2(127.1f, 311.7f)));
    float4 n = frac(sin(seeds) * 43758.5453f);
    return lerp(lerp(n.x, n.y, f.x), lerp(n.z, n.w, f.x), f.y);
}

float FlameFbm(float2 p)
{
    float value = 0.0f;
    float weight = 0.57f;
    for (int octave = 0; octave < 3; ++octave) {
        value += FlameNoise(p) * weight;
        p = p * 2.03f + float2(7.3f, 13.1f);
        weight *= 0.5f;
    }
    return value;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    // Board UV has y=0 at its top; work with height measured from its base.
    float2 uv = float2(input.texcoord.x, 1.0f - input.texcoord.y);
    float age = input.normal.z;
    float seed = input.normal.y;
    float2 flow = uv * float2(4.0f, 3.0f) + float2(seed, seed * 0.37f);
    flow -= input.normal.x * float2(gRender.uvScrollSpeedX, gRender.uvScrollSpeedY);
    float noise = FlameFbm(flow);
    float mask;
    float3 color;
#if FLAME_LAYER == 3
    float2 centeredUv = (uv - 0.5f) * 2.0f;
    mask = 1.0f - smoothstep(0.1f, 0.9f, length(centeredUv));
    color = input.color.rgb;
#elif FLAME_LAYER == 2
    float2 centeredUv = (uv - 0.5f) * 2.0f;
    mask = 1.0f - smoothstep(0.3f, 0.95f, length(centeredUv) + (noise - 0.5f) * 0.35f);
    mask *= smoothstep(0.15f, 0.65f, noise);
    color = input.color.rgb * (0.65f + noise * 0.5f);
#else
    // Soft circular particles; noise varies their density without carving tongues.
    float radius = length((uv - 0.5f) * 2.0f);
    mask = 1.0f - smoothstep(0.15f, 0.95f, radius);
    mask *= (0.8f + noise * 0.2f) * saturate(1.0f - gRender.dissolveThreshold);
    float heat = saturate(1.0f - radius * 0.85f - age * 0.2f + (noise - 0.5f) * 0.1f);
    color = lerp(float3(0.9f, 0.07f, 0.008f), float3(1.0f, 0.46f, 0.035f), smoothstep(0.12f, 0.65f, heat));
    color = lerp(color, float3(1.0f, 0.92f, 0.42f), smoothstep(0.6f, 1.0f, heat));
    color *= input.color.rgb;
#if FLAME_LAYER == 1
    color = lerp(float3(1.0f, 0.5f, 0.06f), float3(1.0f, 0.98f, 0.65f), heat) * input.color.rgb;
#endif
#endif
    float4 source = SampleParticleTexture(uv);
    float alpha = mask * input.color.a * source.a * gMaterial.color.a;
    // The shared material clips at 0.1; these soft flames and smoke need a lower cutoff.
    if (alpha <= 0.001f) {
        discard;
    }
    color *= source.rgb * gMaterial.color.rgb * gRender.emissionStrength;
    return MakeParticlePixelOutput(ApplyParticleFog(float4(color, alpha), input));
}
