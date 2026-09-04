#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Math/MathStruct.h"
#include "Engine/SrvManager/SrvManager.h"

#include <cstdint>
#include <vector>
#include <wrl.h>

class GpuSphFluid {
public:
    struct Particle {
        Vector3 position;
        float density;
        Vector3 velocity;
        float pressure;
        Vector3 restPosition;
        float padding;
    };

    struct CollisionObstacle {
        Vector3 center;
        float padding0 = 0.0f;
        Vector3 halfSize;
        float padding1 = 0.0f;
        Vector3 velocity;
        float padding2 = 0.0f;
    };

    struct Settings {
        uint32_t particleCount = 1024;
        uint32_t maxObstacleCount = 1024;
        float particleRadius = 0.08f;
        float smoothingRadius = 0.4f;
        float restDensity = 3.0f;
        float particleMass = 1.0f;
        float viscosity = 8.0f;
        float stiffness = 48.0f;
        float surfaceTension = 14.0f;

        Vector3 gravity = { 0.0f, -9.8f, 0.0f };
        float damping = 0.22f;

        Vector3 boundsMin = { -50.0f, 0.0f, -50.0f };
        float boundaryPadding = 0.05f;
        Vector3 boundsMax = { 50.0f, 50.0f, 50.0f };
        uint32_t spawnColumns = 16;

        Vector3 spawnOrigin = { -1.0f, 3.2f, -1.0f };
        uint32_t spawnRows = 16;
        Vector3 spawnSpacing = { 0.16f, 0.16f, 0.16f };
        uint32_t spawnLayers = 4;

