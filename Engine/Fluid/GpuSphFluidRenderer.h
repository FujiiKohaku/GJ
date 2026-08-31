#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Fluid/GpuSphFluid.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Math/MathStruct.h"
#include <wrl.h>

class GpuSphFluidRenderer {
public:
    void Initialize(DirectXCommon* dxCommon);
    void Draw(const GpuSphFluid& fluid, const Camera& camera);

private:
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateConstantBuffer();

    struct ViewProjection {
        Matrix4x4 view;
        Matrix4x4 projection;
        Matrix4x4 viewProj;
        Matrix4x4 invProjection;
        Matrix4x4 invView;
        Vector3 cameraPos;
        float time;
        Vector3 corePosition;
        float isLiquidated;
        Vector3 blobColor;
        float padColor;
    };

    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    ViewProjection* constantBufferMapped_ = nullptr;
};
