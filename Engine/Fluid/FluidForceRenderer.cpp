#include "FluidForceRenderer.h"
#include <cassert>

void FluidForceRenderer::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    CreateRootSignature();
    CreatePipelineState();
}

void FluidForceRenderer::CreateRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE srvRanges[2] = {};
    // Particle buffer
    srvRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRanges[0].NumDescriptors = 1;
    srvRanges[0].BaseShaderRegister = 0;
    srvRanges[0].OffsetInDescriptorsFromTableStart = 0;

    // Force buffer
    srvRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRanges[1].NumDescriptors = 1;
    srvRanges[1].BaseShaderRegister = 1;
    srvRanges[1].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER parameters[3] = {};
    
    // ViewProjection matrix
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[0].Constants.Num32BitValues = 20; // 16 for Matrix4x4 + 4 for Vector3 + pad
    parameters[0].Constants.ShaderRegister = 0;
    parameters[0].Constants.RegisterSpace = 0;

    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &srvRanges[0];

    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[2].DescriptorTable.NumDescriptorRanges = 1;
    parameters[2].DescriptorTable.pDescriptorRanges = &srvRanges[1];

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

void FluidForceRenderer::CreatePipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    auto vsBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/Fluid/FluidForceDebug.VS.hlsl");
    auto psBlob = dxCommon_->LoadCompiledShader(L"resources/Shaders/Fluid/FluidForceDebug.PS.hlsl");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    
    psoDesc.InputLayout.NumElements = 0;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void FluidForceRenderer::Draw(const GpuSphFluid& fluid, const Camera& camera)
{
    if (!fluid.IsInitialized()) return;
    
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    
    struct VSConstants {
        Matrix4x4 viewProj;
        Vector3 camPos;
        float pad;
    };
    VSConstants constants;
    constants.viewProj = camera.GetViewProjectionMatrix();
    constants.camPos = camera.GetTranslate();
    constants.pad = 0.0f;

    commandList->SetGraphicsRoot32BitConstants(0, 20, &constants, 0);
    commandList->SetGraphicsRootDescriptorTable(1, fluid.GetParticleSrvHandleGPU());
    commandList->SetGraphicsRootDescriptorTable(2, fluid.GetForceSrvHandleGPU());

    commandList->DrawInstanced(6, fluid.GetParticleCount(), 0, 0);
}
