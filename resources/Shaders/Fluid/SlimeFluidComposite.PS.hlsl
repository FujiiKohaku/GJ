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
    float32_t groundClipEnabled;
    float32_t padding0;
    float32_t4x4 gInvViewProj;
    float32_t4x4 gViewProj;
    float32_t3 eyeWorldPosition;
    float32_t eyeHalfWidthPixels;
    float32_t eyeHalfHeightPixels;
    float32_t eyeVisibility;
    float32_t2 eyeGazeDirection;
    float32_t deathEyes;
    float32_t3 paddingEyes;
    float32_t2 eyeCenterUv;
    float32_t2 paddingEyeCenter;
};

float32_t4 main(VertexShaderOutput input) : SV_TARGET
{
    float32_t dx_analytic = 0.0f;
    float32_t dy_analytic = 0.0f;
    float32_t wSum = 0.0f;
    float32_t spread = 3.5f;
    float32_t density = 0.0f;
    float32_t sigma = 4.0f;

    // neo_Engine MetaballPS と同じ5x5ガウシアン輪郭抽出。
    for(int y = -2; y <= 2; ++y) {
        for(int x = -2; x <= 2; ++x) {
            float32_t w = exp(-float32_t(x * x + y * y) / sigma);
            float32_t2 sampleUv =
                input.texcoord + float32_t2(x, y) * texelSize * spread;
            float32_t sampledThickness =
                gFluidThickness.SampleLevel(gSampler, sampleUv, 0);
            
            density += sampledThickness * w;
            wSum += w;
            
            dx_analytic += (float32_t(x) / 2.0f) * w * sampledThickness;
            dy_analytic += (float32_t(y) / 2.0f) * w * sampledThickness;
        }
    }
    density /= wSum;
    dx_analytic /= wSum;
    dy_analytic /= wSum;

    float32_t threshold = 0.08f;
    float32_t edgeAA = smoothstep(threshold, threshold + 0.03f, density);

    float32_t fluidDepth = gFluidDepth.SampleLevel(gSampler, input.texcoord, 0);
    float32_t4 clipPosition = float32_t4(
        input.texcoord.x * 2.0f - 1.0f,
        1.0f - input.texcoord.y * 2.0f,
        fluidDepth,
        1.0f);
    float32_t4 worldPosition = mul(clipPosition, gInvViewProj);
    worldPosition.xyz /= max(abs(worldPosition.w), 0.0001f);

    // 接地中だけ、流体表面のワールド座標を復元して床より下を切る。
    // 粒子円そのものを潰さず、着地時の外形だけを平らな接地面にする。
    if (groundClipEnabled > 0.5f)
    {
        float32_t groundMask = smoothstep(
            floorHeightWorld - 0.004f,
            floorHeightWorld + 0.008f,
            worldPosition.y);
        edgeAA *= groundMask;
    }

    float32_t depthRight = gFluidDepth.SampleLevel(gSampler, input.texcoord + float32_t2(texelSize.x * 2.0f, 0.0f), 0);
    float32_t depthLeft = gFluidDepth.SampleLevel(gSampler, input.texcoord - float32_t2(texelSize.x * 2.0f, 0.0f), 0);
    float32_t depthBottom = gFluidDepth.SampleLevel(gSampler, input.texcoord + float32_t2(0.0f, texelSize.y * 2.0f), 0);
    float32_t depthTop = gFluidDepth.SampleLevel(gSampler, input.texcoord - float32_t2(0.0f, texelSize.y * 2.0f), 0);

    float32_t dx = -dx_analytic * 8.0f;
    float32_t dy = -dy_analytic * 8.0f;

    float32_t normalStrength = 1.2f;
    float32_t2 normalXY = float32_t2(dx, dy) * normalStrength;
    float32_t xySq = saturate(dot(normalXY, normalXY));
    float32_t dz = -sqrt(1.0f - xySq);
    float32_t3 alphaNormal = float32_t3(normalXY.x, normalXY.y, dz);

    // Global dome correction (wider sampling for smoother dome)
    float32_t2 offX_far = float32_t2(texelSize.x * 4.0f, 0.0f);
    float32_t2 offY_far = float32_t2(0.0f, texelSize.y * 4.0f);
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

    // 縦長カプセル状の青い目。十分な流体密度がある領域だけに描き、
    // 半透明のスライム色を少し残すことで体内に沈んで見せる。
    float32_t2 eyePointBase = (input.texcoord - eyeCenterUv) / texelSize;
    static const float32_t kEyeSeparationPixels = 18.0f;
    float32_t2 leftEyePoint = eyePointBase + float32_t2(kEyeSeparationPixels, 0.0f);
    float32_t2 rightEyePoint = eyePointBase - float32_t2(kEyeSeparationPixels, 0.0f);
    float32_t eyeStraightHalfHeight =
        max(eyeHalfHeightPixels - eyeHalfWidthPixels, 0.0f);
    leftEyePoint.y -= clamp(
        leftEyePoint.y, -eyeStraightHalfHeight, eyeStraightHalfHeight);
    rightEyePoint.y -= clamp(
        rightEyePoint.y, -eyeStraightHalfHeight, eyeStraightHalfHeight);
    float32_t leftEyeDistance = length(leftEyePoint) - eyeHalfWidthPixels;
    float32_t rightEyeDistance = length(rightEyePoint) - eyeHalfWidthPixels;
    float32_t leftEyeMask =
        1.0f - smoothstep(-1.0f, 1.0f, leftEyeDistance);
    float32_t rightEyeMask =
        1.0f - smoothstep(-1.0f, 1.0f, rightEyeDistance);
    // Last-life rupture: replace both capsule eyes with a clear "X" mark.
    float32_t leftCrossBand = min(abs(leftEyePoint.x - leftEyePoint.y),
        abs(leftEyePoint.x + leftEyePoint.y));
    float32_t rightCrossBand = min(abs(rightEyePoint.x - rightEyePoint.y),
        abs(rightEyePoint.x + rightEyePoint.y));
    float32_t leftCrossExtent = max(abs(leftEyePoint.x), abs(leftEyePoint.y));
    float32_t rightCrossExtent = max(abs(rightEyePoint.x), abs(rightEyePoint.y));
    float32_t leftCrossMask =
        (1.0f - smoothstep(2.2f, 3.7f, leftCrossBand)) *
        (1.0f - smoothstep(10.0f, 12.0f, leftCrossExtent));
    float32_t rightCrossMask =
        (1.0f - smoothstep(2.2f, 3.7f, rightCrossBand)) *
        (1.0f - smoothstep(10.0f, 12.0f, rightCrossExtent));
    leftEyeMask = lerp(leftEyeMask, leftCrossMask, deathEyes);
    rightEyeMask = lerp(rightEyeMask, rightCrossMask, deathEyes);
    float32_t eyeMask = max(leftEyeMask, rightEyeMask) * eyeVisibility;
    // The eye center can overlap the floor's depth silhouette while grounded.
    // Do not gate it by the local thickness sample: that sample is a soft
    // metaball edge and becomes zero exactly where the grounded blob is most
    // compressed, which made the eyes appear only after jumping.
    float32_t insideMask = smoothstep(threshold + 0.10f, threshold + 0.24f, density);
    eyeMask *= eyeVisibility;
    float32_t closestEyeDistance = min(leftEyeDistance, rightEyeDistance);
    float32_t eyeInnerMask =
        1.0f - smoothstep(-3.2f, -1.2f, closestEyeDistance);
    eyeInnerMask *= eyeVisibility;
    float32_t eyeRimMask = saturate(eyeMask - eyeInnerMask);

    // 本体色を残した半透明ガラス色。完全な青で塗り潰さない。
    float32_t3 transparentBlue = float32_t3(0.055f, 0.48f, 0.92f);
    float32_t3 submergedBlue = lerp(jellyColor, transparentBlue, 0.62f);
    finalColor = lerp(finalColor, submergedBlue, eyeMask * 0.54f);
    finalColor = lerp(
        finalColor,
        float32_t3(0.18f, 0.67f, 1.0f),
        eyeRimMask * 0.30f);

    float32_t2 eyeHighlightCenter =
        float32_t2(-eyeHalfWidthPixels * 0.28f, -eyeHalfHeightPixels * 0.36f);
    eyeHighlightCenter += float32_t2(
        eyeGazeDirection.x,
        -eyeGazeDirection.y) * eyeHalfWidthPixels * 0.48f;
    float32_t leftEyeHighlight =
        1.0f - smoothstep(eyeHalfWidthPixels * 0.10f, eyeHalfWidthPixels * 0.34f,
            length(eyePointBase - float32_t2(-kEyeSeparationPixels, 0.0f) - eyeHighlightCenter));
    float32_t rightEyeHighlight =
        1.0f - smoothstep(eyeHalfWidthPixels * 0.10f, eyeHalfWidthPixels * 0.34f,
            length(eyePointBase - float32_t2(kEyeSeparationPixels, 0.0f) - eyeHighlightCenter));
    float32_t eyeHighlight = max(leftEyeHighlight, rightEyeHighlight);
    finalColor +=
        float32_t3(0.38f, 0.76f, 1.0f) * eyeHighlight * eyeMask * 0.36f;

    float32_t3 background = gSceneColor.SampleLevel(gSampler, input.texcoord, 0).rgb;
    // Keep the eye visible even when its pixel lands in a low-thickness part
    // of a grounded metaball. The eye mask itself supplies the coverage there.
    float32_t eyeAwareEdge = max(edgeAA, eyeMask);
    float32_t3 result = lerp(background, finalColor, alpha * eyeAwareEdge);

    return float32_t4(result, 1.0f);
}
