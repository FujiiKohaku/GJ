#include "Engine/Fluid/ScreenSpaceFluidRenderer.h"

#include "Engine/Camera/Camera.h"
#include "Engine/Fluid/GpuSphFluid.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Winapp/WinApp.h"

#include <algorithm>
#include <cassert>

void ScreenSpaceFluidRenderer::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    uint32_t firstRtvIndex)
{
    assert(dxCommon != nullptr);
    assert(srvManager != nullptr);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    particleDepthTarget_.Initialize(
        dxCommon_,
        srvManager_,
        firstRtvIndex,
        DXGI_FORMAT_R32_FLOAT,
        1.0f);
    particleThicknessTarget_.Initialize(
        dxCommon_,
        srvManager_,
        firstRtvIndex + 1,
        DXGI_FORMAT_R32_FLOAT,
        0.0f); // Clear value for thickness is 0.0f
    blurTargetX_.Initialize(
        dxCommon_,
        srvManager_,
        firstRtvIndex + 2,
        DXGI_FORMAT_R32_FLOAT,
        1.0f);
    blurTargetY_.Initialize(
        dxCommon_,
        srvManager_,
        firstRtvIndex + 3,
        DXGI_FORMAT_R32_FLOAT,
        1.0f);

    CreateRootSignatures();
    CreatePipelineStates();
    CreateConstantBuffers();
}

void ScreenSpaceFluidRenderer::Finalize()
{
    if (perViewResource_ && perViewData_ != nullptr) {
        perViewResource_->Unmap(0, nullptr);
        perViewData_ = nullptr;
    }
    if (blurResource_ && blurData_ != nullptr) {
        blurResource_->Unmap(0, nullptr);
        blurData_ = nullptr;
    }
    if (compositeResource_ && compositeData_ != nullptr) {
        compositeResource_->Unmap(0, nullptr);
        compositeData_ = nullptr;
    }

    particleDepthTarget_.Finalize();
    particleThicknessTarget_.Finalize();
    blurTargetX_.Finalize();
    blurTargetY_.Finalize();

    depthRootSignature_.Reset();
    fullScreenRootSignature_.Reset();
    depthPipelineState_.Reset();
    blurPipelineState_.Reset();
    compositePipelineState_.Reset();
    perViewResource_.Reset();
    blurResource_.Reset();
    compositeResource_.Reset();

    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}

void ScreenSpaceFluidRenderer::SetSettings(const Settings& settings)
{
    settings_ = settings;
}

