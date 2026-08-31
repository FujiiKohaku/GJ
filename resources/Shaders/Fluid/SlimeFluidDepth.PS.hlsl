#include "SlimeFluidRenderCommon.hlsli"

struct PSOutput {
    float32_t depth : SV_TARGET0;
    float32_t thickness : SV_TARGET1;
};

PSOutput main(SlimeDepthVertexOutput input)
{
    float32_t radiusSquared = dot(input.localPosition, input.localPosition);
    if (radiusSquared > 1.0f)
    {
        discard;
    }

    float32_t sphereNormalZ = sqrt(saturate(1.0f - radiusSquared));
    
    PSOutput output;
    output.depth = saturate(input.centerDepth - sphereNormalZ * depthThickness);
    
    float32_t len = sqrt(radiusSquared);
    float32_t alpha = saturate(1.0f - len);
    alpha = alpha * alpha * (3.0f - 2.0f * alpha);
    output.thickness = alpha * 0.2f;
    
    return output;
}
