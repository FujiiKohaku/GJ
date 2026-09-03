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
    
    output.thickness = sphereNormalZ * 0.72f;
    
    return output;
}
