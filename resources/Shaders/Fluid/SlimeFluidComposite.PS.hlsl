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
    float32_t idleFaceAmount;
    float32_t idleFaceTime;
    float32_t2 paddingIdleFace;
};

float32_t Hash31(float32_t3 value)
{
    value = frac(value * 0.1031f);
    value += dot(value, value.yzx + 33.33f);
    return frac((value.x + value.y) * value.z);
}

float32_t ValueNoise(float32_t3 position)
{
    float32_t3 cell = floor(position);
    float32_t3 local = frac(position);
    local = local * local * (3.0f - 2.0f * local);
    float32_t n000 = Hash31(cell + float32_t3(0.0f, 0.0f, 0.0f));
    float32_t n100 = Hash31(cell + float32_t3(1.0f, 0.0f, 0.0f));
    float32_t n010 = Hash31(cell + float32_t3(0.0f, 1.0f, 0.0f));
    float32_t n110 = Hash31(cell + float32_t3(1.0f, 1.0f, 0.0f));
    float32_t n001 = Hash31(cell + float32_t3(0.0f, 0.0f, 1.0f));
    float32_t n101 = Hash31(cell + float32_t3(1.0f, 0.0f, 1.0f));
    float32_t n011 = Hash31(cell + float32_t3(0.0f, 1.0f, 1.0f));
    float32_t n111 = Hash31(cell + float32_t3(1.0f, 1.0f, 1.0f));
    return lerp(
        lerp(lerp(n000, n100, local.x), lerp(n010, n110, local.x), local.y),
        lerp(lerp(n001, n101, local.x), lerp(n011, n111, local.x), local.y),
        local.z);
}

