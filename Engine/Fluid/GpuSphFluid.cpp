#include "Engine/Fluid/GpuSphFluid.h"

#include "Engine/Logger/Logger.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace {
float MoveTowards(float current, float target, float maxDelta)
{
    const float delta = target - current;
    if (std::abs(delta) <= maxDelta) {
        return target;
    }
    return current + (delta > 0.0f ? maxDelta : -maxDelta);
}

float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}
}

void GpuSphFluid::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    const Settings& settings)
{
    assert(dxCommon != nullptr);
    assert(srvManager != nullptr);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    settings_ = settings;
    settings_.particleCount = (std::max<uint32_t>)(1, settings_.particleCount);
    settings_.maxObstacleCount = (std::max<uint32_t>)(1, settings_.maxObstacleCount);
    previousCorePosition_ = settings_.corePosition;
    hasPreviousCorePosition_ = false;
    coreVelocity_ = { 0.0f, 0.0f, 0.0f };
    idleDuration_ = 0.0f;
    idleStillDuration_ = 0.0f;
    idleExpressionBlend_ = 0.0f;
    liquidBlend_ = isLiquidated_ ? 1.0f : 0.0f;
    emitCursor_ = 0;
    pendingEmitCount_ = 0;
    emitAccumulator_ = 0.0f;
    burstEmitCount_ = 0;
    obstacleCount_ = 0;

    CreateResources();
    CreateDescriptors();
    CreateConstantBuffer();
    CreateRootSignature();
    CreatePipelineStates();
    needsReset_ = true;
}

void GpuSphFluid::Reset(const Settings& settings)
{
    assert(IsInitialized());
    uint32_t oldParticleCount = settings_.particleCount;
    settings_ = settings;
    settings_.particleCount = (std::max<uint32_t>)(1, settings_.particleCount);
    settings_.maxObstacleCount = (std::max<uint32_t>)(1, settings_.maxObstacleCount);
    previousCorePosition_ = settings_.corePosition;
    hasPreviousCorePosition_ = false;
    coreVelocity_ = { 0.0f, 0.0f, 0.0f };
    idleDuration_ = 0.0f;
    idleStillDuration_ = 0.0f;
    idleExpressionBlend_ = 0.0f;
    liquidBlend_ = isLiquidated_ ? 1.0f : 0.0f;
    liquidationBurstStrength_ = 0.0f;
    emitCursor_ = 0;
    pendingEmitCount_ = 0;
    emitAccumulator_ = 0.0f;
    burstEmitCount_ = 0;
    obstacleCount_ = 0;

    if (oldParticleCount != settings_.particleCount || !particleResource_) {
        CreateResources();
        CreateDescriptors();
    }
    needsReset_ = true;
}

void GpuSphFluid::SetControlState(
    const Vector3& corePosition,
    const Vector3& targetVelocity,
    const Vector3& coreForward)
{
    settings_.corePosition = corePosition;
    settings_.targetVelocity = targetVelocity;
    settings_.coreForward = NormalizeSafe(coreForward);
}

void GpuSphFluid::SetEmitter(
    bool enabled,
    const Vector3& position,
    const Vector3& velocity,
    float rateScale,
    float lifetimeScale)
{
    emitterEnabled_ = enabled;
    emitterPosition_ = position;
    emitterVelocity_ = velocity;
    emitterRateScale_ = std::clamp(rateScale, 0.0f, 1.0f);
    emitterLifetimeScale_ = (std::max)(lifetimeScale, 0.05f);
}

void GpuSphFluid::TriggerEmitBurst(uint32_t count)
{
    burstEmitCount_ =
        (std::min<uint32_t>)(
            settings_.particleCount,
            burstEmitCount_ + count);
}

void GpuSphFluid::SetObstacles(
    const std::vector<CollisionObstacle>& obstacles)
{
    if (obstacleData_ == nullptr) {
        obstacleCount_ = 0;
        return;
    }

    obstacleCount_ =
        (std::min<uint32_t>)(
            static_cast<uint32_t>(obstacles.size()),
            settings_.maxObstacleCount);
    if (obstacleCount_ > 0) {
        std::memcpy(
            obstacleData_,
            obstacles.data(),
            sizeof(CollisionObstacle) * static_cast<size_t>(obstacleCount_));
    }
}

