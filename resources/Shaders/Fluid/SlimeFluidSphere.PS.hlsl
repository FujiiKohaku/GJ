#pragma pack_matrix(row_major)

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

struct PSOutput
{
    float32_t4 colorOut : SV_TARGET0;
    float32_t depth : SV_Depth;
};

PSOutput main(VSOutput input)
{
    PSOutput output;
    
    // UVからの距離（中心0、エッジ1）
    float32_t distFromCenter = length(input.uv);
    if (distFromCenter > 1.0f) discard;
    
    float32_t z = sqrt(1.0f - dot(input.uv, input.uv));
    
    // ピクセルのビュー空間座標を計算
    float32_t3 pixelViewPos = input.viewPos;
    pixelViewPos.z -= z * input.radius; 
    
    // クリップ空間へ変換して Z バッファ用の深度を計算
    float32_t4 clipPos = mul(float32_t4(pixelViewPos, 1.0f), projection);
    output.depth = clipPos.z / clipPos.w;
    
    // 表面の揺らぎ（近似）
    float32_t waveX = sin(input.worldPos.x * 5.0f + time * 3.0f) * cos(input.worldPos.y * 5.0f - time * 2.0f) * 0.05f;
    float32_t waveY = cos(input.worldPos.x * 4.0f - time * 3.0f) * sin(input.worldPos.y * 6.0f + time * 2.0f) * 0.05f;
    
    float32_t3 normal = normalize(float32_t3(input.uv.x, input.uv.y, -z) + float32_t3(waveX, waveY, 0.0f));
    float32_t3 lightDir = normalize(float32_t3(0.5f, 1.0f, -0.5f));
    
    float32_t3 viewDir = float32_t3(0.0f, 0.0f, -1.0f);
    float32_t NdotV = max(dot(normal, viewDir), 0.0f);
    
    // フレネル (Schlickの近似)
    float32_t3 F0 = float32_t3(0.04f, 0.04f, 0.04f);
    float32_t3 fresnel = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);
    
    // スペキュラ（ハイライト）
    float32_t3 halfVec = normalize(lightDir + viewDir);
    float32_t NdotH = max(dot(normal, halfVec), 0.0f);
    float32_t spec = pow(NdotH, 256.0f);
    float32_t3 specColor = float32_t3(1.0f, 1.0f, 1.0f) * spec * 5.0f;
    
    // 環境反射 (近似)
    float32_t3 reflectDir = reflect(-viewDir, normal);
    float32_t skyFactor = smoothstep(-0.5f, 1.0f, reflectDir.y);
    float32_t3 envColor = lerp(float32_t3(0.0f, 0.1f, 0.15f), float32_t3(0.7f, 0.85f, 1.0f), skyFactor);
    float32_t3 surfaceReflection = envColor * fresnel * 2.0f + specColor;
    
    // エッジのソフトフェードとAbsorption近似
    float32_t edgeFade = smoothstep(1.0f, 0.3f, distFromCenter);
    float32_t apparentThickness = NdotV;
    float32_t3 shallowColor = float32_t3(0.3f, 0.95f, 0.7f);
    float32_t3 deepColor = float32_t3(0.0f, 0.35f, 0.45f);
    float32_t3 waterBaseColor = lerp(shallowColor, deepColor, apparentThickness) * blobColor;
    
    float32_t NdotL = max(dot(normal, lightDir), 0.0f);
    float32_t3 scatterColor = waterBaseColor * (NdotL * 0.4f + 0.6f);
    
    // 最終的な合成
    float32_t3 finalColor = scatterColor * 0.8f + surfaceReflection;
    float32_t alpha = edgeFade * 0.5f + fresnel.x * 1.5f + spec * 2.0f;
    
    output.colorOut = float32_t4(finalColor, saturate(alpha));
    
    return output;
}