void ScreenSpaceFluidRenderer::RenderDepth(
    const GpuSphFluid& fluid,
    const Camera& camera)
{
    assert(dxCommon_ != nullptr);
    if (fluid.GetParticleCount() == 0) {
        return;
    }

    UpdatePerViewParameter(fluid, camera);

    particleDepthTarget_.Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
    particleThicknessTarget_.Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {
        particleDepthTarget_.GetRtvHandle(),
        particleThicknessTarget_.GetRtvHandle()
    };

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    
    // Clear
    commandList->ClearRenderTargetView(rtvHandles[0], particleDepthTarget_.GetClearColor(), 0, nullptr);
    commandList->ClearRenderTargetView(rtvHandles[1], particleThicknessTarget_.GetClearColor(), 0, nullptr);

    commandList->OMSetRenderTargets(2, rtvHandles, FALSE, nullptr);

    D3D12_VIEWPORT viewport = particleDepthTarget_.GetViewport();
    D3D12_RECT scissorRect = particleDepthTarget_.GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        srvManager_->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(depthRootSignature_.Get());
    commandList->SetPipelineState(depthPipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(
        0,
        fluid.GetParticleSrvHandleGPU());
    commandList->SetGraphicsRootConstantBufferView(
        1,
        perViewResource_->GetGPUVirtualAddress());
    commandList->DrawInstanced(6, fluid.GetParticleCount(), 0, 0);

    particleDepthTarget_.Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    particleThicknessTarget_.Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ScreenSpaceFluidRenderer::SmoothDepth()
{
    UpdateBlurParameter(0);
    blurTargetX_.BeginRender();
    DrawFullScreen(
        fullScreenRootSignature_.Get(),
        blurPipelineState_.Get(),
        particleDepthTarget_.GetSrvHandleGPU(),
        particleDepthTarget_.GetSrvHandleGPU(),
        {},
        blurResource_->GetGPUVirtualAddress());
    blurTargetX_.EndRender();

    UpdateBlurParameter(1);
    blurTargetY_.BeginRender();
    DrawFullScreen(
        fullScreenRootSignature_.Get(),
        blurPipelineState_.Get(),
        blurTargetX_.GetSrvHandleGPU(),
        blurTargetX_.GetSrvHandleGPU(),
        {},
        blurResource_->GetGPUVirtualAddress());
    blurTargetY_.EndRender();
}

void ScreenSpaceFluidRenderer::Composite(
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    UpdateCompositeParameter();
    DrawFullScreen(
        fullScreenRootSignature_.Get(),
        compositePipelineState_.Get(),
        sceneColorHandle,
        blurTargetY_.GetSrvHandleGPU(),
        particleThicknessTarget_.GetSrvHandleGPU(), // third texture
        compositeResource_->GetGPUVirtualAddress());
}

void ScreenSpaceFluidRenderer::Render(
    const GpuSphFluid& fluid,
    const Camera& camera,
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    RenderDepth(fluid, camera);
    SmoothDepth();
    Composite(sceneColorHandle);
}

D3D12_GPU_DESCRIPTOR_HANDLE ScreenSpaceFluidRenderer::GetDepthSrvHandleGPU() const
{
    return particleDepthTarget_.GetSrvHandleGPU();
}

D3D12_GPU_DESCRIPTOR_HANDLE ScreenSpaceFluidRenderer::GetSmoothedDepthSrvHandleGPU() const
{
    return blurTargetY_.GetSrvHandleGPU();
}

D3D12_GPU_DESCRIPTOR_HANDLE ScreenSpaceFluidRenderer::GetThicknessSrvHandleGPU() const
{
    return particleThicknessTarget_.GetSrvHandleGPU();
}

void ScreenSpaceFluidRenderer::CreateRootSignatures()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE particleRange {};
    particleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleRange.NumDescriptors = 1;
    particleRange.BaseShaderRegister = 0;
    particleRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER depthRootParameters[2] {};
    depthRootParameters[0].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    depthRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    depthRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    depthRootParameters[0].DescriptorTable.pDescriptorRanges = &particleRange;

    depthRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    depthRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    depthRootParameters[1].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC depthRootSignatureDesc {};
    depthRootSignatureDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    depthRootSignatureDesc.NumParameters = _countof(depthRootParameters);
    depthRootSignatureDesc.pParameters = depthRootParameters;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT result = D3D12SerializeRootSignature(
        &depthRootSignatureDesc,
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
        IID_PPV_ARGS(&depthRootSignature_));
    assert(SUCCEEDED(result));

    D3D12_DESCRIPTOR_RANGE textureRanges[3] {};
    textureRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRanges[0].NumDescriptors = 1;
    textureRanges[0].BaseShaderRegister = 0;
    textureRanges[0].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    textureRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRanges[1].NumDescriptors = 1;
    textureRanges[1].BaseShaderRegister = 1;
    textureRanges[1].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    textureRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRanges[2].NumDescriptors = 1;
    textureRanges[2].BaseShaderRegister = 2;
    textureRanges[2].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER fullScreenRootParameters[4] {};
    fullScreenRootParameters[0].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    fullScreenRootParameters[0].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    fullScreenRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    fullScreenRootParameters[0].DescriptorTable.pDescriptorRanges =
        &textureRanges[0];

    fullScreenRootParameters[1].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    fullScreenRootParameters[1].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    fullScreenRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    fullScreenRootParameters[1].DescriptorTable.pDescriptorRanges =
        &textureRanges[1];

    fullScreenRootParameters[2].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    fullScreenRootParameters[2].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    fullScreenRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    fullScreenRootParameters[2].DescriptorTable.pDescriptorRanges =
        &textureRanges[2];

    fullScreenRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    fullScreenRootParameters[3].ShaderVisibility =
        D3D12_SHADER_VISIBILITY_PIXEL;
    fullScreenRootParameters[3].Descriptor.ShaderRegister = 0;

    D3D12_STATIC_SAMPLER_DESC sampler {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC fullScreenRootSignatureDesc {};
    fullScreenRootSignatureDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    fullScreenRootSignatureDesc.NumParameters = _countof(fullScreenRootParameters);
    fullScreenRootSignatureDesc.pParameters = fullScreenRootParameters;
    fullScreenRootSignatureDesc.NumStaticSamplers = 1;
    fullScreenRootSignatureDesc.pStaticSamplers = &sampler;

    signatureBlob.Reset();
    errorBlob.Reset();
    result = D3D12SerializeRootSignature(
        &fullScreenRootSignatureDesc,
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
        IID_PPV_ARGS(&fullScreenRootSignature_));
    assert(SUCCEEDED(result));
}

void ScreenSpaceFluidRenderer::CreatePipelineStates()
{
    depthPipelineState_ = CreateDepthPipelineState();
    blurPipelineState_ = CreateFullScreenPipelineState(
        fullScreenRootSignature_.Get(),
        L"resources/Shaders/Fluid/SlimeFluidBilateralBlur.PS.hlsl",
        DXGI_FORMAT_R32_FLOAT);
    compositePipelineState_ = CreateFullScreenPipelineState(
        fullScreenRootSignature_.Get(),
        L"resources/Shaders/Fluid/SlimeFluidComposite.PS.hlsl",
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
}

Microsoft::WRL::ComPtr<ID3D12PipelineState>
ScreenSpaceFluidRenderer::CreateDepthPipelineState()
{
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->LoadCompiledShader(L"resources/Shaders/Fluid/SlimeFluidDepth.VS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->LoadCompiledShader(L"resources/Shaders/Fluid/SlimeFluidDepth.PS.hlsl");

    D3D12_BLEND_DESC blendDesc {};
    blendDesc.IndependentBlendEnable = TRUE;
    // RT0 (Depth): MIN blending
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_MIN;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MIN;
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    // RT1 (Thickness): ADD blending
    blendDesc.RenderTarget[1].BlendEnable = TRUE;
    blendDesc.RenderTarget[1].SrcBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[1].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[1].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[1].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[1].DestBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[1].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[1].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc {};
    pipelineDesc.pRootSignature = depthRootSignature_.Get();
    pipelineDesc.VS = {
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize()
    };
    pipelineDesc.PS = {
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize()
    };
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;
    pipelineDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineDesc.InputLayout.NumElements = 0;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.NumRenderTargets = 2;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
    pipelineDesc.RTVFormats[1] = DXGI_FORMAT_R32_FLOAT;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(result));
    return pipelineState;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState>
ScreenSpaceFluidRenderer::CreateFullScreenPipelineState(
    ID3D12RootSignature* rootSignature,
    const std::wstring& pixelShaderPath,
    DXGI_FORMAT rtvFormat)
{
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
        dxCommon_->LoadCompiledShader(L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
        dxCommon_->LoadCompiledShader(pixelShaderPath);

    D3D12_BLEND_DESC blendDesc {};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc {};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc {};
    pipelineDesc.pRootSignature = rootSignature;
    pipelineDesc.VS = {
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize()
    };
    pipelineDesc.PS = {
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize()
    };
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;
    pipelineDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineDesc.InputLayout.NumElements = 0;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = rtvFormat;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT result = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&pipelineState));
    assert(SUCCEEDED(result));
    return pipelineState;
}

void ScreenSpaceFluidRenderer::CreateConstantBuffers()
{
    perViewResource_ = dxCommon_->CreateBufferResource(sizeof(PerViewParameter));
    perViewResource_->SetName(L"ScreenSpaceFluidRenderer::PerViewCB");
    HRESULT result = perViewResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&perViewData_));
    assert(SUCCEEDED(result));

    blurResource_ = dxCommon_->CreateBufferResource(sizeof(BlurParameter));
    blurResource_->SetName(L"ScreenSpaceFluidRenderer::BlurCB");
    result = blurResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&blurData_));
    assert(SUCCEEDED(result));

    compositeResource_ =
        dxCommon_->CreateBufferResource(sizeof(CompositeParameter));
    compositeResource_->SetName(L"ScreenSpaceFluidRenderer::CompositeCB");
    result = compositeResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&compositeData_));
    assert(SUCCEEDED(result));
}

