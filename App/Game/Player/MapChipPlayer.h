#pragma once

#include "Engine/3D/Object3d.h"
#include "Engine/Math/MathStruct.h"
#include <memory>

#include <vector>

class MapChipField;
class Model;
class BaseMapChipGimmick;
class MapChipPlayer {
public:
    ~MapChipPlayer();

    void Initialize(Model* model, const MapChipField* mapChipField, const Vector3& startPosition);
    void Update(const std::vector<BaseMapChipGimmick*>& dynamicGimmicks);
    void Draw();

    const Vector3& GetPosition() const;
    bool IsGrounded() const;
    bool IsColliding() const;

private:
    void MoveHorizontal(float deltaTime, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks);
    void MoveVertical(float deltaTime, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks);
    bool ResolveHorizontalCollision(Vector3& nextPosition);
    bool ResolveVerticalCollision(Vector3& nextPosition);
    bool ResolveDynamicCollision(Vector3& nextPosition, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks, bool isHorizontal);

    std::unique_ptr<Object3d> object_;
    const MapChipField* mapChipField_ = nullptr;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    bool isGrounded_ = false;
    bool isColliding_ = false;
    
    BaseMapChipGimmick* baseGimmick_ = nullptr;
};
