#include "SlimeFluidCommon.hlsli"

StructuredBuffer<SlimeFluidObstacle> gObstacles : register(t0);

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

bool IsEmittedThisFrame(uint32_t index)
{
    if (emitCount == 0)
    {
        return false;
    }

    uint32_t relativeIndex =
        index >= emitStartIndex
            ? index - emitStartIndex
            : index + particleCount - emitStartIndex;
    return relativeIndex < emitCount;
}

void SpawnLiquidParticle(inout SlimeFluidParticle particle, uint32_t index)
{
    float32_t angle = HashIndex(index, 17.0f + particleLifetime) * kPi * 2.0f;
    float32_t radius = sqrt(HashIndex(index, 29.0f + particleLifetime)) * emitterRadius;
    float32_t heightJitter = (HashIndex(index, 41.0f + particleLifetime) - 0.5f) * emitterRadius;

    float32_t3 randomOffset = float32_t3(
        cos(angle) * radius,
        heightJitter,
        sin(angle) * radius * 0.45f);

    float32_t fan = HashIndex(index, 53.0f + particleLifetime) - 0.5f;
    float32_t upward = lerp(0.55f, 1.25f, HashIndex(index, 67.0f + particleLifetime));
    particle.position = emitterPosition + randomOffset;
    particle.velocity =
        emitterVelocity +
        float32_t3(fan * emitterSpeed * 0.75f, emitterSpeed * upward, fan * emitterSpeed * 0.18f);
    particle.density = restDensity;
    particle.pressure = 0.0f;
    particle.padding = particleLifetime;
}

void DeactivateParticle(inout SlimeFluidParticle particle)
{
    particle.position = float32_t3(0.0f, -10000.0f, 0.0f);
    particle.velocity = float32_t3(0.0f, 0.0f, 0.0f);
    particle.density = restDensity;
    particle.pressure = 0.0f;
    particle.padding = -1.0f;
}

void ResolveObstacleCollision(inout SlimeFluidParticle particle)
{
    float32_t contactRadius = particleRadius * 1.05f;

    [loop]
    for (uint32_t obstacleIndex = 0; obstacleIndex < obstacleCount; ++obstacleIndex)
    {
        SlimeFluidObstacle obstacle = gObstacles[obstacleIndex];
        float32_t3 expandedHalfSize = obstacle.halfSize + contactRadius;
        float32_t3 delta = particle.position - obstacle.center;
        float32_t3 overlap = expandedHalfSize - abs(delta);
        if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f)
        {
            continue;
        }

        float32_t3 normal = float32_t3(0.0f, 0.0f, 0.0f);
        float32_t penetration = overlap.x;
        normal.x = delta.x < 0.0f ? -1.0f : 1.0f;

        if (overlap.y < penetration)
        {
            penetration = overlap.y;
            normal = float32_t3(0.0f, delta.y < 0.0f ? -1.0f : 1.0f, 0.0f);
        }
        if (overlap.z < penetration)
        {
            penetration = overlap.z;
            normal = float32_t3(0.0f, 0.0f, delta.z < 0.0f ? -1.0f : 1.0f);
        }

        particle.position += normal * penetration;

        float32_t3 relativeVelocity = particle.velocity - obstacle.velocity;
        float32_t normalSpeed = dot(relativeVelocity, normal);
        if (normalSpeed < 0.0f)
        {
            relativeVelocity -= normal * normalSpeed * (1.0f + collisionBounce);
        }

        float32_t3 normalVelocity = normal * dot(relativeVelocity, normal);
        float32_t3 tangentVelocity = relativeVelocity - normalVelocity;
        particle.velocity =
            obstacle.velocity +
            normalVelocity +
            tangentVelocity * collisionFriction;
    }
}

void ResolveLiquidPuddle(inout SlimeFluidParticle particle)
{
    particle.velocity.xz *= lerp(horizontalFriction, 0.995f, liquidBlend);
}

void ResolveSlimeEnvelope(inout SlimeFluidParticle particle, float32_t envelopeBlend)
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

    float32_t3 localToCore = float32_t3(localX, localY, localZ);
    float32_t3 scaledToCore = localToCore / max(blobRadii, float32_t3(0.01f, 0.01f, 0.01f));
    float32_t distC = length(scaledToCore);
    float32_t maxRadiusC = 1.2f; // neo_Engine FluidSimCS.hlsl の基本クランプ半径

    if (distC > maxRadiusC)
    {
        float32_t3 surfaceLocal = (scaledToCore / distC) * maxRadiusC;
        float32_t3 targetLocal = surfaceLocal * blobRadii;
        float32_t3 targetWorld = corePosition + right * targetLocal.x + up * targetLocal.y + forward * targetLocal.z;

        float32_t lerpFactor = saturate(10.0f * deltaTime * envelopeBlend);
        particle.position = lerp(particle.position, targetWorld, lerpFactor);

        // 膜を突き破ろうとする外向き速度のキャンセリング
        float32_t3 outwardNormal = normalize(particle.position - corePosition);
        float32_t outwardSpeed = dot(particle.velocity, outwardNormal);
        if (outwardSpeed > 0.0f)
        {
            particle.velocity -= outwardNormal * outwardSpeed * 0.85f;
        }
    }
}

