struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

struct LaserBeamConstants {
    matrix viewProjection;
    float4 cameraPositionAndTime;
    float4 color;
    float4 coreColor;
    float4 beamParameters1; // x: speed, y: intensity, z: noiseScale, w: coreIntensity
    float4 beamParameters2; // x: coreThreshold, y: spinSpeed, z: twistScale, w: (pad)
    float4 startPosition;
    float4 endPositionAndRadius;
};

ConstantBuffer<LaserBeamConstants> gLaser : register(b0);

#define TAU 6.28318530718f

// --- Noise Functions ---
float hash21(float2 p) {
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float noise(float2 p) {
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(hash21(cell), hash21(cell + float2(1, 0)), f.x),
                lerp(hash21(cell + float2(0, 1)), hash21(cell + 1.0f), f.x), f.y);
}

float fBm(float3 p) {
    // A simple 3D to 2D mapping for fBm since our noise is 2D, 
    // or we can implement a basic 3D noise. For performance and similarity, 
    // we use a pseudo-3D by animating the 2D plane.
    float2 uv = p.xy + p.z;
    float value = 0.0f;
    float amplitude = 0.5f;
    [unroll]
    for (int octave = 0; octave < 4; ++octave) {
        value += noise(uv) * amplitude;
        uv = mul(uv, float2x2(1.62f, -1.17f, 1.17f, 1.62f)) + 7.3f;
        amplitude *= 0.5f;
    }
    return value;
}
// -----------------------

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float time = gLaser.cameraPositionAndTime.w * gLaser.beamParameters1.x * 2.0f;
    
    float3 V = normalize(gLaser.cameraPositionAndTime.xyz - input.worldPosition);
    float3 N = normalize(input.normal);
    float fresnel = saturate(dot(N, V)); // 真正面が1.0、フチが0.0
    
    // 根本と先端のフェード（長さに依存せず一定の距離でフェードさせる）
    float beamLength = max(length(gLaser.endPositionAndRadius.xyz - gLaser.startPosition.xyz), 0.001f);
    float worldY = uv.y * beamLength;
    float fadeDistance = 0.2f; // 0.2マス分だけフェード
    float verticalFade = smoothstep(0.0f, fadeDistance, worldY) * (1.0f - smoothstep(max(0.0f, beamLength - fadeDistance), beamLength, worldY));
    
    // スピンとねじれ
    float angle = uv.x * TAU + (gLaser.cameraPositionAndTime.w * gLaser.beamParameters2.y) + (uv.y * gLaser.beamParameters2.z);
    
    float3 p1 = float3(cos(angle), sin(angle), uv.y * 3.0f - time * 3.0f);
    float3 p2 = float3(cos(angle)*1.5f, sin(angle)*1.5f, uv.y * 5.0f - time * 5.0f);
    
    float noise1 = fBm(p1 * gLaser.beamParameters1.z);
    float noise2 = fBm(p2 * gLaser.beamParameters1.z * 1.5f);
    float combinedNoise = (noise1 * 0.6f + noise2 * 0.4f);
    
    // コアとオーラ
    float coreFactor = pow(fresnel, max(0.1f, 10.0f - gLaser.beamParameters2.x * 5.0f));
    float auraFactor = combinedNoise * (1.0f - pow(fresnel, 3.0f));
    
    float3 finalColor = float3(0, 0, 0);
    
    finalColor += gLaser.color.rgb * gLaser.beamParameters1.y * auraFactor;
    
    float corePulse = 0.8f + 0.2f * noise1;
    finalColor += gLaser.coreColor.rgb * gLaser.beamParameters1.w * coreFactor * corePulse;
    
    float alpha = saturate((auraFactor + coreFactor) * verticalFade);
    
    if (alpha <= 0.01f) {
        discard;
    }
    
    return float4(finalColor, alpha * gLaser.color.a);
}
