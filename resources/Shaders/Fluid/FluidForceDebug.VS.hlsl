#include "SlimeFluidCommon.hlsli"

cbuffer ViewProjectionBuffer : register(b0)
{
    float32_t4x4 gViewProj;
    float32_t3 gCamPos;
    float32_t pad;
};
StructuredBuffer<SlimeFluidParticle> gParticleBuffer : register(t0);
StructuredBuffer<float32_t4> gForceBuffer : register(t1);

struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VertexOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VertexOutput output;
    
    SlimeFluidParticle p = gParticleBuffer[instanceId];
    
    float3 pos = p.position;
    float speed = length(p.velocity);
    
    float3 dir = float3(0.0f, 1.0f, 0.0f);
    if (speed > 0.01f) dir = p.velocity / speed;
    
    float scale = 0.02f;
    float lineLen = speed * scale;
    lineLen = min(lineLen, 2.0f); // クランプ
    
    float3 tip = pos + dir * lineLen;
    
    float3 camDir = normalize(pos - gCamPos);
    float3 right = normalize(cross(dir, camDir));
    if (length(right) < 0.01f) right = float3(1.0f, 0.0f, 0.0f);
    
    float headLen = 0.08f;
    float headWidth = 0.04f;
    
    float3 currentPos = pos;
    float4 c = float4(0.0f, 1.0f, 1.0f, 1.0f);
    
    // 先端に行くほど色が赤くなる
    float r = min(speed / 20.0f, 1.0f);
    float g = min(speed / 10.0f, 1.0f) * (1.0f - r);
    float b = 1.0f - r - g;
    float4 tipColor = float4(r, max(g, 0.0f), max(b, 0.0f), 1.0f);
    
    if (vertexId == 0) { currentPos = pos; c = float4(0.0f, 1.0f, 1.0f, 1.0f); }
    else if (vertexId == 1) { currentPos = tip; c = tipColor; }
    else if (vertexId == 2) { currentPos = tip; c = tipColor; }
    else if (vertexId == 3) { currentPos = tip - dir * headLen + right * headWidth; c = tipColor; }
    else if (vertexId == 4) { currentPos = tip; c = tipColor; }
    else if (vertexId == 5) { currentPos = tip - dir * headLen - right * headWidth; c = tipColor; }
    
    output.position = mul(float4(currentPos, 1.0f), gViewProj);
    output.color = c;
    
    return output;
}