void GpuSphFluid::Update(float deltaTime)
{
    assert(IsInitialized());
    UpdateFrameState(deltaTime);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        srvManager_->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetComputeRootSignature(rootSignature_.Get());
    commandList->SetComputeRootDescriptorTable(0, particleUavHandleGPU_);
    commandList->SetComputeRootDescriptorTable(1, forceUavHandleGPU_);
    commandList->SetComputeRootConstantBufferView(
        2,
        simulationParameterResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootDescriptorTable(3, obstacleSrvHandleGPU_);

    TransitionResource(
        particleResource_.Get(),
        particleState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    TransitionResource(
        forceResource_.Get(),
        forceState_,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    if (needsReset_) {
        UpdateSimulationParameter(0.0f, false);
        Dispatch(initializePipelineState_.Get());
        InsertUavBarrier(particleResource_.Get());
        needsReset_ = false;
    }

    const uint32_t substeps =
        (std::max<uint32_t>)(1, settings_.simulationSubsteps);
    const float substepDelta = deltaTime / static_cast<float>(substeps);
    for (uint32_t substep = 0; substep < substeps; ++substep) {
        UpdateSimulationParameter(substepDelta, substep == 0);
        Dispatch(densityPressurePipelineState_.Get());
        InsertUavBarrier(particleResource_.Get());
        Dispatch(forcePipelineState_.Get());
        InsertUavBarrier(forceResource_.Get());
        Dispatch(integratePipelineState_.Get());
        InsertUavBarrier(particleResource_.Get());
    }

    TransitionResource(
        particleResource_.Get(),
        particleState_,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GpuSphFluid::Finalize()
{
    if (simulationParameterResource_ && simulationParameterData_ != nullptr) {
        simulationParameterResource_->Unmap(0, nullptr);
        simulationParameterData_ = nullptr;
    }
    if (obstacleResource_ && obstacleData_ != nullptr) {
        obstacleResource_->Unmap(0, nullptr);
        obstacleData_ = nullptr;
    }

    ReleaseDescriptor(particleSrvIndex_);
    ReleaseDescriptor(forceSrvIndex_);
    ReleaseDescriptor(particleUavIndex_);
    ReleaseDescriptor(forceUavIndex_);
    ReleaseDescriptor(obstacleSrvIndex_);

    particleResource_.Reset();
    forceResource_.Reset();
    obstacleResource_.Reset();
    simulationParameterResource_.Reset();
    rootSignature_.Reset();
    initializePipelineState_.Reset();
    densityPressurePipelineState_.Reset();
    forcePipelineState_.Reset();
    integratePipelineState_.Reset();

    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}

void GpuSphFluid::CreateResources()
{
    ID3D12Device* device = dxCommon_->GetDevice();
    assert(device != nullptr);

    const UINT64 particleBufferSize =
        sizeof(Particle) * static_cast<UINT64>(settings_.particleCount);
    const UINT64 forceBufferSize =
        sizeof(Vector4) * static_cast<UINT64>(settings_.particleCount);
    const UINT64 obstacleBufferSize =
        sizeof(CollisionObstacle) * static_cast<UINT64>(settings_.maxObstacleCount);

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC particleDesc {};
    particleDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    particleDesc.Width = particleBufferSize;
    particleDesc.Height = 1;
    particleDesc.DepthOrArraySize = 1;
    particleDesc.MipLevels = 1;
    particleDesc.SampleDesc.Count = 1;
    particleDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    particleDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    HRESULT result = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &particleDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&particleResource_));
    assert(SUCCEEDED(result));
    particleResource_->SetName(L"GpuSphFluid::ParticleBuffer");
    particleState_ = D3D12_RESOURCE_STATE_COMMON;

    D3D12_RESOURCE_DESC forceDesc = particleDesc;
    forceDesc.Width = forceBufferSize;
    result = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &forceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&forceResource_));
    assert(SUCCEEDED(result));
    forceResource_->SetName(L"GpuSphFluid::ForceBuffer");
    forceState_ = D3D12_RESOURCE_STATE_COMMON;

    if (obstacleResource_ && obstacleData_ != nullptr) {
        obstacleResource_->Unmap(0, nullptr);
        obstacleData_ = nullptr;
    }
    obstacleResource_ = dxCommon_->CreateBufferResource(static_cast<size_t>(obstacleBufferSize));
    obstacleResource_->SetName(L"GpuSphFluid::ObstacleBuffer");
    HRESULT mapResult = obstacleResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&obstacleData_));
    assert(SUCCEEDED(mapResult));
    assert(obstacleData_ != nullptr);
    std::memset(
        obstacleData_,
        0,
        sizeof(CollisionObstacle) * static_cast<size_t>(settings_.maxObstacleCount));
}