void ScreenSpaceFluidRenderer::UpdatePerViewParameter(
    const GpuSphFluid& fluid,
    const Camera& camera)
{
    assert(perViewData_ != nullptr);
    const Matrix4x4& cameraWorld = camera.GetWorldMatrix();

    perViewData_->viewProjection = camera.GetViewProjectionMatrix();
    perViewData_->cameraRight = {
        cameraWorld.m[0][0],
        cameraWorld.m[0][1],
        cameraWorld.m[0][2]
    };
    perViewData_->particleRadius = fluid.GetParticleRadius();
    perViewData_->cameraUp = {
        cameraWorld.m[1][0],
        cameraWorld.m[1][1],
        cameraWorld.m[1][2]
    };
    perViewData_->depthThickness = settings_.depthThickness;
    perViewData_->inverseScreenSize = {
        1.0f / static_cast<float>(WinApp::kClientWidth),
        1.0f / static_cast<float>(WinApp::kClientHeight)
    };
    perViewData_->particleCount = fluid.GetParticleCount();
    perViewData_->padding = 0.0f;
}

void ScreenSpaceFluidRenderer::UpdateBlurParameter(int32_t direction)
{
    assert(blurData_ != nullptr);
    blurData_->texelSize = {
        1.0f / static_cast<float>(WinApp::kClientWidth),
        1.0f / static_cast<float>(WinApp::kClientHeight)
    };
    blurData_->direction = direction;
    blurData_->radius = std::clamp(settings_.blurRadius, 1, 32);
    blurData_->sigma = (std::max)(settings_.blurSigma, 0.01f);
    blurData_->depthSigma = (std::max)(settings_.blurDepthSigma, 0.0001f);
    blurData_->padding0 = 0.0f;
    blurData_->padding1 = 0.0f;
}

