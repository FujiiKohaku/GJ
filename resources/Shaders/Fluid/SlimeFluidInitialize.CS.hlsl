#include "SlimeFluidCommon.hlsli"

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t index = dispatchThreadId.x;
    if (index >= particleCount)
    {
        return;
    }

    float32_t yRandom = HashIndex(index, 3.1f);
    float32_t angle = HashIndex(index, 19.7f) * kPi * 2.0f;
    float32_t diskRadius = sqrt(HashIndex(index, 43.3f));

    float32_t localY = pow(yRandom, 1.45f);
    float32_t capRadius =
        lerp(0.30f, 1.0f, pow(saturate(1.0f - localY), 0.55f));
    float32_t skirt = pow(saturate(1.0f - localY), 4.0f) * 0.18f;
    float32_t radius = (capRadius + skirt) * diskRadius;

    float32_t3 localPosition = float32_t3(
        cos(angle) * radius,
        localY,
        sin(angle) * radius);

    float32_t3 forward = normalize(coreForward);
    float32_t3 up = float32_t3(0.0f, 1.0f, 0.0f);
    if (abs(forward.y) > 0.98f)
    {
        up = float32_t3(0.0f, 0.0f, 1.0f);
    }
    float32_t3 right = normalize(cross(up, forward));
    up = normalize(cross(forward, right));

    SlimeFluidParticle particle;
    particle.restPosition = localPosition;
    particle.position =
        corePosition +
        right * (localPosition.x * blobRadii.x) +
        up * (localPosition.y * blobRadii.y) +
        forward * (localPosition.z * blobRadii.z);
    particle.velocity = float32_t3(0.0f, 0.0f, 0.0f);
    particle.density = restDensity;
    particle.pressure = 0.0f;
    particle.padding = 0.0f;

    gParticles[index] = particle;
    gForces[index] = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
}