void GpuSphFluid::CreateDescriptors()
{
    ReleaseDescriptor(particleSrvIndex_);
    ReleaseDescriptor(forceSrvIndex_);
    ReleaseDescriptor(obstacleSrvIndex_);
    ReleaseDescriptor(particleUavIndex_);
    ReleaseDescriptor(forceUavIndex_);

    ID3D12Device* device = dxCommon_->GetDevice();

    particleSrvIndex_ = srvManager_->Allocate();
    particleSrvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(particleSrvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC particleSrvDesc {};
    particleSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    particleSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    particleSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    particleSrvDesc.Buffer.NumElements = settings_.particleCount;
    particleSrvDesc.Buffer.StructureByteStride = sizeof(Particle);
    device->CreateShaderResourceView(
        particleResource_.Get(),
        &particleSrvDesc,
        srvManager_->GetCPUDescriptorHandle(particleSrvIndex_));

    forceSrvIndex_ = srvManager_->Allocate();
    forceSrvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(forceSrvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC forceSrvDesc {};
    forceSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    forceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    forceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    forceSrvDesc.Buffer.NumElements = settings_.particleCount;
    forceSrvDesc.Buffer.StructureByteStride = sizeof(Vector4);
    device->CreateShaderResourceView(
        forceResource_.Get(),
        &forceSrvDesc,
        srvManager_->GetCPUDescriptorHandle(forceSrvIndex_));

    obstacleSrvIndex_ = srvManager_->Allocate();
    obstacleSrvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(obstacleSrvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC obstacleSrvDesc {};
    obstacleSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    obstacleSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    obstacleSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    obstacleSrvDesc.Buffer.NumElements = settings_.maxObstacleCount;
    obstacleSrvDesc.Buffer.StructureByteStride = sizeof(CollisionObstacle);
    device->CreateShaderResourceView(
        obstacleResource_.Get(),
        &obstacleSrvDesc,
        srvManager_->GetCPUDescriptorHandle(obstacleSrvIndex_));

    particleUavIndex_ = srvManager_->Allocate();
    particleUavHandleGPU_ = srvManager_->GetGPUDescriptorHandle(particleUavIndex_);

    D3D12_UNORDERED_ACCESS_VIEW_DESC particleUavDesc {};
    particleUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    particleUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    particleUavDesc.Buffer.NumElements = settings_.particleCount;
    particleUavDesc.Buffer.StructureByteStride = sizeof(Particle);
    device->CreateUnorderedAccessView(
        particleResource_.Get(),
        nullptr,
        &particleUavDesc,
        srvManager_->GetCPUDescriptorHandle(particleUavIndex_));

    forceUavIndex_ = srvManager_->Allocate();
    forceUavHandleGPU_ = srvManager_->GetGPUDescriptorHandle(forceUavIndex_);

    D3D12_UNORDERED_ACCESS_VIEW_DESC forceUavDesc {};
    forceUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    forceUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    forceUavDesc.Buffer.NumElements = settings_.particleCount;
    forceUavDesc.Buffer.StructureByteStride = sizeof(Vector4);
    device->CreateUnorderedAccessView(
        forceResource_.Get(),
        nullptr,
        &forceUavDesc,
        srvManager_->GetCPUDescriptorHandle(forceUavIndex_));
}

void GpuSphFluid::CreateConstantBuffer()
{
    simulationParameterResource_ =
        dxCommon_->CreateBufferResource(sizeof(SimulationParameter));
    simulationParameterResource_->SetName(L"GpuSphFluid::SimulationParameterCB");

    HRESULT result = simulationParameterResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&simulationParameterData_));
    assert(SUCCEEDED(result));
    assert(simulationParameterData_ != nullptr);
}

void GpuSphFluid::CreateRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE descriptorRanges[3] {};
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descriptorRanges[0].NumDescriptors = 1;
    descriptorRanges[0].BaseShaderRegister = 0;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descriptorRanges[1].NumDescriptors = 1;
    descriptorRanges[1].BaseShaderRegister = 1;
    descriptorRanges[1].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[2].NumDescriptors = 1;
    descriptorRanges[2].BaseShaderRegister = 0;
    descriptorRanges[2].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].Descriptor.ShaderRegister = 0;

    rootParameters[3].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &descriptorRanges[2];

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc {};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT result = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    if (FAILED(result)) {
        if (errorBlob) {
            Logger::Log(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    result = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(result));
}

void GpuSphFluid::CreatePipelineStates()
{
    initializePipelineState_ =
        CreateComputePipeline(L"resources/Shaders/Fluid/SlimeFluidInitialize.CS.hlsl");
    densityPressurePipelineState_ =
        CreateComputePipeline(L"resources/Shaders/Fluid/SlimeFluidDensityPressure.CS.hlsl");
    forcePipelineState_ =
        CreateComputePipeline(L"resources/Shaders/Fluid/SlimeFluidForce.CS.hlsl");
    integratePipelineState_ =
        CreateComputePipeline(L"resources/Shaders/Fluid/SlimeFluidIntegrate.CS.hlsl");
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> GpuSphFluid::CreateComputePipeline(
    const std::wstring& shaderPath)
{
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob =
        dxCommon_->LoadCompiledShader(shaderPath);
    assert(shaderBlob != nullptr);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc {};
    pipelineDesc.pRootSignature = rootSignature_.Get();
    pipelineDesc.CS.pShaderBytecode = shaderBlob->GetBufferPointer();
    pipelineDesc.CS.BytecodeLength = shaderBlob->GetBufferSize();

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT result = dxCommon_->GetDevice()->CreateComputePipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(result));

    return pipelineState;
}

void GpuSphFluid::UpdateSimulationParameter(float deltaTime, bool includeEmission)
{
    assert(simulationParameterData_ != nullptr);
    const float liquid = std::clamp(liquidBlend_, 0.0f, 1.0f);

    simulationParameterData_->particleCount = settings_.particleCount;
    simulationParameterData_->deltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 20.0f);
    simulationParameterData_->smoothingRadius = settings_.smoothingRadius;
    simulationParameterData_->particleMass = settings_.particleMass;
    simulationParameterData_->restDensity = settings_.restDensity;
    simulationParameterData_->stiffness =
        Lerp(settings_.stiffness, settings_.stiffness * 0.38f, liquid);
    simulationParameterData_->viscosity =
        Lerp(settings_.viscosity, settings_.liquidViscosity, liquid);
    simulationParameterData_->surfaceTension =
        Lerp(settings_.surfaceTension, settings_.liquidSurfaceTension, liquid);
    simulationParameterData_->gravity = settings_.gravity * Lerp(1.0f, settings_.liquidGravityScale, liquid);
    simulationParameterData_->damping =
        Lerp(settings_.damping, settings_.liquidDamping, liquid);
    simulationParameterData_->boundsMin = settings_.boundsMin;
    simulationParameterData_->particleRadius = settings_.particleRadius;
    simulationParameterData_->boundsMax = settings_.boundsMax;
    simulationParameterData_->boundaryPadding = settings_.boundaryPadding;
    simulationParameterData_->spawnOrigin = settings_.spawnOrigin;
    simulationParameterData_->spawnColumns = (std::max<uint32_t>)(1, settings_.spawnColumns);
    simulationParameterData_->spawnSpacing = settings_.spawnSpacing;
    simulationParameterData_->spawnRows = (std::max<uint32_t>)(1, settings_.spawnRows);
    simulationParameterData_->spawnLayers = (std::max<uint32_t>)(1, settings_.spawnLayers);
    simulationParameterData_->shapeAttraction =
        Lerp(settings_.shapeAttraction, settings_.liquidShapeAttraction, liquid);
    simulationParameterData_->velocityAttraction =
        Lerp(settings_.velocityAttraction, settings_.liquidVelocityAttraction, liquid);
    simulationParameterData_->horizontalFriction =
        Lerp(settings_.horizontalFriction, settings_.liquidHorizontalFriction, liquid);
    simulationParameterData_->corePosition = settings_.corePosition;
    simulationParameterData_->floorHeight = settings_.floorHeight;
    simulationParameterData_->coreForward = NormalizeSafe(settings_.coreForward);
    simulationParameterData_->targetVelocity = settings_.targetVelocity;
    simulationParameterData_->blobRadii = settings_.blobRadii;
    simulationParameterData_->padding0 = 0.0f;
    simulationParameterData_->padding1 = 0.0f;
    simulationParameterData_->padding2 = 0.0f;
    simulationParameterData_->coreDelta = coreVelocity_;
    simulationParameterData_->padding3 = 0.0f;
    simulationParameterData_->wallMinX = settings_.wallMinX;
    simulationParameterData_->wallMaxX = settings_.wallMaxX;
    simulationParameterData_->wallMinY = settings_.wallMinY;
    simulationParameterData_->wallMaxY = settings_.wallMaxY;
    simulationParameterData_->wallMinZ = wallMinZ_;
    simulationParameterData_->wallMaxZ = wallMaxZ_;
    simulationParameterData_->paddingWall0 = 0.0f;
    simulationParameterData_->paddingWall1 = 0.0f;
    simulationParameterData_->liquidationBurstStrength = liquidationBurstStrength_;
    simulationParameterData_->liquidBlend = liquid;
    simulationParameterData_->sloshStrength = settings_.sloshStrength;
    simulationParameterData_->puddleSpread = settings_.puddleSpread;
    simulationParameterData_->emitStartIndex = emitCursor_;
    simulationParameterData_->emitCount = includeEmission ? pendingEmitCount_ : 0;
    simulationParameterData_->obstacleCount = obstacleCount_;
    simulationParameterData_->particleLifetime =
        settings_.particleLifetime * emitterLifetimeScale_;
    simulationParameterData_->emitterPosition = emitterPosition_;
    simulationParameterData_->emitterRadius = settings_.emitterRadius;
    simulationParameterData_->emitterVelocity = emitterVelocity_;
    simulationParameterData_->emitterSpeed = settings_.emitterSpeed;
    simulationParameterData_->collisionFriction = settings_.collisionFriction;
    simulationParameterData_->collisionBounce = settings_.collisionBounce;
    simulationParameterData_->padding4 = 0.0f;
    simulationParameterData_->padding5 = 0.0f;

    if (includeEmission) {
        emitCursor_ = (emitCursor_ + pendingEmitCount_) % settings_.particleCount;
        pendingEmitCount_ = 0;
        liquidationBurstStrength_ = 0.0f;
    }
}

