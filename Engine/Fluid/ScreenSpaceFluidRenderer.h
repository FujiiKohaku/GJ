#pragma once

#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Math/MathStruct.h"
#include "Engine/SrvManager/SrvManager.h"

#include <cstdint>
#include <wrl.h>

class Camera;
class GpuSphFluid;

class ScreenSpaceFluidRenderer {
public:
    struct Settings {
        float depthThickness = 0.026f;
        int32_t blurRadius = 18;
        float blurSigma = 8.0f;
        float blurDepthSigma = 0.08f;

        Vector3 slimeColor = { 0.03f, 0.68f, 0.32f };
        float refractionStrength = 0.045f;
        float translucency = 0.72f;
        float specularStrength = 1.9f;
        float fresnelStrength = 0.62f;
    };

    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        uint32_t firstRtvIndex = kDefaultFirstRtvIndex);
    void Finalize();
    void SetSettings(const Settings& settings);
    const Settings& GetSettings() const { return settings_; }

    void RenderDepth(const GpuSphFluid& fluid, const Camera& camera);
    void SmoothDepth();
    void Composite(D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);
    void Render(
        const GpuSphFluid& fluid,
        const Camera& camera,
        D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);

    D3D12_GPU_DESCRIPTOR_HANDLE GetDepthSrvHandleGPU() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSmoothedDepthSrvHandleGPU() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetThicknessSrvHandleGPU() const;

private:
    class RenderTarget {
    public:
        ~RenderTarget();

        void Initialize(
            DirectXCommon* dxCommon,
            SrvManager* srvManager,
            uint32_t rtvIndex,
            DXGI_FORMAT format,
            float clearValue);
        void Finalize();
        void BeginRender();
        void EndRender();
        D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const { return srvHandleGPU_; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle() const { return rtvHandle_; }
        const float* GetClearColor() const { return clearColor_; }
        D3D12_VIEWPORT GetViewport() const { return viewport_; }
        D3D12_RECT GetScissorRect() const { return scissorRect_; }
        void Transition(D3D12_RESOURCE_STATES nextState);

    private:
        void CreateResource();
        void CreateViews(uint32_t rtvIndex);

        static constexpr uint32_t kInvalidDescriptorIndex = 0xffffffffu;

        DirectXCommon* dxCommon_ = nullptr;
        SrvManager* srvManager_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_ {};
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_ {};
        uint32_t srvIndex_ = kInvalidDescriptorIndex;
        D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;
        D3D12_VIEWPORT viewport_ {};
        D3D12_RECT scissorRect_ {};
        DXGI_FORMAT format_ = DXGI_FORMAT_R32_FLOAT;
        float clearColor_[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    struct PerViewParameter {
        Matrix4x4 viewProjection;
        Vector3 cameraRight;
        float particleRadius;
        Vector3 cameraUp;
        float depthThickness;
        Vector2 inverseScreenSize;
        uint32_t particleCount;
        float padding;
    };

    struct BlurParameter {
        Vector2 texelSize;
        int32_t direction;
        int32_t radius;
        float sigma;
        float depthSigma;
        float padding0;
        float padding1;
    };

    struct CompositeParameter {
        Vector2 texelSize;
        float refractionStrength;
        float translucency;
        Vector3 slimeColor;
        float specularStrength;
        float fresnelStrength;
        float padding0;
        float padding1;
        float padding2;
    };

    static constexpr uint32_t kDefaultFirstRtvIndex = 8;

    void CreateRootSignatures();
    void CreatePipelineStates();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateDepthPipelineState();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateFullScreenPipelineState(
        ID3D12RootSignature* rootSignature,
        const std::wstring& pixelShaderPath,
        DXGI_FORMAT rtvFormat);
    void CreateConstantBuffers();
    void UpdatePerViewParameter(const GpuSphFluid& fluid, const Camera& camera);
    void UpdateBlurParameter(int32_t direction);
    void UpdateCompositeParameter();
    void DrawFullScreen(
        ID3D12RootSignature* rootSignature,
        ID3D12PipelineState* pipelineState,
        D3D12_GPU_DESCRIPTOR_HANDLE firstTexture,
        D3D12_GPU_DESCRIPTOR_HANDLE secondTexture,
        D3D12_GPU_DESCRIPTOR_HANDLE thirdTexture,
        D3D12_GPU_VIRTUAL_ADDRESS constantBufferView);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Settings settings_ {};

    RenderTarget particleDepthTarget_;
    RenderTarget particleThicknessTarget_;
    RenderTarget blurTargetX_;
    RenderTarget blurTargetY_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> depthRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> fullScreenRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> depthPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> blurPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> blurResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> compositeResource_;
    PerViewParameter* perViewData_ = nullptr;
    BlurParameter* blurData_ = nullptr;
    CompositeParameter* compositeData_ = nullptr;
};
