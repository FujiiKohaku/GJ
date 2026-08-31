#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Math/MathStruct.h"
#include "Engine/SrvManager/SrvManager.h"

#include <cstdint>
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

    struct Settings {
        uint32_t particleCount = 1024;
        float particleRadius = 0.08f;
        float smoothingRadius = 0.4f;
        float restDensity = 3.0f;
        float particleMass = 1.0f;
        float viscosity = 15.0f;
        float stiffness = 50.0f;
        float surfaceTension = 15.0f;

        Vector3 gravity = { 0.0f, -9.8f, 0.0f };
        float damping = 0.55f;

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
        float shapeAttraction = 60.0f;
        float velocityAttraction = 12.0f;
        float horizontalFriction = 0.72f;
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
    void SetLiquidated(bool liquidated) { isLiquidated_ = liquidated; }
    bool IsLiquidated() const { return isLiquidated_; }
    void Update(float deltaTime);
    void Finalize();

    bool IsInitialized() const { return dxCommon_ != nullptr; }
    uint32_t GetParticleCount() const { return settings_.particleCount; }
    float GetParticleRadius() const { return settings_.particleRadius; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetParticleSrvHandleGPU() const { return particleSrvHandleGPU_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetForceSrvHandleGPU() const { return forceSrvHandleGPU_; }

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
    };

    static constexpr uint32_t kInvalidDescriptorIndex = 0xffffffffu;
    static constexpr uint32_t kThreadGroupSize = 256;

    void CreateResources();
    void CreateDescriptors();
    void CreateConstantBuffer();
    void CreateRootSignature();
    void CreatePipelineStates();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateComputePipeline(const std::wstring& shaderPath);

    void UpdateSimulationParameter(float deltaTime);
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

    Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> forceResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> simulationParameterResource_;
    SimulationParameter* simulationParameterData_ = nullptr;

    uint32_t particleSrvIndex_ = kInvalidDescriptorIndex;
    uint32_t forceSrvIndex_ = kInvalidDescriptorIndex;
    uint32_t particleUavIndex_ = kInvalidDescriptorIndex;
    uint32_t forceUavIndex_ = kInvalidDescriptorIndex;
    D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU_ {};
    D3D12_GPU_DESCRIPTOR_HANDLE forceSrvHandleGPU_ {};
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
