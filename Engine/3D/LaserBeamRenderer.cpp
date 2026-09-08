#include "LaserBeamRenderer.h"

#include "Engine/Camera/Camera.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Math/MatrixMath.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <numbers>
#include <vector>

LaserBeamRenderer* LaserBeamRenderer::GetInstance()
{
    static LaserBeamRenderer instance;
    return &instance;
}

void LaserBeamRenderer::Initialize()
{
    dxCommon_ = DirectXCommon::GetInstance();
    
    // 円柱メッシュの作成
    CreateMesh(32);
    
    CreateRootSignature();
    CreatePipelineState();

    constantResource_ = dxCommon_->CreateBufferResource(sizeof(Constants) * kMaxBeams);
    const HRESULT hr = constantResource_->Map(0, nullptr, reinterpret_cast<void**>(&constants_));
    assert(SUCCEEDED(hr));
    std::memset(constants_, 0, sizeof(Constants) * kMaxBeams);
}

void LaserBeamRenderer::Finalize()
{
    // CComPtr uses automatic cleanup, just null out pointers if needed
    constants_ = nullptr;
}

void LaserBeamRenderer::Update(float deltaTime)
{
    time_ += deltaTime;
}

void LaserBeamRenderer::Draw(const Vector3& startPosition, const Vector3& endPosition, float radius, const LaserBeamRenderParams& params)
{
    if (drawIndex_ >= kMaxBeams || radius <= 0.0f) return;

    Constants& data = constants_[drawIndex_];
    if (camera_) {
        data.viewProjection = camera_->GetViewProjectionMatrix();
        const Vector3 cameraPosition = camera_->GetTranslate();
        data.cameraPositionAndTime = { cameraPosition.x, cameraPosition.y, cameraPosition.z, time_ };
    }
    
    data.color = params.color;
    data.coreColor = params.coreColor;
    data.beamParameters1 = { params.speed, params.intensity, params.noiseScale, params.coreIntensity };
    data.beamParameters2 = { params.coreThreshold, params.spinSpeed, params.twistScale, 0.0f };
    
    data.startPosition = { startPosition.x, startPosition.y, startPosition.z, 1.0f };
    data.endPositionAndRadius = { endPosition.x, endPosition.y, endPosition.z, radius };

    ++drawIndex_;
}

void LaserBeamRenderer::DrawAll()
{
    if (drawIndex_ == 0) return;

    auto* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (uint32_t i = 0; i < drawIndex_; ++i) {
        const D3D12_GPU_VIRTUAL_ADDRESS address = constantResource_->GetGPUVirtualAddress() + sizeof(Constants) * i;
        commandList->SetGraphicsRootConstantBufferView(0, address);
        commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
    }

    drawIndex_ = 0;
}

void LaserBeamRenderer::CreateMesh(uint32_t divisions)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    const float angleStep = 2.0f * std::numbers::pi_v<float> / static_cast<float>(divisions);
    constexpr uint32_t kHeightSegments = 10; // レーザーの分割数はある程度あればOK
    
    for (uint32_t ring = 0; ring <= kHeightSegments; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(kHeightSegments); // 0.0 to 1.0 (Y-axis along cylinder)
        
        for (uint32_t side = 0; side <= divisions; ++side) {
            const float angle = static_cast<float>(side) * angleStep;
            
            // XZ plane forms the circle, Y is the length
            const float x = std::cos(angle);
            const float z = std::sin(angle);
            const float u = static_cast<float>(side) / static_cast<float>(divisions);
            
            // Position forms a cylinder around Y axis, from Y=0 to Y=1
            vertices.push_back({
                { x, v, z, 1.0f },
                { x, 0.0f, z },
                { u, v }
            });
        }
    }
    
    const uint32_t ringStride = divisions + 1;
    for (uint32_t ring = 0; ring < kHeightSegments; ++ring) {
        for (uint32_t side = 0; side < divisions; ++side) {
            const uint32_t i0 = ring * ringStride + side;
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + ringStride;
            const uint32_t i3 = i2 + 1;
            
            indices.insert(indices.end(), { i0, i2, i1, i1, i2, i3 });
        }
    }

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * vertices.size());
    Vertex* mappedVertices = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
    std::memcpy(mappedVertices, vertices.data(), sizeof(Vertex) * vertices.size());
    vertexBufferView_ = { vertexResource_->GetGPUVirtualAddress(), static_cast<UINT>(sizeof(Vertex) * vertices.size()), sizeof(Vertex) };

    indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indices.size());
    uint32_t* mappedIndices = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices));
    std::memcpy(mappedIndices, indices.data(), sizeof(uint32_t) * indices.size());
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexCount_ = static_cast<uint32_t>(indices.size());
}

void LaserBeamRenderer::CreateRootSignature()
{
    D3D12_ROOT_PARAMETER parameter {};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameter.Descriptor.ShaderRegister = 0; // b0
    
    D3D12_ROOT_SIGNATURE_DESC desc {};
    desc.NumParameters = 1;
    desc.pParameters = &parameter;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr) && error) {
        Logger::Log(reinterpret_cast<const char*>(error->GetBufferPointer()));
    }
    assert(SUCCEEDED(hr));
    
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void LaserBeamRenderer::CreatePipelineState()
{
    auto vertexShader = dxCommon_->LoadCompiledShader(L"resources/Shaders/LaserBeam/LaserBeam.VS.hlsl");
    auto pixelShader = dxCommon_->LoadCompiledShader(L"resources/Shaders/LaserBeam/LaserBeam.PS.hlsl");
    assert(vertexShader && pixelShader);

    D3D12_INPUT_ELEMENT_DESC inputs[3] {};
    inputs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputs[1] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputs[2] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

    D3D12_RASTERIZER_DESC rasterizer {};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.DepthClipEnable = TRUE;

    D3D12_BLEND_DESC blend {};
    auto& target = blend.RenderTarget[0];
    target.BlendEnable = TRUE;
    // 半透明合成 (Alpha Blend) で背景色に関わらずレーザーの色をはっきり出す
    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOp = D3D12_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D12_BLEND_ONE;
    target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC depth {};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 半透明描画のためZ書き込みは行わない
    depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc {};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = { inputs, 3 };
    desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    desc.RasterizerState = rasterizer;
    desc.BlendState = blend;
    desc.DepthStencilState = depth;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // エンジンのレンダリングターゲットフォーマットに合わせる
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    
    const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}
