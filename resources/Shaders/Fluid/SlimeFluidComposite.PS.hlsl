struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

Texture2D<float32_t4> gSceneColor : register(t0);
Texture2D<float32_t> gFluidDepth : register(t1);
Texture2D<float32_t> gFluidThickness : register(t2);
SamplerState gSampler : register(s0);

cbuffer SlimeFluidCompositeParameter : register(b0)
{
    float32_t2 texelSize;
    float32_t refractionStrength;
    float32_t translucency;
    float32_t3 slimeColor;
    float32_t specularStrength;
    float32_t fresnelStrength;
    float32_t floorHeightWorld;
    float32_t2 padding0;
    float32_t4x4 gInvViewProj;
};

float32_t4 main(VertexShaderOutput input) : SV_TARGET
{
    float32_t dx_analytic = 0.0f;
    float32_t dy_analytic = 0.0f;
    float32_t wSum = 0.0f;
    float32_t spread = 4.0f;
    float32_t density = 0.0f;
    float32_t sigma = 10.0f;

    // 11x11 blur for a stable slime silhouette.
    for(int y = -5; y <= 5; ++y) {
        for(int x = -5; x <= 5; ++x) {
            float32_t w = exp(-float32_t(x * x + y * y) / sigma);
            float32_t2 sampleUv =
                input.texcoord + float32_t2(x, y) * texelSize * spread;
            float32_t sampledThickness =
                saturate(gFluidThickness.SampleLevel(gSampler, sampleUv, 0));
            float32_t sampledDepth =
                gFluidDepth.SampleLevel(gSampler, sampleUv, 0);
            float32_t sampledSurface =
                1.0f - smoothstep(0.995f, 0.9995f, sampledDepth);
            float32_t sampledDensity =
                max(sampledThickness, sampledSurface * 0.32f);
            
            density += sampledDensity * w;
            wSum += w;
            
            dx_analytic += (float32_t(x) / 5.0f) * w * sampledDensity;
            dy_analytic += (float32_t(y) / 5.0f) * w * sampledDensity;
        }
    }
    density /= wSum;
    dx_analytic /= wSum;
    dy_analytic /= wSum;

    float32_t threshold = 0.025f;
    float32_t edgeAA = smoothstep(threshold - 0.010f, threshold + 0.025f, density);

    if (edgeAA <= 0.0f) {
        return gSceneColor.SampleLevel(gSampler, input.texcoord, 0);
    }

    float32_t depthRight = gFluidDepth.SampleLevel(gSampler, input.texcoord + float32_t2(texelSize.x * 2.0f, 0.0f), 0);
    float32_t depthLeft = gFluidDepth.SampleLevel(gSampler, input.texcoord - float32_t2(texelSize.x * 2.0f, 0.0f), 0);
    float32_t depthBottom = gFluidDepth.SampleLevel(gSampler, input.texcoord + float32_t2(0.0f, texelSize.y * 2.0f), 0);
    float32_t depthTop = gFluidDepth.SampleLevel(gSampler, input.texcoord - float32_t2(0.0f, texelSize.y * 2.0f), 0);

    float32_t dx = -dx_analytic * 4.0f + (depthLeft - depthRight) * 140.0f;
    float32_t dy = -dy_analytic * 4.0f + (depthBottom - depthTop) * 140.0f;

    float32_t normalStrength = 0.72f;
    float32_t2 normalXY = float32_t2(dx, dy) * normalStrength;
    float32_t xySq = saturate(dot(normalXY, normalXY));
    float32_t dz = -sqrt(1.0f - xySq);
    float32_t3 alphaNormal = float32_t3(normalXY.x, normalXY.y, dz);

    // Global dome correction (wider sampling for smoother dome)
    float32_t2 offX_far = float32_t2(texelSize.x * 40.0f, 0.0f);
    float32_t2 offY_far = float32_t2(0.0f, texelSize.y * 40.0f);
    float32_t aRight = saturate(gFluidThickness.SampleLevel(gSampler, input.texcoord + offX_far, 0).r);
    float32_t aLeft  = saturate(gFluidThickness.SampleLevel(gSampler, input.texcoord - offX_far, 0).r);
    float32_t aBottom = saturate(gFluidThickness.SampleLevel(gSampler, input.texcoord + offY_far, 0).r);
    float32_t aTop    = saturate(gFluidThickness.SampleLevel(gSampler, input.texcoord - offY_far, 0).r);

    float32_t dx_far = (aLeft - aRight);
    float32_t dy_far = (aBottom - aTop);
    float32_t2 farNormalXY = float32_t2(dx_far, dy_far) * 0.4f; 
    float32_t farXySq = saturate(dot(farNormalXY, farNormalXY));
    float32_t3 farNormal = float32_t3(farNormalXY.x, farNormalXY.y, -sqrt(1.0f - farXySq));

    // Use more dome normal to suppress particle-level bumps
    float32_t blendFactor = smoothstep(0.2f, 0.6f, density);
    float32_t3 normal = normalize(lerp(alphaNormal, farNormal, blendFactor));

    float32_t3 viewDir = float32_t3(0.0f, 0.0f, -1.0f);
    float32_t3 lightDir = normalize(float32_t3(0.35f, 0.75f, -0.8f));
    float32_t NdotV = max(dot(normal, viewDir), 0.0f);
    float32_t fresnel = pow(1.0f - NdotV, 2.2f);

    // 上向き法線(頭部・上部)のみハイライト・反射を許可するマスク（足元の白反射を解消）
    float32_t topHighlightMask = saturate(1.0f - smoothstep(-0.15f, 0.25f, normal.y));

    float32_t2 refractOffset =
        normal.xy * (refractionStrength * 0.45f + fresnel * 0.012f);
    float32_t3 refracted = gSceneColor.SampleLevel(gSampler, input.texcoord + refractOffset, 0).rgb;

    float32_t thickness = saturate(density * 2.2f);
    float32_t3 jellyColor =
        slimeColor * (0.58f + thickness * 0.42f) +
        float32_t3(0.03f, 0.10f, 0.03f);

    float32_t3 halfVec = normalize(lightDir + viewDir);
    float32_t spec =
        pow(max(dot(normal, halfVec), 0.0f), 300.0f) *
        specularStrength * topHighlightMask;

    float32_t3 finalColor =
        lerp(jellyColor, refracted * jellyColor, translucency * 0.28f);
    finalColor += jellyColor * 0.34f;

    float32_t3 rimColor = float32_t3(0.8f, 1.0f, 0.9f);
    finalColor += rimColor * (fresnel * fresnelStrength * topHighlightMask);

    finalColor += float32_t3(1.0f, 1.0f, 1.0f) * spec;

    float32_t alpha = saturate(0.86f + thickness * 0.18f + spec * 0.15f);

    float32_t outlineOuter =
        1.0f - smoothstep(threshold + 0.02f, threshold + 0.12f, density);
    float32_t outlineInner =
        smoothstep(threshold - 0.006f, threshold + 0.018f, density);
    float32_t outlineFactor = saturate(outlineOuter * outlineInner);
    finalColor =
        lerp(finalColor, float32_t3(0.0f, 0.10f, 0.04f), outlineFactor * 0.62f);
    alpha = lerp(alpha, 0.98f, outlineFactor);

    float32_t3 background = gSceneColor.SampleLevel(gSampler, input.texcoord, 0).rgb;
    float32_t3 result = lerp(background, finalColor, alpha * edgeAA);

    return float32_t4(result, 1.0f);
}
