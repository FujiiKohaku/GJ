#include "Engine/Fluid/GpuSphFluid.h"

#include "Engine/Logger/Logger.h"

#include <algorithm>
#include <cassert>

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

void GpuSphFluid::Update(float deltaTime)
{
    assert(IsInitialized());
    UpdateSimulationParameter(deltaTime);

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

    TransitionResource(
        particleResource_.Get(),
        particleState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    TransitionResource(
        forceResource_.Get(),
        forceState_,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    if (needsReset_) {
        Dispatch(initializePipelineState_.Get());
        InsertUavBarrier(particleResource_.Get());
        needsReset_ = false;
    }

    Dispatch(densityPressurePipelineState_.Get());
    InsertUavBarrier(particleResource_.Get());
    Dispatch(forcePipelineState_.Get());
    InsertUavBarrier(forceResource_.Get());
    Dispatch(integratePipelineState_.Get());
    InsertUavBarrier(particleResource_.Get());

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

    ReleaseDescriptor(particleSrvIndex_);
    ReleaseDescriptor(particleUavIndex_);
    ReleaseDescriptor(forceUavIndex_);

    particleResource_.Reset();
    forceResource_.Reset();
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
}

void GpuSphFluid::CreateDescriptors()
{
    ReleaseDescriptor(particleSrvIndex_);
    ReleaseDescriptor(forceSrvIndex_);
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

    D3D12_DESCRIPTOR_RANGE descriptorRanges[2] {};
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

    D3D12_ROOT_PARAMETER rootParameters[3] {};
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

void GpuSphFluid::UpdateSimulationParameter(float deltaTime)
{
    assert(simulationParameterData_ != nullptr);
    simulationParameterData_->particleCount = settings_.particleCount;
    simulationParameterData_->deltaTime = std::clamp(deltaTime, 0.0f, 1.0f / 20.0f);
    simulationParameterData_->smoothingRadius = settings_.smoothingRadius;
    simulationParameterData_->particleMass = settings_.particleMass;
    simulationParameterData_->restDensity = settings_.restDensity;
    simulationParameterData_->stiffness = settings_.stiffness;
    simulationParameterData_->viscosity = settings_.viscosity;
    simulationParameterData_->surfaceTension = settings_.surfaceTension;
    simulationParameterData_->gravity = settings_.gravity;
    simulationParameterData_->damping = settings_.damping;
    simulationParameterData_->boundsMin = settings_.boundsMin;
    simulationParameterData_->particleRadius = settings_.particleRadius;
    simulationParameterData_->boundsMax = settings_.boundsMax;
    simulationParameterData_->boundaryPadding = settings_.boundaryPadding;
    simulationParameterData_->spawnOrigin = settings_.spawnOrigin;
    simulationParameterData_->spawnColumns = (std::max<uint32_t>)(1, settings_.spawnColumns);
    simulationParameterData_->spawnSpacing = settings_.spawnSpacing;
    simulationParameterData_->spawnRows = (std::max<uint32_t>)(1, settings_.spawnRows);
    simulationParameterData_->spawnLayers = (std::max<uint32_t>)(1, settings_.spawnLayers);
    simulationParameterData_->shapeAttraction = isLiquidated_ ? 0.0f : settings_.shapeAttraction;
    simulationParameterData_->velocityAttraction = isLiquidated_ ? 0.0f : settings_.velocityAttraction;
    simulationParameterData_->horizontalFriction = settings_.horizontalFriction;
    simulationParameterData_->corePosition = settings_.corePosition;
    simulationParameterData_->floorHeight = settings_.floorHeight;
    simulationParameterData_->coreForward = NormalizeSafe(settings_.coreForward);
    simulationParameterData_->targetVelocity = settings_.targetVelocity;
    simulationParameterData_->blobRadii = settings_.blobRadii;
    simulationParameterData_->padding0 = 0.0f;
    simulationParameterData_->padding1 = 0.0f;
    simulationParameterData_->padding2 = 0.0f;
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
