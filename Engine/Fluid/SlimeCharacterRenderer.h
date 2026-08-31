#pragma once

#include "Engine/Math/MathStruct.h"

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class Camera;
class DirectXCommon;

class SlimeCharacterRenderer {
public:
    void Initialize(Camera* camera);
    void Finalize();
    void Update(float deltaTime);
    void PreDraw();
    void Draw(
        const Vector3& position,
        const Vector3& radii,
        const Vector3& forward,
        float speed,
        const Vector4& color);

private:
    struct Constants {
        Matrix4x4 viewProjection;
        Vector4 cameraPositionAndTime;
        Vector4 color;
        Vector4 positionAndGround;
        Vector4 radiiAndWobble;
        Vector4 forwardAndSpeed;
        Vector4 cameraRightAndRadius;
        Vector4 cameraUpAndHeight;
        std::array<float, 20> padding {};
    };
    static_assert(sizeof(Constants) == 256);

    void CreateRootSignature();
    void CreatePipelineState();

    static constexpr uint32_t kMaxSlimes = 8;

    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantResource_;
    Constants* constants_ = nullptr;
    uint32_t drawIndex_ = 0;
    float time_ = 0.0f;
};
