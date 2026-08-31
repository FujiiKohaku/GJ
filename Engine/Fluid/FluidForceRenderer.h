#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Fluid/GpuSphFluid.h"
#include "Engine/Camera/Camera.h"
#include <wrl.h>

class FluidForceRenderer {
public:
    void Initialize(DirectXCommon* dxCommon);
    void Draw(const GpuSphFluid& fluid, const Camera& camera);

private:
    void CreateRootSignature();
    void CreatePipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
