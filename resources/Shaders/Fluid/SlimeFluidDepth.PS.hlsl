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
    float32_t radialDistance = sqrt(radiusSquared);
    float32_t alpha = saturate(1.0f - radialDistance);
    alpha = alpha * alpha * (3.0f - 2.0f * alpha);
    
    PSOutput output;
    output.depth = saturate(input.centerDepth - sphereNormalZ * depthThickness);
    
    // neo_Engine GPUFluidPS と同じ、1粒あたり0.2の加算アルファ密度。
    output.thickness = alpha * 0.2f;
    
    return output;
}
