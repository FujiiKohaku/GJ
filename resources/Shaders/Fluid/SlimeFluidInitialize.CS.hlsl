#include "SlimeFluidCommon.hlsli"

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t index = dispatchThreadId.x;
    if (index >= particleCount)
    {
        return;
    }

    // neo_Engine の実働経路 Renderer::EmitGPUFluid / FluidSimCS::Emit と同じ配置。
    uint32_t sx = index * 123u;
    sx = (sx << 13u) ^ sx;
    sx = sx * (sx * sx * 15731u + 789221u) + 1376312589u;
    uint32_t sy = index * 456u;
    sy = (sy << 13u) ^ sy;
    sy = sy * (sy * sy * 15731u + 789221u) + 1376312589u;
    uint32_t sz = index * 789u;
    sz = (sz << 13u) ^ sz;
    sz = sz * (sz * sz * 15731u + 789221u) + 1376312589u;
    float32_t3 randomVector = float32_t3(
        (float32_t(sx & 0x7fffffffu) / float32_t(0x7fffffffu) - 0.5f) * 2.0f,
        (float32_t(sy & 0x7fffffffu) / float32_t(0x7fffffffu) - 0.5f) * 2.0f,
        (float32_t(sz & 0x7fffffffu) / float32_t(0x7fffffffu) - 0.5f) * 2.0f);
    uint32_t sr = index * 999u;
    sr = (sr << 13u) ^ sr;
    sr = sr * (sr * sr * 15731u + 789221u) + 1376312589u;
    float32_t radiusRandom = float32_t(sr & 0x7fffffffu) / float32_t(0x7fffffffu);
    float32_t3 localPosition = float32_t3(0.0f, 0.0f, 0.0f);
    if (length(randomVector) > 0.01f)
    {
        localPosition = normalize(randomVector) * pow(radiusRandom, 0.3333f) * 1.5f;
    }

    float32_t3 forward = normalize(coreForward);
    float32_t3 up = float32_t3(0.0f, 1.0f, 0.0f);
    if (abs(forward.y) > 0.98f)
    {
        up = float32_t3(0.0f, 0.0f, 1.0f);
    }
    float32_t3 right = normalize(cross(up, forward));
    up = normalize(cross(forward, right));

    // neoでは放出位置から、0.8上にある形状コアへ収束する。
    float32_t3 worldPosition = corePosition - up * 0.03096f +
        right * (localPosition.x * blobRadii.x) +
        up * (localPosition.y * blobRadii.y) +
        forward * (localPosition.z * blobRadii.z);

    SlimeFluidParticle particle;
    particle.restPosition = localPosition;
    particle.position = worldPosition;
    particle.velocity = float32_t3(0.0f, 0.0f, 0.0f);
    particle.density = restDensity;
    particle.pressure = 0.0f;
    particle.padding = 9999.0f;

    gParticles[index] = particle;
    gForces[index] = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
}
