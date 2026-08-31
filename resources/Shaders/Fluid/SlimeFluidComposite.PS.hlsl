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
    float32_t padding0;
    float32_t padding1;
    float32_t padding2;
};

float32_t4 main(VertexShaderOutput input) : SV_TARGET
{
    float32_t dx_analytic = 0.0f;
    float32_t dy_analytic = 0.0f;
    float32_t wSum = 0.0f;
    float32_t spread = 12.0f;
    float32_t density = 0.0f;
    float32_t sigma = 8.0f;

    // 11x11 wide blur for smooth density field
    for(int y = -5; y <= 5; ++y) {
        for(int x = -5; x <= 5; ++x) {
            float32_t w = exp(-float32_t(x * x + y * y) / sigma);
            float32_t4 samp = gFluidThickness.SampleLevel(gSampler, input.texcoord + float32_t2(x, y) * texelSize * spread, 0);
            
            float32_t sampledDensity = saturate(samp.r);
            density += sampledDensity * w;
            wSum += w;
            
            dx_analytic += (float32_t(x) / 5.0f) * w * sampledDensity;
            dy_analytic += (float32_t(y) / 5.0f) * w * sampledDensity;
        }
    }
    density /= wSum;
    dx_analytic /= wSum;
    dy_analytic /= wSum;

    float32_t threshold = 0.04f;
    float32_t edgeAA = smoothstep(threshold - 0.015f, threshold + 0.015f, density);
    if (edgeAA <= 0.0f) {
        return gSceneColor.SampleLevel(gSampler, input.texcoord, 0);
    }

    float32_t dx = -dx_analytic * 6.0f;
    float32_t dy = -dy_analytic * 6.0f;

    float32_t normalStrength = 1.0f;
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

    // NeoEngine Lighting style
    float32_t3 viewDir = float32_t3(0.0f, 0.0f, -1.0f);
    float32_t3 lightDir = normalize(float32_t3(0.4f, 0.6f, -1.0f));
    float32_t NdotV = max(dot(normal, viewDir), 0.0f);
    float32_t fresnel = pow(1.0f - NdotV, 2.5f);

    // 屈折
    float32_t2 refractOffset = normal.xy * (0.04f + fresnel * 0.02f);
    float32_t3 refracted = gSceneColor.SampleLevel(gSampler, input.texcoord + refractOffset, 0).rgb;

    float32_t thickness = NdotV; // 中心のほうが厚いと仮定
    float32_t3 slimeBase = float32_t3(0.05f, 0.8f, 0.2f);
    float32_t3 jellyColor = slimeBase * (0.8f - thickness * 0.3f);

    float32_t3 halfVec = normalize(lightDir + viewDir);
    float32_t spec = pow(max(dot(normal, halfVec), 0.0f), 400.0f); // 鋭いハイライト

    // ベースカラーと屈折の合成
    float32_t3 finalColor = refracted * lerp(float32_t3(1.0f, 1.0f, 1.0f), jellyColor, 0.5f);
    finalColor += jellyColor * 0.6f;

    // リムライト
    float32_t3 rimColor = float32_t3(0.8f, 1.0f, 0.9f);
    finalColor += rimColor * fresnel * 0.8f;

    // ハイライト
    finalColor += float32_t3(1.0f, 1.0f, 1.0f) * spec * 2.0f;

    // アルファ
    float32_t alpha = saturate(0.4f + fresnel * 0.4f + spec);

    // 黒いアウトラインの追加
    float32_t outlineWidth = 0.12f; // アウトラインの太さ（遷移幅を広げて滑らかに）
    float32_t outlineFactor = smoothstep(threshold + outlineWidth, threshold + 0.01f, density);
    finalColor = lerp(finalColor, float32_t3(0.0f, 0.1f, 0.05f), outlineFactor * 0.8f); // 濃い緑黒で縁取る
    alpha = lerp(alpha, 0.95f, outlineFactor); // 縁は不透明に

    float32_t3 background = gSceneColor.SampleLevel(gSampler, input.texcoord, 0).rgb;
    float32_t3 result = lerp(background, finalColor, alpha * edgeAA);

    return float32_t4(result, 1.0f);
}