float32_t NebulaNoise(float32_t3 position)
{
    float32_t value = ValueNoise(position);
    value += ValueNoise(position * 2.03f + 17.0f) * 0.50f;
    value += ValueNoise(position * 4.07f - 11.0f) * 0.25f;
    return value / 1.75f;
}

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
    // 低周波の星雲だけで色を作る。高周波の星ノイズは移動時に細かく
    // 明滅して見えたため使わず、広い色面がゆっくり移る表現にする。
    float32_t3 cosmicPosition = worldPosition.xyz * 0.78f + normal * 0.22f;
    float32_t nebulaWide = NebulaNoise(cosmicPosition * 0.72f);
    float32_t nebulaAccent = NebulaNoise(cosmicPosition * 1.12f + 5.0f);
    float32_t nebulaBlend = lerp(nebulaWide, nebulaAccent, 0.30f);
    float32_t violetBand = smoothstep(0.28f, 0.78f, nebulaBlend);
    float32_t cyanBand = smoothstep(0.34f, 0.82f, 1.0f - nebulaWide * 0.55f + nebulaAccent * 0.45f);
    float32_t3 deepSpace = lerp(
        float32_t3(0.008f, 0.010f, 0.070f),
        float32_t3(0.018f, 0.105f, 0.205f),
        thickness);
    float32_t3 violetNebula = float32_t3(0.24f, 0.055f, 0.52f) * violetBand;
    float32_t3 cyanNebula = float32_t3(0.025f, 0.44f, 0.62f) * cyanBand;
    float32_t3 jellyColor = deepSpace + violetNebula * 0.46f + cyanNebula * 0.42f;

    float32_t3 halfVec = normalize(lightDir + viewDir);
    float32_t spec =
        pow(max(dot(normal, halfVec), 0.0f), 300.0f) *
        specularStrength * topHighlightMask;

    float32_t3 finalColor =
        lerp(jellyColor, refracted * jellyColor, translucency * 0.28f);
    finalColor += jellyColor * 0.34f;

    float32_t3 rimColor = lerp(
        float32_t3(0.22f, 0.08f, 0.75f),
        float32_t3(0.20f, 0.95f, 1.0f),
        saturate(normal.y * 0.5f + 0.5f));
    finalColor += rimColor * (fresnel * fresnelStrength * 1.35f);

    finalColor += float32_t3(1.0f, 1.0f, 1.0f) * spec;

    float32_t alpha = saturate(0.86f + thickness * 0.18f + spec * 0.15f);

    float32_t outlineOuter =
        1.0f - smoothstep(threshold + 0.02f, threshold + 0.12f, density);
    float32_t outlineInner =
        smoothstep(threshold - 0.006f, threshold + 0.018f, density);
    float32_t outlineFactor = saturate(outlineOuter * outlineInner);
    finalColor =
        lerp(finalColor, float32_t3(0.005f, 0.010f, 0.075f), outlineFactor * 0.78f);
    alpha = lerp(alpha, 0.98f, outlineFactor);

    // 縦長カプセル状の青い目。十分な流体密度がある領域だけに描き、
    // 半透明のスライム色を少し残すことで体内に沈んで見せる。
    float32_t2 eyePointBase = (input.texcoord - eyeCenterUv) / texelSize;
    // When idle, the pair of eyes glances around the body with a slow,
    // uneven rhythm. Movement resets idleFaceAmount, bringing the gaze back
    // to center immediately.
    // Look around for a while, then settle back to center before repeating.
    // The vertical offset is intentionally upward so the idle pose also scans
    // the space above the player.
    float32_t gazeCycle = frac(idleFaceTime * 0.16f);
    // Take over a second to leave center and to return. This is deliberately
    // slower than the glance motion, avoiding any visible eye teleport.
    float32_t gazeActive = smoothstep(0.08f, 0.28f, gazeCycle) *
        (1.0f - smoothstep(0.56f, 0.82f, gazeCycle));
    float32_t glanceX = sin(idleFaceTime * 0.58f) * 38.0f;
    float32_t glanceY = -12.0f - abs(sin(idleFaceTime * 0.43f + 0.65f)) * 12.0f;
    float32_t2 idleGazeOffset =
        float32_t2(glanceX, glanceY) * idleFaceAmount * gazeActive;
    eyePointBase -= idleGazeOffset;
    // Preserve the gaze transition strength for the highlight. Normalizing
    // this vector made the glint jump to its full offset on the first frame.
    float32_t2 idleHighlightGaze = float32_t2(
        idleGazeOffset.x / 38.0f,
        idleGazeOffset.y / 24.0f);
    static const float32_t kEyeSeparationPixels = 18.0f;
    float32_t2 leftEyePoint = eyePointBase + float32_t2(kEyeSeparationPixels, 0.0f);
    float32_t2 rightEyePoint = eyePointBase - float32_t2(kEyeSeparationPixels, 0.0f);
    // A real blink closes the rendered eye vertically to a thin horizontal
    // line for a short moment, instead of merely shrinking the whole eye.
    float32_t blinkPhase = frac(idleFaceTime * 0.58f);
    float32_t blinkPulse = saturate(
        smoothstep(0.70f, 0.78f, blinkPhase) -
        smoothstep(0.82f, 0.92f, blinkPhase));
    blinkPulse *= idleFaceAmount;
    float32_t blinkVerticalScale = lerp(1.0f, 0.12f, blinkPulse);
    float32_t idleSquint = idleFaceAmount * 0.16f;
    float32_t currentEyeHalfHeight = lerp(
        eyeHalfHeightPixels, eyeHalfWidthPixels * 0.68f, idleSquint);
    float32_t eyeStraightHalfHeight =
        max(currentEyeHalfHeight - eyeHalfWidthPixels, 0.0f);
    leftEyePoint.y -= clamp(
        leftEyePoint.y, -eyeStraightHalfHeight, eyeStraightHalfHeight);
    rightEyePoint.y -= clamp(
        rightEyePoint.y, -eyeStraightHalfHeight, eyeStraightHalfHeight);
    leftEyePoint.y /= blinkVerticalScale;
    rightEyePoint.y /= blinkVerticalScale;
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

    // Dark eyes deliberately contrast the luminous cosmic body.
    float32_t3 eyeCoreColor = float32_t3(0.002f, 0.004f, 0.025f);
    finalColor = lerp(finalColor, eyeCoreColor, eyeMask * 0.98f);
    finalColor = lerp(
        finalColor,
        float32_t3(0.20f, 0.78f, 1.0f),
        eyeRimMask * 0.76f);

    float32_t2 eyeHighlightCenter =
        float32_t2(-eyeHalfWidthPixels * 0.28f, -currentEyeHalfHeight * 0.36f);
    // The white glint follows both movement gaze and the idle look-around
    // direction, making the player visibly look toward each glance target.
    float32_t2 highlightGaze = float32_t2(
        eyeGazeDirection.x,
        -eyeGazeDirection.y) + idleHighlightGaze;
    eyeHighlightCenter += highlightGaze * eyeHalfWidthPixels * 0.58f;
    // Keep the glint fully inside the dark pupil even at the far-left glance.
    // The highlight radius is up to 34% of the pupil radius, so its center
    // must remain well inside the horizontal edge.
    eyeHighlightCenter.x = clamp(
        eyeHighlightCenter.x,
        -eyeHalfWidthPixels * 0.40f,
        eyeHalfWidthPixels * 0.40f);
    float32_t leftEyeHighlight =
        1.0f - smoothstep(eyeHalfWidthPixels * 0.10f, eyeHalfWidthPixels * 0.34f,
            length(eyePointBase - float32_t2(-kEyeSeparationPixels, 0.0f) - eyeHighlightCenter));
    float32_t rightEyeHighlight =
        1.0f - smoothstep(eyeHalfWidthPixels * 0.10f, eyeHalfWidthPixels * 0.34f,
            length(eyePointBase - float32_t2(kEyeSeparationPixels, 0.0f) - eyeHighlightCenter));
    float32_t eyeHighlight = max(leftEyeHighlight, rightEyeHighlight);
    finalColor +=
        float32_t3(0.72f, 0.93f, 1.0f) * eyeHighlight * eyeMask * 0.72f;

    float32_t3 background = gSceneColor.SampleLevel(gSampler, input.texcoord, 0).rgb;
    // Keep the eye visible even when its pixel lands in a low-thickness part
    // of a grounded metaball. The eye mask itself supplies the coverage there.
    float32_t eyeAwareEdge = max(edgeAA, eyeMask);
    float32_t3 result = lerp(background, finalColor, alpha * eyeAwareEdge);

    return float32_t4(result, 1.0f);
}
