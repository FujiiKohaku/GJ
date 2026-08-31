#include "Engine/Fluid/SlimeCharacterRenderer.h"

#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <algorithm>

void SlimeCharacterRenderer::Initialize(Camera* camera)
{
    assert(camera != nullptr);
    camera_ = camera;
    dxCommon_ = DirectXCommon::GetInstance();

    CreateRootSignature();
    CreatePipelineState();

    constantResource_ =
        dxCommon_->CreateBufferResource(sizeof(Constants) * kMaxSlimes);
    const HRESULT hr = constantResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&constants_));
    assert(SUCCEEDED(hr));
    std::memset(constants_, 0, sizeof(Constants) * kMaxSlimes);
}

void SlimeCharacterRenderer::Finalize()
{
    if (constantResource_ && constants_ != nullptr) {
        constantResource_->Unmap(0, nullptr);
        constants_ = nullptr;
    }

    rootSignature_.Reset();
    pipelineState_.Reset();
    constantResource_.Reset();
    dxCommon_ = nullptr;
    camera_ = nullptr;
}

void SlimeCharacterRenderer::Update(float deltaTime)
{
    time_ += deltaTime;
}

void SlimeCharacterRenderer::PreDraw()
{
    drawIndex_ = 0;
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void SlimeCharacterRenderer::Draw(
    const Vector3& position,
    const Vector3& radii,
    const Vector3& forward,
    float speed,
    const Vector4& color)
{
    if (drawIndex_ >= kMaxSlimes || color.w <= 0.0f) {
        return;
    }

    Constants& data = constants_[drawIndex_];
    data.viewProjection = camera_->GetViewProjectionMatrix();
    const Vector3 cameraPosition = camera_->GetTranslate();
    data.cameraPositionAndTime = {
        cameraPosition.x,
        cameraPosition.y,
        cameraPosition.z,
        time_
    };
    data.color = color;
    data.positionAndGround = { position.x, position.y, position.z, 0.0f };
    data.radiiAndWobble = {
        radii.x,
        radii.y,
        radii.z,
        (std::min)(speed / 2.4f, 1.0f)
    };
    const Vector3 normalizedForward = NormalizeSafe(forward);
    data.forwardAndSpeed = {
        normalizedForward.x,
        normalizedForward.y,
        normalizedForward.z,
        speed
    };
    const Matrix4x4& cameraWorld = camera_->GetWorldMatrix();
    data.cameraRightAndRadius = {
        cameraWorld.m[0][0],
        cameraWorld.m[0][1],
        cameraWorld.m[0][2],
        radii.x
    };
    data.cameraUpAndHeight = {
        cameraWorld.m[1][0],
        cameraWorld.m[1][1],
        cameraWorld.m[1][2],
        radii.y
    };

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    const D3D12_GPU_VIRTUAL_ADDRESS address =
        constantResource_->GetGPUVirtualAddress() +
        sizeof(Constants) * drawIndex_;
    commandList->SetGraphicsRootConstantBufferView(0, address);
    commandList->DrawInstanced(6, 1, 0, 0);
    ++drawIndex_;
}

void SlimeCharacterRenderer::CreateRootSignature()
{
    D3D12_ROOT_PARAMETER parameter {};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameter.Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC desc {};
    desc.NumParameters = 1;
    desc.pParameters = &parameter;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &error);
    if (FAILED(hr) && error) {
        Logger::Log(reinterpret_cast<const char*>(error->GetBufferPointer()));
    }
    assert(SUCCEEDED(hr));

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void SlimeCharacterRenderer::CreatePipelineState()
{
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShader =
        dxCommon_->LoadCompiledShader(
            L"resources/Shaders/Fluid/SlimeCharacter.VS.hlsl");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShader =
        dxCommon_->LoadCompiledShader(
            L"resources/Shaders/Fluid/SlimeCharacter.PS.hlsl");
    assert(vertexShader && pixelShader);

    D3D12_RASTERIZER_DESC rasterizer {};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.DepthClipEnable = TRUE;

    D3D12_BLEND_DESC blend {};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depth {};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc {};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = { nullptr, 0 };
    desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    desc.RasterizerState = rasterizer;
    desc.BlendState = blend;
    desc.DepthStencilState = depth;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;

    const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}
