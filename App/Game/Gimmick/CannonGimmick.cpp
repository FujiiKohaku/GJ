#include "CannonGimmick.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr const char* kCannonModel = "Cannon/Cannon.obj";
constexpr float kRecoilDuration = 0.24f;
}

bool CannonGimmick::Initialize(
    const Vector3& position,
    const std::string&,
    const BaseGimmickParam*)
{
    position_ = position;
    Model* model = ModelManager::GetInstance()->Load(kCannonModel);
    if (model == nullptr) {
        return false;
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);
    object_->SetColor({0.18f, 0.22f, 0.25f, 1.0f});
    object_->SetEnableLighting(true);
    object_->SetTranslate(position_);
    object_->Update();
    return true;
}

void CannonGimmick::Update()
{
    if (!object_) {
        return;
    }

    float recoil = 0.0f;
    if (!isEditorMode_ && recoilTime_ > 0.0f) {
        recoilTime_ -= TimeManager::GetInstance()->GetDeltaTime();
        const float progress = std::clamp(recoilTime_ / kRecoilDuration, 0.0f, 1.0f);
        recoil = std::sin(progress * 3.14159265f) * 0.12f;
    }
    object_->SetTranslate(position_ + Vector3{-recoil, -recoil * 0.35f, 0.0f});
    object_->Update();
}

void CannonGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

void CannonGimmick::EnableToonLighting()
{
    if (object_) {
        object_->EnableToonLighting();
    }
}

void CannonGimmick::SetEditorMode(bool isEditorMode)
{
    isEditorMode_ = isEditorMode;
    if (isEditorMode_) {
        launchRequested_ = false;
        recoilTime_ = 0.0f;
    }
}

AABB CannonGimmick::GetAABB() const
{
    return {position_, {1.0f, 1.0f, 1.0f}};
}

void CannonGimmick::OnPlayerStepped()
{
    if (isEditorMode_ || launchRequested_) {
        return;
    }
    launchRequested_ = true;
    recoilTime_ = kRecoilDuration;
}

bool CannonGimmick::ConsumeCannonLaunchRequest(Vector3& outPosition)
{
    if (!launchRequested_) {
        return false;
    }
    launchRequested_ = false;
    outPosition = position_;
    return true;
}