        Vector3 corePosition = { 0.0f, 0.16f, 0.0f };
        float floorHeight = 0.0f;
        Vector3 coreForward = { 0.0f, 0.0f, 1.0f };
        Vector3 targetVelocity = { 0.0f, 0.0f, 0.0f };
        Vector3 blobRadii = { 1.0f, 1.15f, 0.82f };
        float shapeAttraction = 3.2f;
        float velocityAttraction = 5.5f;
        float horizontalFriction = 0.88f;
        float liquidShapeAttraction = 0.18f;
        float liquidVelocityAttraction = 0.45f;
        float liquidViscosity = 1.4f;
        float liquidSurfaceTension = 2.2f;
        float liquidDamping = 0.06f;
        float liquidHorizontalFriction = 0.985f;
        float liquidGravityScale = 1.35f;
        float liquidTransitionSpeed = 6.5f;
        float gatherTransitionSpeed = 3.5f;
        float sloshStrength = 1.8f;
        float puddleSpread = 4.5f;
        float emitterRate = 520.0f;
        float emitterRadius = 0.16f;
        float emitterSpeed = 6.0f;
        float particleLifetime = 5.5f;
        float collisionFriction = 0.82f;
        float collisionBounce = 0.10f;
        float wallMinX = -1000.0f;
        float wallMaxX = 1000.0f;
        float wallMinY = -1000.0f;
        float wallMaxY = 1000.0f;
        uint32_t simulationSubsteps = 2;
    };

    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        const Settings& settings = Settings());
    void Reset(const Settings& settings);
    void SetControlState(
        const Vector3& corePosition,
        const Vector3& targetVelocity,
        const Vector3& coreForward);
    void SetEmitter(bool enabled, const Vector3& position, const Vector3& velocity);
    void TriggerEmitBurst(uint32_t count);
    void SetObstacles(const std::vector<CollisionObstacle>& obstacles);
    void SetBlobRadii(const Vector3& blobRadii) { settings_.blobRadii = blobRadii; }
    void SetFloorHeight(float floorHeight) { settings_.floorHeight = floorHeight; }
    void SetGrounded(bool grounded) { isGrounded_ = grounded; }
    bool IsGrounded() const { return isGrounded_; }
    void SetEyeOffsetX(float offset) { eyeOffsetX_ = offset; }
    float GetEyeOffsetX() const { return eyeOffsetX_; }
    void SetEyeOffsetY(float offset) { eyeOffsetY_ = offset; }
    float GetEyeOffsetY() const { return eyeOffsetY_; }
    void SetWallBoundaries(float wallMinX, float wallMaxX, float wallMinZ = -0.3f, float wallMaxZ = 0.3f, float wallMinY = -1000.0f, float wallMaxY = 1000.0f);
    void TriggerLiquidationBurst(float strength = 8.0f);
    void SetLiquidated(bool liquidated) { isLiquidated_ = liquidated; }
    bool IsLiquidated() const { return isLiquidated_; }
    void Update(float deltaTime);
    void Finalize();

    bool IsInitialized() const { return dxCommon_ != nullptr; }
    uint32_t GetParticleCount() const { return settings_.particleCount; }
    float GetParticleRadius() const { return settings_.particleRadius; }
    const Settings& GetSettings() const { return settings_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetParticleSrvHandleGPU() const { return particleSrvHandleGPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetForceSrvHandleGPU() const { return forceSrvHandleGPU_; }

    std::vector<Particle> GetParticlesCPU() const;
    void SetParticlesCPU(const std::vector<Particle>& particles);

private:
    struct SimulationParameter {
        uint32_t particleCount;
        float deltaTime;
        float smoothingRadius;
        float particleMass;

        float restDensity;
        float stiffness;
        float viscosity;
        float surfaceTension;

        Vector3 gravity;
        float damping;

        Vector3 boundsMin;
        float particleRadius;

        Vector3 boundsMax;
        float boundaryPadding;

        Vector3 spawnOrigin;
        uint32_t spawnColumns;

        Vector3 spawnSpacing;
        uint32_t spawnRows;

        uint32_t spawnLayers;
        float shapeAttraction;
        float velocityAttraction;
        float horizontalFriction;

        Vector3 corePosition;
        float floorHeight;

        Vector3 coreForward;
        float padding0;

        Vector3 targetVelocity;
        float padding1;

        Vector3 blobRadii;
        float padding2;

        Vector3 coreDelta;
        float padding3;

        float wallMinX;
        float wallMaxX;
        float wallMinY;
        float wallMaxY;

        float wallMinZ;
        float wallMaxZ;
        float paddingWall0;
        float paddingWall1;

        float liquidationBurstStrength;
        float liquidBlend;
        float sloshStrength;
        float puddleSpread;

        uint32_t emitStartIndex;
        uint32_t emitCount;
        uint32_t obstacleCount;
        float particleLifetime;

        Vector3 emitterPosition;
        float emitterRadius;

        Vector3 emitterVelocity;
        float emitterSpeed;

        float collisionFriction;
        float collisionBounce;
        float padding4;
        float padding5;
    };

    static constexpr uint32_t kInvalidDescriptorIndex = 0xffffffffu;
    static constexpr uint32_t kThreadGroupSize = 256;

    void CreateResources();
    void CreateDescriptors();
    void CreateConstantBuffer();
    void CreateRootSignature();
    void CreatePipelineStates();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateComputePipeline(const std::wstring& shaderPath);

    void UpdateSimulationParameter(float deltaTime, bool includeEmission);
    void UpdateFrameState(float deltaTime);
    void Dispatch(ID3D12PipelineState* pipelineState);
    void TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES& currentState,
        D3D12_RESOURCE_STATES nextState);
    void InsertUavBarrier(ID3D12Resource* resource);
    void ReleaseDescriptor(uint32_t& descriptorIndex);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Settings settings_ {};
    bool needsReset_ = true;
    bool isLiquidated_ = false;
    bool isGrounded_ = false;
    bool hasPreviousCorePosition_ = false;
    bool emitterEnabled_ = false;
    float liquidBlend_ = 0.0f;
    float eyeOffsetX_ = 0.0f;
    float eyeOffsetY_ = 0.0f;
    float liquidationBurstStrength_ = 0.0f;
    float emitAccumulator_ = 0.0f;
    uint32_t emitCursor_ = 0;
    uint32_t pendingEmitCount_ = 0;
    uint32_t burstEmitCount_ = 0;
    Vector3 previousCorePosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 coreVelocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 emitterPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 emitterVelocity_ = { 0.0f, 0.0f, 0.0f };
    uint32_t obstacleCount_ = 0;
    float wallMinZ_ = -0.3f;
    float wallMaxZ_ = 0.3f;

    Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> forceResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> obstacleResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> simulationParameterResource_;
    SimulationParameter* simulationParameterData_ = nullptr;
    CollisionObstacle* obstacleData_ = nullptr;

    uint32_t particleSrvIndex_ = kInvalidDescriptorIndex;
    uint32_t forceSrvIndex_ = kInvalidDescriptorIndex;
    uint32_t obstacleSrvIndex_ = kInvalidDescriptorIndex;
    uint32_t particleUavIndex_ = kInvalidDescriptorIndex;
    uint32_t forceUavIndex_ = kInvalidDescriptorIndex;
    D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE forceSrvHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE obstacleSrvHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE forceUavHandleGPU_ {};

    D3D12_RESOURCE_STATES particleState_ = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES forceState_ = D3D12_RESOURCE_STATE_COMMON;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initializePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> densityPressurePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> forcePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> integratePipelineState_;
};
