struct VertexShaderInput {
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

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

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;

    // Cylinder mesh is created along the Y-axis (from y=0 to y=1).
    // We scale and rotate it to go from startPosition to endPosition.
    
    float3 startPos = gLaser.startPosition.xyz;
    float3 endPos = gLaser.endPositionAndRadius.xyz;
    float radius = max(gLaser.endPositionAndRadius.w, 0.001f);
    
    float3 direction = endPos - startPos;
    float beamLength = length(direction);
    float3 dirNorm = direction / max(beamLength, 0.001f);
    
    // Create an orthonormal basis
    float3 up = abs(dirNorm.y) > 0.999f ? float3(1, 0, 0) : float3(0, 1, 0);
    float3 right = normalize(cross(up, dirNorm));
    up = cross(dirNorm, right);
    
    // input.position.y is 0 to 1
    float3 localPos = right * input.position.x * radius
                    + dirNorm * input.position.y * beamLength
                    + up * input.position.z * radius;
                    
    float3 worldPos = startPos + localPos;
    
    output.worldPosition = worldPos;
    output.position = mul(float4(worldPos, 1.0f), gLaser.viewProjection);
    
    float3 worldNormal = right * input.normal.x
                       + dirNorm * input.normal.y
                       + up * input.normal.z;
    output.normal = normalize(worldNormal);
    
    output.texcoord = input.texcoord;
    
    return output;
}
