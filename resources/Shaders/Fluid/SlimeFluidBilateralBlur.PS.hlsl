struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

Texture2D<float32_t> gDepthTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer SlimeFluidBlurParameter : register(b0)
{
    float32_t2 texelSize;
    int32_t direction;
    int32_t radius;
    float32_t sigma;
    float32_t depthSigma;
    float32_t padding0;
    float32_t padding1;
};

float32_t main(VertexShaderOutput input) : SV_TARGET
{
    float32_t centerDepth = gDepthTexture.Sample(gSampler, input.texcoord);
    bool centerHasSurface = centerDepth < 0.999f;

    float32_t2 axis = direction == 0 ? float32_t2(texelSize.x, 0.0f) : float32_t2(0.0f, texelSize.y);
    float32_t totalWeight = 0.0f;
    float32_t depthSum = 0.0f;

    [loop]
    for (int32_t sampleIndex = -radius; sampleIndex <= radius; ++sampleIndex)
    {
        float32_t2 sampleUv = input.texcoord + axis * (float32_t) sampleIndex;
        float32_t sampleDepth = gDepthTexture.Sample(gSampler, sampleUv);
        if (sampleDepth >= 0.999f)
        {
            continue;
        }

        float32_t spatialWeight =
            exp(-((float32_t) sampleIndex * (float32_t) sampleIndex) / (2.0f * sigma * sigma));
        float32_t rangeWeight = 1.0f;
        if (centerHasSurface)
        {
            float32_t depthDifference = sampleDepth - centerDepth;
            rangeWeight =
                exp(-(depthDifference * depthDifference) / (2.0f * depthSigma * depthSigma));
        }
        float32_t weight = spatialWeight * rangeWeight;

        depthSum += sampleDepth * weight;
        totalWeight += weight;
    }

    if (totalWeight <= 0.0001f)
    {
        return centerDepth;
    }

    float32_t smoothedDepth = depthSum / totalWeight;
    if (centerHasSurface)
    {
        return smoothedDepth;
    }

    float32_t silhouette = saturate(totalWeight / max(sigma * 1.15f, 1.0f));
    if (silhouette <= 0.025f)
    {
        return 1.0f;
    }

    return lerp(1.0f, smoothedDepth, silhouette);
}