void GpuSphFluid::UpdateFrameState(float deltaTime)
{
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 20.0f);
    const float targetLiquidBlend = isLiquidated_ ? 1.0f : 0.0f;
    const float transitionSpeed =
        isLiquidated_ ? settings_.liquidTransitionSpeed : settings_.gatherTransitionSpeed;
    liquidBlend_ = MoveTowards(
        liquidBlend_,
        targetLiquidBlend,
        transitionSpeed * safeDeltaTime);

    if (!hasPreviousCorePosition_ || safeDeltaTime <= 0.0f) {
        coreVelocity_ = settings_.targetVelocity;
        hasPreviousCorePosition_ = true;
    } else {
        const Vector3 coreDelta = settings_.corePosition - previousCorePosition_;
        coreVelocity_ = {
            coreDelta.x / safeDeltaTime,
            coreDelta.y / safeDeltaTime,
            coreDelta.z / safeDeltaTime
        };
    }
    previousCorePosition_ = settings_.corePosition;

    const float targetSpeedSquared =
        settings_.targetVelocity.x * settings_.targetVelocity.x +
        settings_.targetVelocity.y * settings_.targetVelocity.y +
        settings_.targetVelocity.z * settings_.targetVelocity.z;
    const bool isIdle = !isLiquidated_ && targetSpeedSquared < 0.01f;
    if (isIdle) {
        idleDuration_ = (std::min)(idleDuration_ + safeDeltaTime, 60.0f);
        idleStillDuration_ = (std::min)(idleStillDuration_ + safeDeltaTime, 60.0f);
    } else {
        idleStillDuration_ = 0.0f;
    }

    const float targetIdleExpression = idleStillDuration_ >= 0.75f ? 1.0f : 0.0f;
    const float blendSpeed = targetIdleExpression > idleExpressionBlend_
        ? 2.0f : 3.0f;
    idleExpressionBlend_ = MoveTowards(
        idleExpressionBlend_, targetIdleExpression, safeDeltaTime * blendSpeed);
    // Keep the final gaze phase during the fade-out, then reset it only after
    // the eyes have naturally returned to their normal center position.
    if (!isIdle && idleExpressionBlend_ <= 0.0f) {
        idleDuration_ = 0.0f;
    }

    pendingEmitCount_ = 0;
    if (emitterEnabled_ && safeDeltaTime > 0.0f) {
        emitAccumulator_ +=
            settings_.emitterRate * emitterRateScale_ * safeDeltaTime;
        pendingEmitCount_ =
            (std::min<uint32_t>)(
                static_cast<uint32_t>(emitAccumulator_),
                settings_.particleCount);
        emitAccumulator_ -= static_cast<float>(pendingEmitCount_);
    }
    pendingEmitCount_ =
        (std::min<uint32_t>)(
            settings_.particleCount,
            pendingEmitCount_ + burstEmitCount_);
    burstEmitCount_ = 0;
}

