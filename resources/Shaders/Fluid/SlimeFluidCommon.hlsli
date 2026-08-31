#ifndef SLIME_FLUID_COMMON_HLSLI
#define SLIME_FLUID_COMMON_HLSLI

static const float kPi = 3.14159265359f;
static const float kEpsilon = 0.00001f;

struct SlimeFluidParticle
{
    float32_t3 position;
    float32_t density;
    float32_t3 velocity;
    float32_t pressure;
    float32_t3 restPosition;
    float32_t padding;
};

cbuffer SlimeFluidSimulationParameter : register(b0)
{
    uint32_t particleCount;
    float32_t deltaTime;
    float32_t smoothingRadius;
    float32_t particleMass;

    float32_t restDensity;
    float32_t stiffness;
    float32_t viscosity;
    float32_t surfaceTension;

    float32_t3 gravity;
    float32_t damping;

    float32_t3 boundsMin;
    float32_t particleRadius;

    float32_t3 boundsMax;
    float32_t boundaryPadding;

    float32_t3 spawnOrigin;
    uint32_t spawnColumns;

    float32_t3 spawnSpacing;
    uint32_t spawnRows;

    uint32_t spawnLayers;
    float32_t shapeAttraction;
    float32_t velocityAttraction;
    float32_t horizontalFriction;

    float32_t3 corePosition;
    float32_t floorHeight;

    float32_t3 coreForward;
    float32_t padding0;

    float32_t3 targetVelocity;
    float32_t padding1;

    float32_t3 blobRadii;
    float32_t padding2;
};

RWStructuredBuffer<SlimeFluidParticle> gParticles : register(u0);
RWStructuredBuffer<float32_t4> gForces : register(u1);

float32_t Poly6Kernel(float32_t r2, float32_t h)
{
    float32_t h2 = h * h;
    if (r2 >= h2)
    {
        return 0.0f;
    }

    float32_t x = h2 - r2;
    return 315.0f / (64.0f * kPi * pow(h, 9.0f)) * x * x * x;
}

float32_t SpikyGradientScale(float32_t r, float32_t h)
{
    if (r <= kEpsilon || r >= h)
    {
        return 0.0f;
    }

    float32_t x = h - r;
    return -45.0f / (kPi * pow(h, 6.0f)) * x * x / r;
}

float32_t ViscosityLaplacian(float32_t r, float32_t h)
{
    if (r >= h)
    {
        return 0.0f;
    }

    return 45.0f / (kPi * pow(h, 6.0f)) * (h - r);
}

uint3 DecodeSpawnIndex(uint32_t index)
{
    uint32_t columns = max(spawnColumns, 1u);
    uint32_t rows = max(spawnRows, 1u);
    uint32_t slice = columns * rows;
    uint32_t z = index / slice;
    uint32_t remainder = index - z * slice;
    uint32_t y = remainder / columns;
    uint32_t x = remainder - y * columns;
    return uint3(x, y, z % max(spawnLayers, 1u));
}

float32_t Hash11(float32_t value)
{
    return frac(sin(value * 12.9898f) * 43758.5453f);
}

float32_t HashIndex(uint32_t index, float32_t salt)
{
    return Hash11((float32_t) index + salt);
}

#endif
