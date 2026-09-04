#include "object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLightCollection> gPointLights : register(b3);
ConstantBuffer<SpotLightCollection> gSpotLights : register(b4);
ConstantBuffer<AmbientLight> gAmbientLight : register(b5);

Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);

SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

float3 ToonIllumination(float3 normal, float3 worldPosition)
{
    float3 N = normalize(normal);
    float3 L = -gDirectionalLight.direction;
    L /= max(length(L), 0.0001f);
    // Half Lambert gives vertical faces a readable middle band.
    float3 light = max(gAmbientLight.color.rgb * gAmbientLight.color.a, 0.0f);
    light += gDirectionalLight.color.rgb * max(gDirectionalLight.intensity, 0.0f) *
        saturate(dot(N, L) * 0.5f + 0.5f);
    for (uint index = 0; index < kMaxPointLights; ++index) {
        PointLight source = gPointLights.lights[index];
        if (source.isActive == 0 || source.radius <= 0.0f) {
            continue;
        }
        float3 toLight = source.position - worldPosition;
        float distanceToLight = length(toLight);
        float attenuation = pow(saturate(1.0f - distanceToLight / source.radius), max(source.decay, 0.001f));
        float diffuse = saturate(dot(N, toLight / max(distanceToLight, 0.0001f)));
        light += source.color.rgb * max(source.intensity, 0.0f) * attenuation * diffuse;
    }
    for (uint index = 0; index < kMaxSpotLights; ++index) {
        SpotLight source = gSpotLights.lights[index];
        if (source.isActive == 0 || source.distance <= 0.0f) {
            continue;
        }
        float3 toLight = source.position - worldPosition;
        float distanceToLight = length(toLight);
        float3 Ls = toLight / max(distanceToLight, 0.0001f);
        float3 direction = source.direction / max(length(source.direction), 0.0001f);
        float cone = saturate((dot(-Ls, direction) - source.cosAngle) / max(1.0f - source.cosAngle, 0.0001f));
        float attenuation = pow(saturate(1.0f - distanceToLight / source.distance), max(source.decay, 0.001f));
        light += source.color.rgb * max(source.intensity, 0.0f) * cone * attenuation * saturate(dot(N, Ls));
    }

    float brightness = dot(light, float3(0.2126f, 0.7152f, 0.0722f));
    // Terrain uses painted shadow colors and a stable face value so its shape
    // remains readable even under broad ambient lighting.
    if (gMaterial.enableLighting == 7 || gMaterial.enableLighting == 8) {
        brightness += saturate(N.y) * 0.20f;
        float3 terrainBand = float3(0.58f, 0.53f, 0.72f);
        if (brightness >= 0.95f) {
            terrainBand = float3(1.08f, 1.01f, 0.88f);
        } else if (brightness >= 0.45f) {
            terrainBand = float3(0.84f, 0.80f, 0.83f);
        }
        float faceValue = 0.88f;
        if (N.y > 0.5f) {
            faceValue = 1.08f;
        } else if (N.y < -0.5f) {
            faceValue = 0.72f;
        }
        float terrainPeak = max(max(light.r, light.g), light.b);
        float3 terrainTint = light / max(terrainPeak, 0.0001f);
        return terrainBand * faceValue * lerp(float3(1, 1, 1), terrainTint, 0.15f);
    }
    float3 band = float3(0.40f, 0.53f, 0.55f);
    if (brightness >= 0.95f) {
        band = float3(1.0f, 0.92f, 0.78f);
    } else if (brightness >= 0.45f) {
        band = float3(0.73f, 0.74f, 0.66f);
    }
    // Retain a restrained tint from local lights without specular highlights.
    float peak = max(max(light.r, light.g), light.b);
    float3 tint = light / max(peak, 0.0001f);
    return band * lerp(float3(1.0f, 1.0f, 1.0f), tint, 0.30f);
}

float TerrainNoise(float2 position)
{
    float2 cell = floor(position);
    float2 blend = frac(position);
    blend = blend * blend * (3.0f - 2.0f * blend);
    float4 seeds = float4(dot(cell, float2(127.1f, 311.7f)),
        dot(cell + float2(1, 0), float2(127.1f, 311.7f)),
        dot(cell + float2(0, 1), float2(127.1f, 311.7f)),
        dot(cell + 1.0f, float2(127.1f, 311.7f)));
    float4 values = frac(sin(seeds) * 43758.5453f);
    return lerp(lerp(values.x, values.y, blend.x), lerp(values.z, values.w, blend.x), blend.y);
}