void GpuSphFluid::SetWallBoundaries(float wallMinX, float wallMaxX, float wallMinZ, float wallMaxZ, float wallMinY, float wallMaxY)
{
    settings_.wallMinX = wallMinX;
    settings_.wallMaxX = wallMaxX;
    settings_.wallMinY = wallMinY;
    settings_.wallMaxY = wallMaxY;
    wallMinZ_ = wallMinZ;
    wallMaxZ_ = wallMaxZ;
}

void GpuSphFluid::TriggerLiquidationBurst(float strength)
{
    liquidationBurstStrength_ = (std::max)(liquidationBurstStrength_, strength);
}

void GpuSphFluid::Dispatch(ID3D12PipelineState* pipelineState)
{
    assert(pipelineState != nullptr);
    dxCommon_->GetCommandList()->SetPipelineState(pipelineState);

    const uint32_t groupCount =
        (settings_.particleCount + kThreadGroupSize - 1) / kThreadGroupSize;
    dxCommon_->GetCommandList()->Dispatch(groupCount, 1, 1);
}

void GpuSphFluid::TransitionResource(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES& currentState,
    D3D12_RESOURCE_STATES nextState)
{
    if (resource == nullptr || currentState == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = currentState;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    currentState = nextState;
}

void GpuSphFluid::InsertUavBarrier(ID3D12Resource* resource)
{
    if (resource == nullptr) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
}

void GpuSphFluid::ReleaseDescriptor(uint32_t& descriptorIndex)
{
    if (srvManager_ == nullptr || descriptorIndex == kInvalidDescriptorIndex) {
        return;
    }

    srvManager_->Free(descriptorIndex);
    descriptorIndex = kInvalidDescriptorIndex;
}

std::vector<GpuSphFluid::Particle> GpuSphFluid::GetParticlesCPU() const
{
    std::vector<Particle> result(settings_.particleCount);
    if (!particleResource_ || settings_.particleCount == 0 || dxCommon_ == nullptr) {
        return result;
    }

    ID3D12Device* device = dxCommon_->GetDevice();
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12CommandQueue* commandQueue = dxCommon_->GetCommandQueue();
    ID3D12CommandAllocator* commandAllocator = dxCommon_->GetCommandAllocator();
    if (!device || !commandList || !commandQueue || !commandAllocator) {
        return result;
    }

    const UINT64 bufferSize = sizeof(Particle) * static_cast<UINT64>(settings_.particleCount);

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufferDesc {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = bufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&readbackBuffer));
    if (FAILED(hr)) return result;

    D3D12_RESOURCE_STATES oldState = particleState_;
    const_cast<GpuSphFluid*>(this)->TransitionResource(
        particleResource_.Get(),
        const_cast<GpuSphFluid*>(this)->particleState_,
        D3D12_RESOURCE_STATE_COPY_SOURCE);

    commandList->CopyResource(readbackBuffer.Get(), particleResource_.Get());

    const_cast<GpuSphFluid*>(this)->TransitionResource(
        particleResource_.Get(),
        const_cast<GpuSphFluid*>(this)->particleState_,
        oldState);

    commandList->Close();
    ID3D12CommandList* ppCommandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(1, ppCommandLists);
    dxCommon_->WaitForGPU();

    commandAllocator->Reset();
    commandList->Reset(commandAllocator, nullptr);

    void* mappedData = nullptr;
    D3D12_RANGE readRange{ 0, bufferSize };
    if (SUCCEEDED(readbackBuffer->Map(0, &readRange, &mappedData))) {
        std::memcpy(result.data(), mappedData, bufferSize);
        readbackBuffer->Unmap(0, nullptr);
    }

    return result;
}

