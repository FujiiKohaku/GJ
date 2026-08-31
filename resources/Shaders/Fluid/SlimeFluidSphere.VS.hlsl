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

    // ローカル座標からワールド座標へ（パーティクル位置はすでにワールド空間）
    float32_t3 centerWorld = p.position;
    float32_t radius = 0.7f; // スライムらしい丸みを帯びた融合表現のために大きめにする

    float32_t2 offset = quadOffsets[vertexID];
    float32_t3 up = float32_t3(view._12, view._22, view._32);
    float32_t3 right = float32_t3(view._11, view._21, view._31);
    float32_t3 worldPos = centerWorld + right * offset.x * radius + up * offset.y * radius;

    float32_t4 worldPos4 = float32_t4(worldPos, 1.0f);
    float32_t4 vPos = mul(worldPos4, view);
    output.pos = mul(worldPos4, viewProj);
    output.uv = offset;
    output.viewPos = vPos.xyz;
    output.worldPos = worldPos;
    output.centerWorldPos = centerWorld;
    output.radius = radius;

    // 速度と位置（中心距離）ベースのヒートマップ
    // 中心（radialT=0）が赤、外側（radialT=1）が青になるように反転する
    float32_t speedT = saturate(length(p.velocity) / 5.0f);
    float32_t radialT = saturate(length(p.position) / 1.2f);
    
    // tが0(外側)で青、tが1(中心または高速)で赤
    float32_t t = saturate((1.0f - radialT) * 0.7f + speedT * 0.3f);
    
    float32_t3 heatColor = lerp(float32_t3(0.1f, 0.4f, 1.0f), float32_t3(0.2f, 0.9f, 0.3f), saturate(t * 2.0f));
    heatColor = lerp(heatColor, float32_t3(1.0f, 0.2f, 0.1f), saturate((t - 0.5f) * 2.0f));
    output.color = heatColor;

    return output;
}