void ResolveWallAndFloorCollision(inout SlimeFluidParticle particle)
{
    // neo_Engineの床オフセットもワールド寸法と同じ36%へ縮小する。
    float32_t minFloorY = floorHeight + 0.072f;
    if (particle.position.y < minFloorY)
    {
        particle.position.y = minFloorY;
        
        if (particle.velocity.y < 0.0f)
        {
            particle.velocity.y *= -collisionBounce;
        }
        particle.velocity.xz *= collisionFriction;
    }

    float32_t minWallX = wallMinX + particleRadius;
    float32_t maxWallX = wallMaxX - particleRadius;
    if (particle.position.x < minWallX)
    {
        particle.position.x = minWallX;
        if (particle.velocity.x < 0.0f)
        {
            particle.velocity.x *= -collisionBounce;
        }
    }
    else if (particle.position.x > maxWallX)
    {
        particle.position.x = maxWallX;
        if (particle.velocity.x > 0.0f)
        {
            particle.velocity.x *= -collisionBounce;
        }
    }

    // 手前と奥の見えない壁(Z軸バウンダリ)への衝突・閉じ込め処理
    float32_t minWallZ = wallMinZ + particleRadius;
    float32_t maxWallZ = wallMaxZ - particleRadius;
    if (particle.position.z < minWallZ)
    {
        particle.position.z = minWallZ;
        if (particle.velocity.z < 0.0f)
        {
            particle.velocity.z *= -collisionBounce;
        }
    }
    else if (particle.position.z > maxWallZ)
    {
        particle.position.z = maxWallZ;
        if (particle.velocity.z > 0.0f)
        {
            particle.velocity.z *= -collisionBounce;
        }
    }

    float32_t maxCeilingY = wallMaxY - particleRadius;
    if (particle.position.y > maxCeilingY)
    {
        particle.position.y = maxCeilingY;
        if (particle.velocity.y > 0.0f)
        {
            particle.velocity.y *= -collisionBounce;
        }
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
    if (IsEmittedThisFrame(index))
    {
        SpawnLiquidParticle(particle, index);
        ResolveObstacleCollision(particle);
        ResolveWallAndFloorCollision(particle);
        ResolveBoundary(particle);
        gParticles[index] = particle;
        return;
    }

    if (particle.padding <= 0.0f && liquidBlend > 0.5f)
    {
        DeactivateParticle(particle);
        gParticles[index] = particle;
        return;
    }

    if (liquidBlend > 0.5f)
    {
        particle.padding -= deltaTime;
        if (particle.padding <= 0.0f)
        {
            DeactivateParticle(particle);
            gParticles[index] = particle;
            return;
        }
    }
    else
    {
        particle.padding = 9999.0f;
    }

    float32_t3 acceleration =
        gForces[index].xyz / max(particle.density, kEpsilon);

    particle.velocity += acceleration * deltaTime;

    if (liquidationBurstStrength > kEpsilon)
    {
        float32_t angle = HashIndex(index, 71.3f) * kPi * 2.0f;
        float32_t speed01 = lerp(0.45f, 1.0f, HashIndex(index, 97.9f));
        particle.velocity.x += cos(angle) * liquidationBurstStrength * speed01;
        particle.velocity.z += sin(angle) * liquidationBurstStrength * speed01 * 0.20f;
        particle.velocity.y = min(particle.velocity.y, 0.0f);
    }
    
    float32_t quietDamping = lerp(1.0f, 0.996f, liquidBlend);
    float32_t movingDamping = lerp(1.0f, 0.999f, liquidBlend);
    if (length(targetVelocity) < 0.1f) {
        particle.velocity *= quietDamping;
    } else {
        particle.velocity *= movingDamping;
    }

    float32_t maxSpeed = 30.0f;
    float32_t speed = length(particle.velocity);
    if (speed > maxSpeed)
    {
        particle.velocity *= maxSpeed / speed;
    }

    particle.position += particle.velocity * deltaTime;

    float32_t envelopeBlend = saturate(1.0f - liquidBlend * 1.35f);
    if (envelopeBlend > 0.001f) {
        ResolveSlimeEnvelope(particle, envelopeBlend);
    } else {
        ResolveLiquidPuddle(particle);
    }
    ResolveObstacleCollision(particle);
    ResolveWallAndFloorCollision(particle);
    ResolveBoundary(particle);
    gParticles[index] = particle;
}