void GpuSphFluid::SetParticlesCPU(const std::vector<Particle>& particles)
{
    if (!particleResource_ || particles.empty() || dxCommon_ == nullptr) return;

    ID3D12Device* device = dxCommon_->GetDevice();
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12CommandQueue* commandQueue = dxCommon_->GetCommandQueue();
    ID3D12CommandAllocator* commandAllocator = dxCommon_->GetCommandAllocator();
    if (!device || !commandList || !commandQueue || !commandAllocator) return;

    const UINT64 bufferSize = (std::min)(
        sizeof(Particle) * static_cast<UINT64>(settings_.particleCount),
        sizeof(Particle) * static_cast<UINT64>(particles.size()));

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = bufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));
    if (FAILED(hr)) return;

    void* mappedData = nullptr;
    D3D12_RANGE writeRange{ 0, 0 };
    if (SUCCEEDED(uploadBuffer->Map(0, &writeRange, &mappedData))) {
        std::memcpy(mappedData, particles.data(), bufferSize);
        uploadBuffer->Unmap(0, nullptr);
    }

    D3D12_RESOURCE_STATES oldState = particleState_;
    TransitionResource(
        particleResource_.Get(),
        particleState_,
        D3D12_RESOURCE_STATE_COPY_DEST);

    commandList->CopyBufferRegion(particleResource_.Get(), 0, uploadBuffer.Get(), 0, bufferSize);

    TransitionResource(
        particleResource_.Get(),
        particleState_,
        oldState);

    commandList->Close();
    ID3D12CommandList* ppCommandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(1, ppCommandLists);
    dxCommon_->WaitForGPU();

    commandAllocator->Reset();
    commandList->Reset(commandAllocator, nullptr);

    needsReset_ = false;
}
