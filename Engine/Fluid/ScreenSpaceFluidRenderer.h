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
        float depthThickness = 0.055f;
        int32_t blurRadius = 34;
        float blurSigma = 16.0f;
        float blurDepthSigma = 0.12f;

        Vector3 slimeColor = { 0.02f, 0.72f, 0.35f };
        float refractionStrength = 0.082f;
        float translucency = 0.88f;
        float specularStrength = 2.85f;
        float fresnelStrength = 0.92f;
    };

    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        uint32_t firstRtvIndex = kDefaultFirstRtvIndex);
    void Finalize();
    void SetSettings(const Settings& settings);
    const Settings& GetSettings() const { return settings_; }

    void RenderDepth(const std::vector<const GpuSphFluid*>& fluids, const Camera& camera);
    void RenderDepth(const GpuSphFluid& fluid, const Camera& camera);
    void SmoothDepth();
    void Composite(
        const GpuSphFluid& fluid,
        const Camera& camera,
        D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle);
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
        float floorHeightWorld;
        float groundClipEnabled;
        float padding0;
        Matrix4x4 invViewProj;
        Matrix4x4 viewProj;
        Vector3 eyeWorldPosition;
        float eyeHalfWidthPixels;
        float eyeHalfHeightPixels;
        float eyeVisibility;
        Vector2 eyeGazeDirection;
        float deathEyes;
        Vector3 paddingEyes;
        Vector2 eyeCenterUv;
        Vector2 paddingEyeCenter;
        float idleFaceAmount;
        float idleFaceTime;
        Vector2 paddingIdleFace;
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
    void UpdateCompositeParameter(const GpuSphFluid& fluid, const Camera& camera);
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
