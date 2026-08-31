#pragma once

#include "Engine/3D/Object3d.h"
#include "Engine/Math/MathStruct.h"
#include <memory>

class MapChipField;
class Model;
class MapChipPlayer {
public:
    ~MapChipPlayer();

    void Initialize(Model* model, const MapChipField* mapChipField);
    void Update();
    void Draw();

    const Vector3& GetPosition() const;
    bool IsGrounded() const;
    bool IsColliding() const;

private:
    void MoveHorizontal(float deltaTime);
    void MoveVertical(float deltaTime);
    bool ResolveHorizontalCollision(Vector3& nextPosition);
    bool ResolveVerticalCollision(Vector3& nextPosition);

    std::unique_ptr<Object3d> object_;
    const MapChipField* mapChipField_ = nullptr;
    Vector3 position_ = { 2.0f, 3.0f, -0.1f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    bool isGrounded_ = false;
    bool isColliding_ = false;
};
