#include "App/SlimePlayer.h"

#include "Engine/Fluid/SlimeCharacterRenderer.h"
#include "Engine/Input/Input.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr Vector3 kInitialPosition = { 0.0f, 0.16f, 0.0f };
constexpr Vector3 kInitialForward = { 0.0f, 0.0f, 1.0f };
constexpr Vector3 kBaseScale = { 0.92f, 0.58f, 0.72f };
constexpr float kMoveSpeed = 2.05f;
constexpr float kVelocityBlend = 8.0f;
constexpr float kBoundsMinX = -1.55f;
constexpr float kBoundsMaxX = 1.55f;
constexpr float kBoundsMinZ = -1.20f;
constexpr float kBoundsMaxZ = 1.20f;

float Saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float SmoothStep(float value)
{
    value = Saturate(value);
    return value * value * (3.0f - 2.0f * value);
}

float Pulse(float normalizedTime, float start, float peak, float end)
{
    if (normalizedTime < start || normalizedTime > end) {
        return 0.0f;
    }
    if (normalizedTime < peak) {
        return SmoothStep((normalizedTime - start) / (peak - start));
    }
    return 1.0f - SmoothStep((normalizedTime - peak) / (end - peak));
}

Vector3 ScaleAdd(const Vector3& a, const Vector3& b, float amount)
{
    return {
        a.x + b.x * amount,
        a.y + b.y * amount,
        a.z + b.z * amount
    };
}
}

void SlimePlayer::Initialize()
{
    Reset();
}

void SlimePlayer::Reset()
{
    position_ = kInitialPosition;
    velocity_ = { 0.0f, 0.0f, 0.0f };
    forward_ = kInitialForward;
    baseScale_ = kBaseScale;
    visualScale_ = kBaseScale;
    action_ = Action::Idle;
    actionTimer_ = 0.0f;
    actionDuration_ = 0.0f;
    actionCooldown_ = 0.0f;
    wobble_ = 0.0f;
    time_ = 0.0f;
}

void SlimePlayer::Update(float deltaTime)
{
    time_ += deltaTime;
    if (actionCooldown_ > 0.0f) {
        actionCooldown_ = (std::max)(0.0f, actionCooldown_ - deltaTime);
    }

    UpdateInput(deltaTime);
    UpdateAction(deltaTime);

    const float speed = Vector3Length(velocity_);
    const float speed01 = Saturate(speed / kMoveSpeed);
    const float idlePulse = std::sin(time_ * 4.2f) * 0.018f;
    const float movementSquash = speed01 * 0.08f;

    Vector3 targetScale = {
        baseScale_.x + movementSquash + idlePulse,
        baseScale_.y - movementSquash * 0.75f - idlePulse * 0.35f,
        baseScale_.z + speed01 * 0.14f
    };
    targetScale = ScaleAdd(targetScale, EvaluateActionScale(), 1.0f);

    const float scaleBlend = Saturate(deltaTime * 12.0f);
    visualScale_ = Lerp(visualScale_, targetScale, scaleBlend);
    const float wobbleBlend = Saturate(deltaTime * 7.0f);
    wobble_ += (speed01 - wobble_) * wobbleBlend;
}

void SlimePlayer::Draw(SlimeCharacterRenderer& renderer) const
{
    renderer.Draw(
        position_,
        visualScale_,
        forward_,
        Vector3Length(velocity_) + wobble_,
        { 0.05f, 0.94f, 0.34f, 0.94f });
}

SlimePlayer::Hitbox SlimePlayer::GetHitbox() const
{
    const float forwardReach =
        (std::max)(0.0f, visualScale_.z - baseScale_.z) * 0.72f;
    Hitbox hitbox;
    hitbox.center = position_ + forward_ * forwardReach;
    hitbox.center.y += visualScale_.y * 0.48f;
    hitbox.size = {
        visualScale_.x * 1.78f,
        visualScale_.y * 1.22f,
        visualScale_.z * 1.82f + forwardReach
    };
    return hitbox;
}

