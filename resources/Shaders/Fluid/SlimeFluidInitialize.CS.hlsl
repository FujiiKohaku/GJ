#include "SlimeFluidCommon.hlsli"

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t index = dispatchThreadId.x;
    if (index >= particleCount)
    {
        return;
    }

    float32_t angle = HashIndex(index, 19.7f) * kPi * 2.0f;
    float32_t cosTheta = lerp(-1.0f, 1.0f, HashIndex(index, 3.1f));
    float32_t sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));
    float32_t radius = pow(HashIndex(index, 43.3f), 1.0f / 3.0f);

    float32_t3 localPosition = float32_t3(
        cos(angle) * sinTheta * radius,
        cosTheta * radius,
        sin(angle) * sinTheta * radius);

    float32_t3 forward = normalize(coreForward);
    float32_t3 up = float32_t3(0.0f, 1.0f, 0.0f);
    if (abs(forward.y) > 0.98f)
    {
        up = float32_t3(0.0f, 0.0f, 1.0f);
    }
    float32_t3 right = normalize(cross(up, forward));
    up = normalize(cross(forward, right));

    float32_t3 worldPosition = corePosition +
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
