#include "SlimeFluidCommon.hlsli"

void ResolveBoundary(inout SlimeFluidParticle particle)
{
    float32_t3 minPosition = boundsMin + particleRadius + boundaryPadding;
    float32_t3 maxPosition = boundsMax - particleRadius - boundaryPadding;

    if (particle.position.x < minPosition.x)
    {
        particle.position.x = minPosition.x;
        particle.velocity.x *= -damping;
    }
    if (particle.position.x > maxPosition.x)
    {
        particle.position.x = maxPosition.x;
        particle.velocity.x *= -damping;
    }

    if (particle.position.y < minPosition.y)
    {
        particle.position.y = minPosition.y;
        particle.velocity.y *= -damping;
        particle.velocity.xz *= 0.92f;
    }
    if (particle.position.y > maxPosition.y)
    {
        particle.position.y = maxPosition.y;
        particle.velocity.y *= -damping;
    }

    if (particle.position.z < minPosition.z)
    {
        particle.position.z = minPosition.z;
        particle.velocity.z *= -damping;
    }
    if (particle.position.z > maxPosition.z)
    {
        particle.position.z = maxPosition.z;
        particle.velocity.z *= -damping;
    }
}

void ResolveSlimeEnvelope(inout SlimeFluidParticle particle)
{
    float32_t3 forward = normalize(coreForward);
    float32_t3 up = float32_t3(0.0f, 1.0f, 0.0f);
    if (abs(forward.y) > 0.98f)
    {
        up = float32_t3(0.0f, 0.0f, 1.0f);
    }
    float32_t3 right = normalize(cross(up, forward));
    up = normalize(cross(forward, right));

    float32_t3 toParticle = particle.position - corePosition;
    float32_t localX = dot(toParticle, right);
    float32_t localY = dot(toParticle, up);
    float32_t localZ = dot(toParticle, forward);

    float32_t y01 = saturate(localY / max(blobRadii.y, 0.001f));
    float32_t allowedRadius =
        lerp(blobRadii.x * 1.08f, blobRadii.x * 0.36f, pow(y01, 1.15f));
    float32_t2 horizontal = float32_t2(localX, localZ / max(blobRadii.z / blobRadii.x, 0.001f));
    float32_t horizontalLength = length(horizontal);
    if (horizontalLength > allowedRadius)
    {
        float32_t2 clamped = horizontal / max(horizontalLength, kEpsilon) * allowedRadius;
        float32_t targetX = clamped.x;
        float32_t targetZ = clamped.y * max(blobRadii.z / blobRadii.x, 0.001f);
        float32_t3 target =
            corePosition +
            right * targetX +
            up * localY +
            forward * targetZ;
        particle.position = lerp(particle.position, target, saturate(deltaTime * 10.0f));
        particle.velocity.xz *= 0.94f;
    }

    if (particle.position.y < floorHeight + particleRadius * 0.55f)
    {
        particle.position.y = floorHeight + particleRadius * 0.55f;
        particle.velocity.y = max(particle.velocity.y * -damping, 0.0f);
        particle.velocity.xz *= horizontalFriction;
    }
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint32_t index = dispatchThreadId.x;
    if (index >= particleCount)
    {
        return;
    }

    SlimeFluidParticle particle = gParticles[index];
    float32_t3 acceleration =
        gForces[index].xyz / max(particle.density, kEpsilon);

    particle.velocity += acceleration * deltaTime;
    particle.velocity *= 0.999f;
    particle.position += particle.velocity * deltaTime;

    // 液状化中（shapeAttraction == 0）はエンベロープ制限を無効化し、自由に流れさせる
    if (shapeAttraction > kEpsilon) {
        ResolveSlimeEnvelope(particle);
    } else {
        // 液状化中でも床との衝突だけは保持
        if (particle.position.y < floorHeight + particleRadius * 0.55f) {
            particle.position.y = floorHeight + particleRadius * 0.55f;
            particle.velocity.y = max(particle.velocity.y * -damping, 0.0f);
            particle.velocity.xz *= 0.85f; // 床上で水のように広がる
        }
    }
    ResolveBoundary(particle);
    gParticles[index] = particle;
}
