#pragma once

#include "Engine/Math/MathStruct.h"
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class Camera;
class DirectXCommon;

struct LaserBeamRenderParams {
    Vector4 color = { 0.8f, 0.0f, 1.0f, 1.0f };
    Vector4 coreColor = { 0.0f, 1.0f, 1.0f, 1.0f };
    float speed = 3.0f;
    float intensity = 6.0f;
    float noiseScale = 1.0f;
    float coreIntensity = 40.0f;
    float coreThreshold = 1.0f;
    float spinSpeed = 0.5f;
    float twistScale = 1.0f;
};

class LaserBeamRenderer {
public:
    static LaserBeamRenderer* GetInstance();

    void Initialize();
    void Finalize();

    void SetCamera(Camera* camera) { camera_ = camera; }

    void Update(float deltaTime);
    
    // パラメータをキューに登録する
    void Draw(const Vector3& startPosition, const Vector3& endPosition, float radius, const LaserBeamRenderParams& params);

    // 登録されたすべてのレーザーを一括描画する
    void DrawAll();

private:
    LaserBeamRenderer() = default;
    ~LaserBeamRenderer() = default;
    LaserBeamRenderer(const LaserBeamRenderer&) = delete;
    LaserBeamRenderer& operator=(const LaserBeamRenderer&) = delete;

    struct Vertex {
        Vector4 position;
        Vector3 normal;
        Vector2 texcoord;
    };

    struct Constants {
        Matrix4x4 viewProjection;
        Vector4 cameraPositionAndTime;
        Vector4 color;
        Vector4 coreColor;
        Vector4 beamParameters1; // x: speed, y: intensity, z: noiseScale, w: coreIntensity
        Vector4 beamParameters2; // x: coreThreshold, y: spinSpeed, z: twistScale, w: (pad)
        Vector4 startPosition;
        Vector4 endPositionAndRadius;
        std::array<float, 20> padding {}; // Ensure 256-byte alignment
    };
    static_assert(sizeof(Constants) == 256);

    void CreateMesh(uint32_t divisions);
    void CreateRootSignature();
    void CreatePipelineState();

    static constexpr uint32_t kMaxBeams = 64;
    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_ {};
    Constants* constants_ = nullptr;
    uint32_t indexCount_ = 0;
    uint32_t drawIndex_ = 0;
    float time_ = 0.0f;
};
