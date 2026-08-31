#include "SlimeFluidCommon.hlsli"

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t index = dispatchThreadId.x;
    if (index >= particleCount)
    {
        return;
    }

    SlimeFluidParticle particle = gParticles[index];
    float32_t3 pressureForce = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 viscosityForce = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 cohesionForce = float32_t3(0.0f, 0.0f, 0.0f);

    for (uint32_t otherIndex = 0; otherIndex < particleCount; ++otherIndex)
    {
        if (otherIndex == index)
        {
            continue;
        }

        SlimeFluidParticle other = gParticles[otherIndex];
        float32_t3 offset = particle.position - other.position;
        float32_t distance = length(offset);
        if (distance >= smoothingRadius || distance <= kEpsilon)
        {
            continue;
        }

        float32_t density = max(other.density, kEpsilon);
        float32_t pressureTerm =
            (particle.pressure + other.pressure) / (2.0f * density);
        pressureForce +=
            -particleMass * pressureTerm *
            SpikyGradientScale(distance, smoothingRadius) * offset;

        viscosityForce +=
            viscosity * particleMass *
            (other.velocity - particle.velocity) / density *
            ViscosityLaplacian(distance, smoothingRadius);

        float32_t surfaceWeight =
            saturate(1.0f - distance / smoothingRadius);
        cohesionForce +=
            surfaceTension * surfaceWeight * surfaceWeight *
            (other.position - particle.position);
    }

    float32_t3 gravityForce = gravity * particle.density;
    float32_t3 forward = normalize(coreForward);
    float32_t3 up = float32_t3(0.0f, 1.0f, 0.0f);
    if (abs(forward.y) > 0.98f)
    {
        up = float32_t3(0.0f, 0.0f, 1.0f);
    }
    float32_t3 right = normalize(cross(up, forward));
    up = normalize(cross(forward, right));

    // 水風船アプローチ: 個別のrestPositionではなく、全パーティクルをコア(中心)に引き寄せる
    // SPHの圧力が反発力となり、ぷるぷるとしたゼリー状の動き(Slosh)が生まれる
    float32_t3 toCore = corePosition - particle.position;
    float32_t3 shapeAcceleration = toCore * shapeAttraction;
    shapeAcceleration.y *= 2.0f; // NeoEngine同様にY軸の復元力を倍増させて重力による潰れを防ぐ

    shapeAcceleration += (targetVelocity - particle.velocity) * velocityAttraction;
    float32_t3 shapeForce = shapeAcceleration * max(particle.density, restDensity);

    gForces[index] =
        float32_t4(pressureForce + viscosityForce + cohesionForce + gravityForce + shapeForce, 0.0f);
}