float TerrainWorldNoise(float3 position, float3 normal)
{
    float3 weights = pow(abs(normal), 4.0f);
    weights /= max(weights.x + weights.y + weights.z, 0.0001f);
    return TerrainNoise(position.zy) * weights.x +
        TerrainNoise(position.xz) * weights.y + TerrainNoise(position.xy) * weights.z;
}

float3 MossTerrainColor(float3 worldPosition, float3 normal)
{
    // World-space samples continue across tile boundaries. Small stepped texels
    // give the surface a voxel-like grain without outlining individual blocks.
    float3 position = floor(worldPosition * 64.0f) / 64.0f;
    float broad = TerrainWorldNoise(position * 0.65f, normal);
    float grain = TerrainWorldNoise(position * 18.0f + 23.7f, normal);
    // One texture spans four map units. Wrap sampling and world-space UVs
    // keep adjacent blocks on the same continuous pattern.
    float3 weights = pow(abs(normal), 4.0f);
    weights /= max(weights.x + weights.y + weights.z, 0.0001f);
    float3 texturePosition = worldPosition * 0.25f;
    float3 dirt = gTexture.Sample(gSampler, float2(texturePosition.z, -texturePosition.y)).rgb * weights.x;
    dirt += gTexture.Sample(gSampler, texturePosition.xz).rgb * weights.y;
    dirt += gTexture.Sample(gSampler, float2(texturePosition.x, -texturePosition.y)).rgb * weights.z;
    // Keep the hand-painted dirt palette free of additional cloudy shading.

    float depth = max(gMaterial.environmentCoefficient - worldPosition.y, 0.0f);
    // Roughly 0.12-0.30 units of moss hang down from the actual ground surface.
    float mossDepth = 0.12f + TerrainNoise(position.xz * 4.0f + 8.3f) * 0.18f;
    mossDepth += (TerrainNoise(position.xz * 16.0f + 41.2f) - 0.5f) * 0.04f;
    // A narrow antialiased edge keeps the moss painted rather than airbrushed.
    float edgeWidth = max(fwidth(depth - mossDepth), 0.001f);
    float moss = 1.0f - smoothstep(mossDepth - edgeWidth, mossDepth + edgeWidth, depth);
    // No moss on the underside of floating platforms.
    if (normal.y < -0.5f) {
        moss = 0.0f;
    }
    float mossTone = broad * 0.65f + grain * 0.35f;
    float3 green = float3(0.26f, 0.38f, 0.13f);
    if (mossTone >= 0.62f) {
        green = float3(0.49f, 0.62f, 0.25f);
    } else if (mossTone >= 0.38f) {
        green = float3(0.37f, 0.50f, 0.18f);
    }
    return lerp(dirt, green, moss);
}

