#ifndef SLIME_FLUID_RENDER_COMMON_HLSLI
#define SLIME_FLUID_RENDER_COMMON_HLSLI

struct SlimeFluidParticle
{
    float32_t3 position;
    float32_t density;
    float32_t3 velocity;
    float32_t pressure;
    float32_t3 restPosition;
    float32_t padding;
};

cbuffer SlimeFluidPerView : register(b0)
{
    float32_t4x4 viewProjection;
    float32_t3 cameraRight;
    float32_t particleRadius;
    float32_t3 cameraUp;
    float32_t depthThickness;
    float32_t2 inverseScreenSize;
    uint32_t particleCount;
    float32_t paddingPerView;
};

struct SlimeDepthVertexOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 localPosition : TEXCOORD0;
    float32_t centerDepth : TEXCOORD1;
};

#endif
