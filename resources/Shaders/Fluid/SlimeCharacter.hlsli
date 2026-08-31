struct SlimeCharacterConstants
{
    float4x4 viewProjection;
    float4 cameraPositionAndTime;
    float4 color;
    float4 positionAndGround;
    float4 radiiAndWobble;
    float4 forwardAndSpeed;
    float4 cameraRightAndRadius;
    float4 cameraUpAndHeight;
};

ConstantBuffer<SlimeCharacterConstants> gSlime : register(b0);

struct SlimeCharacterVertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL0;
    float3 localPosition : TEXCOORD0;
    float2 uv : TEXCOORD1;
};