float3 StoneTileColor(float3 worldPosition, float3 normal)
{
    // Match MossSoil: 256 texels across four map units = 64 texels per unit.
    // Dominant-axis projection keeps every cube face at the same density.
    float2 facePosition = float2(worldPosition.x, -worldPosition.y);
    float3 faceNormal = abs(normal);
    if (faceNormal.y >= faceNormal.x && faceNormal.y >= faceNormal.z) {
        facePosition = worldPosition.xz;
    } else if (faceNormal.x > faceNormal.z) {
        facePosition = float2(worldPosition.z, -worldPosition.y);
    }
    float2 texel = floor((facePosition + 0.5f) * 64.0f);
    // Rectangular bricks use a running bond: alternate rows shift half a brick.
    // floor/frac keep the bond continuous across blocks and negative coordinates.
    float2 brickSize = float2(32.0f, 16.0f);
    float row = floor(texel.y / brickSize.y);
    float rowOffset = frac(row * 0.5f) * brickSize.x;
    float2 brickTexel = texel + float2(rowOffset, 0.0f);
    float2 tile = floor(brickTexel / brickSize);
    float2 local = brickTexel - tile * brickSize;
    float stoneTone = TerrainNoise(tile * 7.13f + 19.6f);
    float grain = TerrainNoise(texel * 0.73f + 31.2f);
    float mottling = TerrainNoise(texel * 0.12f + 5.8f);
    float3 stone = lerp(float3(0.36f, 0.39f, 0.42f),
        float3(0.58f, 0.59f, 0.57f), stoneTone);
    stone *= 0.88f + floor(mottling * 4.0f) * 0.055f;
    stone += (floor(grain * 4.0f) - 1.5f) * 0.014f;

    float2 farEdge = brickSize - 1.0f - local;
    float edge = min(min(local.x, local.y), min(farEdge.x, farEdge.y));
    // A two-texel joint and small chips separate stones without thick outlines.
    float chip = 0.0f;
    if (grain > 0.72f) {
        chip = 1.0f;
    }
    if (edge < 1.0f + chip) {
        return float3(0.20f, 0.22f, 0.24f) * (0.92f + grain * 0.16f);
    }
    if (local.x < 3.0f || local.y < 3.0f) {
        stone *= 1.16f;
    }
    if (farEdge.x < 3.0f || farEdge.y < 3.0f) {
        stone *= 0.76f;
    }
    // Sparse mineral flecks stay on the same 64-texel grid as the soil detail.
    if (grain > 0.82f) {
        stone *= 1.12f;
    } else if (grain < 0.16f) {
        stone *= 0.86f;
    }
    return stone;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gMaterial.color;

    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    if (gMaterial.enableLighting == 2)
    {
        // Fixed diffuse shading keeps ice silhouettes readable as the camera moves.
        float3 N = normalize(input.normal);
        float faceLight = saturate(dot(N, normalize(float3(-0.6f, 0.8f, -0.5f))) * 0.5f + 0.5f);
        float3 faceTint = lerp(float3(0.30f, 0.55f, 0.78f), float3(1.0f, 1.0f, 1.0f), faceLight);
        float3 iceTexture = lerp(float3(1.0f, 1.0f, 1.0f), textureColor.rgb, 0.25f);
        output.color = float4(gMaterial.color.rgb * iceTexture * faceTint, gMaterial.color.a * textureColor.a);
    }
    else if (gMaterial.enableLighting >= 3 && gMaterial.enableLighting <= 5)
    {
        // Archive-only materials: paper=3, leather=4, brass=5. Other scenes are unchanged.
        float3 N = normalize(input.normal);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        float3 L = normalize(float3(-3.5f, 6.0f, -6.0f) - input.worldPosition);
        float3 H = normalize(L + V);
        float pool = exp(-dot(input.worldPosition.xy * float2(0.095f, 0.10f),
                              input.worldPosition.xy * float2(0.095f, 0.10f)));
        float diffuse = saturate(dot(N, L));
        float3 base = gMaterial.color.rgb * textureColor.rgb;
        float3 illumination = float3(0.32f, 0.34f, 0.37f) +
            float3(0.85f, 0.72f, 0.52f) * (0.25f + diffuse * 0.65f) * pool;
        float occlusion = 1.0f;
        float specular = 0.0f;
        if (gMaterial.enableLighting == 3)
        {
            // Each page now uses one full texture. Shade only the real outer edges;
            // subdividing the UV here would create dark seams across the page.
            // environmentCoefficient carries contact strength only in archive mode.
            float pageU = saturate(transformedUV.x);
            float edgeDistance = min(pageU, 1.0f - pageU);
            float edgeShade = exp(-edgeDistance * 36.0f) * 0.08f;
            float contact = saturate(gMaterial.environmentCoefficient);
            float band = exp(-pow((edgeDistance - (0.12f + contact * 0.28f)) / 0.18f, 2.0f));
            occlusion = 1.0f - edgeShade - contact * (0.08f + band * 0.18f);
            // A little diffuse transmission, without a plastic highlight on paper.
            illumination += float3(0.12f, 0.10f, 0.07f) * saturate(dot(-N, L));
        }
        else
        {
            bool brass = gMaterial.enableLighting == 5;
            float grain = 0.85f + 0.15f * sin(transformedUV.x * 950.0f) * sin(transformedUV.y * 1130.0f);
            specular = pow(saturate(dot(N, H)), brass ? 72.0f : 30.0f) *
                (brass ? 0.38f : 0.07f) * pool * grain;
        }
        output.color = float4(base * illumination * saturate(occlusion) +
            float3(1.0f, 0.78f, 0.42f) * specular, gMaterial.color.a * textureColor.a);
    }
    else if (gMaterial.enableLighting == 8)
    {
        float3 normal = normalize(input.normal);
        output.color = float4(
            gMaterial.color.rgb * StoneTileColor(input.worldPosition, normal) *
                ToonIllumination(normal, input.worldPosition),
            gMaterial.color.a);
    }
    else if (gMaterial.enableLighting == 7)
    {
        float3 normal = normalize(input.normal);
        output.color = float4(
            gMaterial.color.rgb * MossTerrainColor(input.worldPosition, normal) *
                ToonIllumination(normal, input.worldPosition),
            gMaterial.color.a);
    }
    else if (gMaterial.enableLighting == 6)
    {
        output.color = float4(
            gMaterial.color.rgb * textureColor.rgb * ToonIllumination(input.normal, input.worldPosition),
            gMaterial.color.a * textureColor.a);
    }
    else if (gMaterial.enableLighting != 0)
    {
        float3 baseColor = gMaterial.color.rgb * textureColor.rgb;
        float3 N = normalize(input.normal);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        float3 ambient = baseColor * gAmbientLight.color.rgb * gAmbientLight.color.a;

        float3 Ld = normalize(-gDirectionalLight.direction);

        float NdotLd = saturate(dot(N, Ld));
        float3 dirDiffuse = baseColor * gDirectionalLight.color.rgb * NdotLd * gDirectionalLight.intensity;

        float3 Hd = normalize(Ld + V);
        float NdotHd = saturate(dot(N, Hd));
        // A zero shininess value disables highlights while keeping diffuse lighting.
        float3 dirSpec = gMaterial.shininess > 0.0f
            ? gDirectionalLight.color.rgb * gDirectionalLight.intensity * pow(NdotHd, gMaterial.shininess) : float3(0, 0, 0);

        float3 pointDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
        float3 pointSpec = float32_t3(0.0f, 0.0f, 0.0f);
        for (uint32_t lightIndex = 0; lightIndex < kMaxPointLights; ++lightIndex)
        {
            PointLight pointLight = gPointLights.lights[lightIndex];
            if (pointLight.isActive == 0)
            {
                continue;
            }

            float3 Lp = normalize(input.worldPosition - pointLight.position);
            float dist = length(pointLight.position - input.worldPosition);
            float decayF = pow(saturate(-dist / pointLight.radius + 1.0f), pointLight.decay);
            float3 pointColor = pointLight.color.rgb * pointLight.intensity * decayF;

            float NdotLp = saturate(dot(N, Lp));
            pointDiffuse += baseColor * pointColor * NdotLp;

            float3 Hp = normalize(Lp + V);
            float NdotHp = saturate(dot(N, Hp));
            if (gMaterial.shininess > 0.0f) {
                pointSpec += pointColor * pow(NdotHp, gMaterial.shininess);
            }
        }

        float3 spotDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
        float3 spotSpec = float32_t3(0.0f, 0.0f, 0.0f);
        for (uint32_t lightIndex = 0; lightIndex < kMaxSpotLights; ++lightIndex)
        {
            SpotLight spotLight = gSpotLights.lights[lightIndex];
            if (spotLight.isActive == 0)
            {
                continue;
            }

            float3 spotLightDirectionOnSurface = normalize(input.worldPosition - spotLight.position);
            float3 spotLightColor = spotLight.color.rgb * spotLight.intensity;
            float32_t cosAngle = dot(spotLightDirectionOnSurface, spotLight.direction);
            float32_t falloffFactor = saturate((cosAngle - spotLight.cosAngle) / (1.0f - spotLight.cosAngle));
            float distS = length(spotLight.position - input.worldPosition);
            float attenuationFactor = pow(saturate(-distS / spotLight.distance + 1.0f), spotLight.decay);

            spotLightColor *= attenuationFactor * falloffFactor;

            float NdotS = saturate(dot(N, spotLightDirectionOnSurface));
            spotDiffuse += baseColor * spotLightColor * NdotS;

            float3 Hs = normalize(spotLightDirectionOnSurface + V);
            float NdotHs = saturate(dot(N, Hs));
            if (gMaterial.shininess > 0.0f) {
                spotSpec += spotLightColor * pow(NdotHs, gMaterial.shininess);
            }
        }

        output.color.rgb = ambient + dirDiffuse + dirSpec + pointDiffuse + pointSpec + spotDiffuse + spotSpec;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    if (gMaterial.enableEnvironmentMap != 0 && gMaterial.enableLighting != 6 &&
        gMaterial.enableLighting != 7 && gMaterial.enableLighting != 8)
    {
        float3 N = normalize(input.normal);
        float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float3 reflectedVector = reflect(cameraToPosition, N);

        float4 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);

        output.color.rgb += environmentColor.rgb * gMaterial.environmentCoefficient;
    }

    return output;
}
