#pragma pack_matrix(row_major)

struct SlimeFluidParticle
{
    float32_t3 position;
    float32_t density;
    float32_t3 velocity;
    float32_t pressure;
    float32_t3 restPosition;
    float32_t padding;
};

StructuredBuffer<SlimeFluidParticle> Particles : register(t0);

cbuffer ViewProjection : register(b0)
{
    float32_t4x4 view;
    float32_t4x4 projection;
    float32_t4x4 viewProj;
    float32_t4x4 invProjection;
    float32_t4x4 invView;
    float32_t3 cameraPos;
    float32_t time;
    float32_t3 corePosition;
    float32_t isLiquidated;
    float32_t3 blobColor;
    float32_t padColor;
};

struct VSOutput
{
    float32_t4 pos : SV_POSITION;
    float32_t2 uv : TEXCOORD;
    float32_t3 viewPos : POSITION0;
    float32_t3 worldPos : POSITION1;
    float32_t3 centerWorldPos : POSITION2;
    float32_t radius : BLENDWEIGHT0;
    float32_t3 color : COLOR;
};

static const float32_t2 quadOffsets[4] =
{
    float32_t2(-1.0f, 1.0f), float32_t2(1.0f, 1.0f),
    float32_t2(-1.0f, -1.0f), float32_t2(1.0f, -1.0f)
};

VSOutput main(uint32_t vertexID : SV_VertexID, uint32_t instanceID : SV_InstanceID)
{
    VSOutput output;
    SlimeFluidParticle p = Particles[instanceID];

    if (p.padding <= 0.0f)
    {
        output.pos = float32_t4(2.0f, 2.0f, 0.0f, 1.0f);
        output.uv = float32_t2(2.0f, 2.0f);
        output.viewPos = float32_t3(0.0f, 0.0f, 0.0f);
        output.worldPos = float32_t3(0.0f, 0.0f, 0.0f);
        output.centerWorldPos = float32_t3(0.0f, 0.0f, 0.0f);
        output.radius = 0.0f;
        output.color = float32_t3(0.0f, 0.0f, 0.0f);
        return output;
    }

    float32_t3 centerWorld = p.position;
    float32_t radius = 0.12f;

    float32_t2 offset = quadOffsets[vertexID];
    float32_t3 right = float32_t3(view._11, view._12, view._13);
    float32_t3 up = float32_t3(view._21, view._22, view._23);
    float32_t3 worldPos = centerWorld + right * (offset.x * radius) + up * (offset.y * radius);

    float32_t4 worldPos4 = float32_t4(worldPos, 1.0f);
    float32_t4 vPos = mul(worldPos4, view);
    output.pos = mul(worldPos4, viewProj);
    output.uv = offset;
    output.viewPos = vPos.xyz;
    output.worldPos = worldPos;
    output.centerWorldPos = centerWorld;
    output.radius = radius;

    float32_t speedT = saturate(length(p.velocity) / 5.0f);
    output.color = lerp(
        float32_t3(0.05f, 0.45f, 1.0f),
        float32_t3(0.12f, 1.0f, 0.62f),
        saturate(0.35f + speedT * 0.65f));

    return output;
}
