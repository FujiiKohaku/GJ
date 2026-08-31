#include "GpuSphFluidRenderer.h"
#include <cassert>

void GpuSphFluidRenderer::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateRootSignature();
    CreatePipelineState();
    CreateConstantBuffer();
}

void GpuSphFluidRenderer::CreateRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE srvRanges[1] = {};
    // Particle buffer
    srvRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRanges[0].NumDescriptors = 1;
    srvRanges[0].BaseShaderRegister = 0;
    srvRanges[0].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER parameters[2] = {};
    
    // ViewProjection CBV
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[0].Descriptor.RegisterSpace = 0;

    // Particle SRV Table
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &srvRanges[0];

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(parameters);
    desc.pParameters = parameters;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    assert(SUCCEEDED(hr));
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void GpuSphFluidRenderer::CreatePipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    auto vsBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/Fluid/SlimeFluidSphere.VS.hlsl");
    auto psBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/Fluid/SlimeFluidSphere.PS.hlsl");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    
    // Alpha blending for fluid rendering
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    
    psoDesc.InputLayout.NumElements = 0;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void GpuSphFluidRenderer::CreateConstantBuffer()
{
    constantBuffer_ = dxCommon_->CreateBufferResource(sizeof(ViewProjection));
    HRESULT hr = constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferMapped_));
    assert(SUCCEEDED(hr));
}

void GpuSphFluidRenderer::Draw(const GpuSphFluid& fluid, const Camera& camera)
{
    if (!fluid.IsInitialized()) return;
    
    // Update constant buffer
    const Matrix4x4& view = camera.GetViewMatrix();
    const Matrix4x4& proj = camera.GetProjectionMatrix();
    
    constantBufferMapped_->view = view;
    constantBufferMapped_->projection = proj;
    constantBufferMapped_->viewProj = camera.GetViewProjectionMatrix();
    constantBufferMapped_->invProjection = MatrixMath::Inverse(proj);
    constantBufferMapped_->invView = MatrixMath::Inverse(view);
    constantBufferMapped_->cameraPos = camera.GetTranslate();
    static float timer = 0.0f;
    timer += 0.016f; // rough approximation of delta time for visual effects
    constantBufferMapped_->time = timer;
    constantBufferMapped_->corePosition = {0.0f, 0.0f, 0.0f}; // Or use a core pos if needed
    constantBufferMapped_->isLiquidated = 0.0f;
    constantBufferMapped_->blobColor = {0.03f, 0.68f, 0.32f};
    
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(1, fluid.GetParticleSrvHandleGPU());

    commandList->DrawInstanced(4, fluid.GetParticleCount(), 0, 0);
}
