#pragma once

#include "Engine/Math/MathStruct.h"

class SlimeCharacterRenderer;

class SlimePlayer {
public:
    struct Hitbox {
        Vector3 center;
        Vector3 size;
    };

    void Initialize();
    void Reset();
    void Update(float deltaTime);
    void Draw(SlimeCharacterRenderer& renderer) const;

    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetForward() const { return forward_; }
    const Vector3& GetVisualScale() const { return visualScale_; }
    Hitbox GetHitbox() const;

private:
    enum class Action {
        Idle,
        Bounce,
        Spike,
        Hammer
    };

    void UpdateInput(float deltaTime);
    void UpdateAction(float deltaTime);
    void StartAction(Action action);
    Vector3 EvaluateActionScale() const;

    Vector3 position_ = { 0.0f, 0.16f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 forward_ = { 0.0f, 0.0f, 1.0f };
    Vector3 baseScale_ = { 0.92f, 0.58f, 0.72f };
    Vector3 visualScale_ = { 0.92f, 0.58f, 0.72f };
    Action action_ = Action::Idle;
    float actionTimer_ = 0.0f;
    float actionDuration_ = 0.0f;
    float actionCooldown_ = 0.0f;
    float wobble_ = 0.0f;
    float time_ = 0.0f;
};
