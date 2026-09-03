#pragma once

#include "Engine/Math/MathStruct.h"
#include <vector>

class MapChipField;
class BaseMapChipGimmick;
class MapChipPlayer {
public:
    ~MapChipPlayer();

    void Initialize(const MapChipField* mapChipField, const Vector3& startPosition);
    void Update(const std::vector<BaseMapChipGimmick*>& dynamicGimmicks);

    const Vector3& GetPosition() const;
    const Vector3& GetVelocity() const;
    const Vector3& GetForward() const;
    const Vector3& GetVisualScale() const;
    Vector3 GetFluidCorePosition() const;
    float GetFluidFloorHeight() const;
    float GetFluidCeilingHeight() const;
    void GetWallBoundaries(float& outMinX, float& outMaxX, float& outMaxY, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks) const;
    bool IsGrounded() const;
    bool IsColliding() const;
    bool IsCrushed() const;

private:
    void UpdateVisualShape(float deltaTime);
    void UpdateVerticalConfinement(const std::vector<BaseMapChipGimmick*>& dynamicGimmicks);
    void MoveHorizontal(float deltaTime, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks);
    void MoveVertical(float deltaTime, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks);
    bool ResolveHorizontalCollision(Vector3& nextPosition);
    bool ResolveVerticalCollision(Vector3& nextPosition);
    bool ResolveDynamicCollision(Vector3& nextPosition, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks, bool isHorizontal);

    const MapChipField* mapChipField_ = nullptr;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 forward_ = { 0.0f, 0.0f, 1.0f };
    Vector3 baseScale_ = { 0.5f, 0.5f, 0.5f };
    Vector3 visualScale_ = { 0.5f, 0.5f, 0.5f };
    float time_ = 0.0f;
    float wobble_ = 0.0f;
    float fluidFloorHeight_ = 0.0f;
    float fluidCeilingHeight_ = 1000.0f;
    float verticalCompression01_ = 0.0f;
    float wallSquash_ = 0.0f;      // 壁衝突時の潰れ量
    float landSquash_ = 0.0f;      // 着地時の潰れ量
    float ceilingSquash_ = 0.0f;   // 天井衝突時の潰れ量
    bool isGrounded_ = false;
    bool isColliding_ = false;
    bool isCrushed_ = false;
    bool wasGrounded_ = false;  // 前フレームの接地状態（着地の瞬間を検知）
    
    BaseMapChipGimmick* baseGimmick_ = nullptr;
};