void SlimePlayer::UpdateInput(float deltaTime)
{
    Input* input = Input::GetInstance();
    Vector3 inputDirection = { 0.0f, 0.0f, 0.0f };
    if (input->IsKeyPressed(DIK_W)) {
        inputDirection.z += 1.0f;
    }
    if (input->IsKeyPressed(DIK_S)) {
        inputDirection.z -= 1.0f;
    }
    if (input->IsKeyPressed(DIK_D)) {
        inputDirection.x += 1.0f;
    }
    if (input->IsKeyPressed(DIK_A)) {
        inputDirection.x -= 1.0f;
    }
    if (input->IsGamepadConnected()) {
        inputDirection.x += input->GetGamepadLeftStickX();
        inputDirection.z += input->GetGamepadLeftStickY();
    }

    if (!IsNearlyZero(inputDirection)) {
        inputDirection = Normalize(inputDirection);
        forward_ = inputDirection;
    }

    if (actionCooldown_ <= 0.0f) {
        if (input->IsMouseTrigger(0) ||
            input->IsKeyTrigger(DIK_J) ||
            input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_X)) {
            StartAction(Action::Spike);
        } else if (
            input->IsMouseTrigger(1) ||
            input->IsKeyTrigger(DIK_K) ||
            input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_B)) {
            StartAction(Action::Hammer);
        } else if (
            input->IsKeyTrigger(DIK_SPACE) ||
            input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_A)) {
            StartAction(Action::Bounce);
        }
    }

    const Vector3 targetVelocity = inputDirection * kMoveSpeed;
    const float velocityBlend = Saturate(deltaTime * kVelocityBlend);
    velocity_ = Lerp(velocity_, targetVelocity, velocityBlend);
    position_ += velocity_ * deltaTime;
    position_.x = std::clamp(position_.x, kBoundsMinX, kBoundsMaxX);
    position_.z = std::clamp(position_.z, kBoundsMinZ, kBoundsMaxZ);
}

void SlimePlayer::UpdateAction(float deltaTime)
{
    if (action_ == Action::Idle) {
        return;
    }

    actionTimer_ += deltaTime;
    if (actionTimer_ >= actionDuration_) {
        action_ = Action::Idle;
        actionTimer_ = 0.0f;
        actionDuration_ = 0.0f;
    }
}

void SlimePlayer::StartAction(Action action)
{
    action_ = action;
    actionTimer_ = 0.0f;
    switch (action_) {
    case Action::Bounce:
        actionDuration_ = 0.34f;
        actionCooldown_ = 0.16f;
        break;
    case Action::Spike:
        actionDuration_ = 0.38f;
        actionCooldown_ = 0.46f;
        break;
    case Action::Hammer:
        actionDuration_ = 0.56f;
        actionCooldown_ = 0.64f;
        break;
    case Action::Idle:
    default:
        actionDuration_ = 0.0f;
        actionCooldown_ = 0.0f;
        break;
    }
}

Vector3 SlimePlayer::EvaluateActionScale() const
{
    if (action_ == Action::Idle || actionDuration_ <= 0.0f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    const float t = Saturate(actionTimer_ / actionDuration_);
    switch (action_) {
    case Action::Bounce: {
        const float squash = Pulse(t, 0.00f, 0.20f, 0.48f);
        const float stretch = Pulse(t, 0.24f, 0.52f, 1.00f);
        return {
            squash * 0.25f - stretch * 0.14f,
            -squash * 0.19f + stretch * 0.26f,
            squash * 0.12f - stretch * 0.07f
        };
    }
    case Action::Spike: {
        const float charge = Pulse(t, 0.00f, 0.20f, 0.36f);
        const float thrust = Pulse(t, 0.18f, 0.42f, 0.80f);
        return {
            charge * 0.20f - thrust * 0.52f,
            -charge * 0.16f - thrust * 0.38f,
            -charge * 0.08f + thrust * 1.62f
        };
    }
    case Action::Hammer: {
        const float inflate = Pulse(t, 0.00f, 0.24f, 0.44f);
        const float slam = Pulse(t, 0.28f, 0.54f, 1.00f);
        return {
            inflate * 0.24f + slam * 0.62f,
            inflate * 0.22f - slam * 0.36f,
            inflate * 0.26f + slam * 0.82f
        };
    }
    case Action::Idle:
    default:
        return { 0.0f, 0.0f, 0.0f };
    }
}