void ScreenSpaceFluidRenderer::UpdateCompositeParameter()
{
    assert(compositeData_ != nullptr);
    compositeData_->texelSize = {
        1.0f / static_cast<float>(WinApp::kClientWidth),
        1.0f / static_cast<float>(WinApp::kClientHeight)
    };
    compositeData_->refractionStrength = settings_.refractionStrength;
    compositeData_->translucency = settings_.translucency;
    compositeData_->slimeColor = settings_.slimeColor;
    compositeData_->specularStrength = settings_.specularStrength;
    compositeData_->fresnelStrength = settings_.fresnelStrength;
    compositeData_->padding0 = 0.0f;
    compositeData_->padding1 = 0.0f;
    compositeData_->padding2 = 0.0f;
}

void ScreenSpaceFluidRenderer::DrawFullScreen(
    ID3D12RootSignature* rootSignature,
    ID3D12PipelineState* pipelineState,
    D3D12_GPU_DESCRIPTOR_HANDLE firstTexture,
    D3D12_GPU_DESCRIPTOR_HANDLE secondTexture,
    D3D12_GPU_DESCRIPTOR_HANDLE thirdTexture,
    D3D12_GPU_VIRTUAL_ADDRESS constantBufferView)
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        srvManager_->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->SetGraphicsRootSignature(rootSignature);
    commandList->SetPipelineState(pipelineState);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(0, firstTexture);
    commandList->SetGraphicsRootDescriptorTable(1, secondTexture);
    if (thirdTexture.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(2, thirdTexture);
    } else {
        commandList->SetGraphicsRootDescriptorTable(2, firstTexture);
    }
    commandList->SetGraphicsRootConstantBufferView(3, constantBufferView);
    commandList->DrawInstanced(3, 1, 0, 0);
}

ScreenSpaceFluidRenderer::RenderTarget::~RenderTarget()
{
    Finalize();
}

void ScreenSpaceFluidRenderer::RenderTarget::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    uint32_t rtvIndex,
    DXGI_FORMAT format,
    float clearValue)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    format_ = format;
    clearColor_[0] = clearValue;
    clearColor_[1] = clearValue;
    clearColor_[2] = clearValue;
    clearColor_[3] = clearValue;

    CreateResource();
    CreateViews(rtvIndex);

    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void ScreenSpaceFluidRenderer::RenderTarget::Finalize()
{
    if (srvManager_ != nullptr && srvIndex_ != kInvalidDescriptorIndex) {
        srvManager_->Free(srvIndex_);
        srvIndex_ = kInvalidDescriptorIndex;
    }

    resource_.Reset();
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}

void ScreenSpaceFluidRenderer::RenderTarget::BeginRender()
{
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, nullptr);
    commandList->ClearRenderTargetView(rtvHandle_, clearColor_, 0, nullptr);
}

void ScreenSpaceFluidRenderer::RenderTarget::EndRender()
{
    Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ScreenSpaceFluidRenderer::RenderTarget::CreateResource()
{
    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = WinApp::kClientWidth;
    resourceDesc.Height = WinApp::kClientHeight;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = format_;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue {};
    clearValue.Format = format_;
    clearValue.Color[0] = clearColor_[0];
    clearValue.Color[1] = clearColor_[1];
    clearValue.Color[2] = clearColor_[2];
    clearValue.Color[3] = clearColor_[3];

    HRESULT result = dxCommon_->GetDevice()->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&resource_));
    assert(SUCCEEDED(result));
    currentState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

void ScreenSpaceFluidRenderer::RenderTarget::CreateViews(uint32_t rtvIndex)
{
    ID3D12Device* device = dxCommon_->GetDevice();
    rtvHandle_ = dxCommon_->GetRTVHandle(rtvIndex);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

    srvIndex_ = srvManager_->Allocate();
    srvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(srvIndex_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = format_;
    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(
        resource_.Get(),
        &srvDesc,
        srvManager_->GetCPUDescriptorHandle(srvIndex_));
}

void ScreenSpaceFluidRenderer::RenderTarget::Transition(
    D3D12_RESOURCE_STATES nextState)
{
    if (currentState_ == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = currentState_;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    currentState_ = nextState;
}
