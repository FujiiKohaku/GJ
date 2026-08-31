#include "SlimeCharacter.hlsli"

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 345.45f));
    p += dot(p, p + 34.23f);
    return frac(p.x * p.y);
}

float Noise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    return lerp(
        lerp(Hash21(cell), Hash21(cell + float2(1.0f, 0.0f)), f.x),
        lerp(Hash21(cell + float2(0.0f, 1.0f)), Hash21(cell + 1.0f), f.x),
        f.y);
}

float Eye(float2 p, float2 center, float2 radius)
{
    float2 d = (p - center) / radius;
    return 1.0f - smoothstep(0.72f, 1.0f, dot(d, d));
}

float Mouth(float2 p)
{
    float smileY = -0.16f + p.x * p.x * 1.9f;
    float mouthLine = abs(p.y - smileY);
    float span = 1.0f - smoothstep(0.16f, 0.25f, abs(p.x));
    return (1.0f - smoothstep(0.016f, 0.030f, mouthLine)) * span;
}

float4 main(SlimeCharacterVertexOutput input) : SV_TARGET
{
    float2 p = input.localPosition.xy;
    float zStretch = saturate((gSlime.radiiAndWobble.z - 0.95f) / 1.25f);
    float bodyDistance = length(float2(
        p.x * lerp(0.92f, 0.72f, zStretch),
        (p.y + 0.10f) * lerp(0.82f, 1.08f, zStretch)));
    float bottomMask = smoothstep(-1.02f, -0.88f, p.y);
    float silhouette = (1.0f - smoothstep(0.96f, 1.03f, bodyDistance)) * bottomMask;
    if (silhouette <= 0.01f) {
        discard;
    }

    float topRound = saturate((p.y + 0.90f) * 0.58f);
    float3 normal = normalize(float3(
        p.x * 0.62f,
        (p.y + 0.10f) * 0.46f,
        sqrt(saturate(1.0f - dot(float2(p.x * 0.62f, (p.y + 0.10f) * 0.46f),
            float2(p.x * 0.62f, (p.y + 0.10f) * 0.46f))))));
    float3 viewDirection =
        normalize(gSlime.cameraPositionAndTime.xyz - input.worldPosition);
    float3 lightDirection = normalize(float3(-0.35f, 0.75f, -0.58f));
    float3 halfVector = normalize(lightDirection + viewDirection);

    float ndotv = saturate(dot(normal, viewDirection));
    float ndotl = saturate(dot(normal, lightDirection));
    float rim = pow(1.0f - ndotv, 1.7f);
    float outline = smoothstep(0.76f, 0.98f, bodyDistance);
    float blackRim = saturate(outline + rim * 0.48f);

    float height = saturate(p.y * 0.5f + 0.5f);
    float centerGlow = 1.0f - smoothstep(0.12f, 0.78f,
        length(float2(p.x * 0.76f, (p.y + 0.08f) * 0.70f)));

    float time = gSlime.cameraPositionAndTime.w;
    float internalNoise =
        Noise(p * 4.0f + float2(time * 0.22f, -time * 0.31f));

    float3 baseColor = gSlime.color.rgb;
    float3 deepColor = baseColor * float3(0.18f, 0.42f, 0.22f);
    float3 brightCore = saturate(baseColor * float3(1.55f, 1.95f, 1.35f));
    float3 surfaceColor =
        lerp(deepColor, brightCore, centerGlow * 0.78f + topRound * 0.13f);
    surfaceColor += baseColor * internalNoise * 0.10f;
    surfaceColor *= 0.56f + ndotl * 0.30f + ndotv * 0.22f;

    float specSoft = pow(saturate(dot(normal, halfVector)), 62.0f) * 0.65f;
    float specHard = pow(saturate(dot(normal, halfVector)), 180.0f) * 2.6f;
    float bottomShine = pow(saturate(dot(normal, normalize(float3(-0.35f, -0.55f, -0.55f)))), 120.0f) * 1.7f;
    float fresnelHighlight = pow(1.0f - ndotv, 4.2f) * 0.42f;
    float whiteCrescent =
        (1.0f - smoothstep(0.012f, 0.036f, abs((p.y + 0.64f) + p.x * 0.17f))) *
        smoothstep(-0.72f, -0.55f, p.y) *
        (1.0f - smoothstep(0.00f, 0.48f, p.x));
    float3 highlight =
        float3(1.0f, 1.0f, 0.92f) *
            (specSoft + specHard + bottomShine + whiteCrescent * 1.2f) +
        baseColor * fresnelHighlight;

    float2 faceUv = p;
    float faceMask =
        smoothstep(-0.34f, -0.05f, p.y) *
        (1.0f - smoothstep(0.35f, 0.62f, p.y)) *
        (1.0f - smoothstep(0.52f, 0.72f, abs(p.x)));
    float eyes =
        Eye(faceUv, float2(-0.17f, 0.12f), float2(0.046f, 0.065f)) +
        Eye(faceUv, float2(0.17f, 0.12f), float2(0.046f, 0.065f));
    float mouth = Mouth(faceUv);
    float face = saturate(eyes + mouth) * faceMask;

    float3 color = surfaceColor + highlight;
    color = lerp(color, float3(0.005f, 0.018f, 0.012f), blackRim * 0.88f);
    color = lerp(color, float3(0.0f, 0.03f, 0.04f), face);
    color = min(color, float3(1.35f, 1.45f, 1.30f));

    float alpha =
        gSlime.color.a *
        silhouette *
        saturate(0.68f + rim * 0.24f + centerGlow * 0.12f);
    alpha = max(alpha, face * 0.96f);
    return float4(color, alpha);
}
