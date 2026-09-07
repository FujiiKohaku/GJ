#include "CheckpointGimmick.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>

bool CheckpointGimmick::Initialize(const Vector3& position, const std::string&, const BaseGimmickParam*)
{
    position_ = position;
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(ModelManager::GetInstance()->CreateCube("resources/Textures/white.png"));
    object_->SetTranslate(position_);
    object_->SetScale(size_);
    object_->SetEnableLighting(true);
    object_->EnableToonLighting();
    object_->SetColor({ 0.15f, 0.72f, 1.0f, 1.0f });
    object_->Update();
    return true;
}

void CheckpointGimmick::Update()
{
    if (!object_) return;

    time_ += TimeManager::GetInstance()->GetDeltaTime();
    const float pulse = 1.0f + std::sin(time_ * 3.5f) * 0.08f;
    object_->SetScale({ size_.x * pulse, size_.y, size_.z * pulse });
    object_->SetColor(isActivated_
        ? Vector4{ 0.28f, 1.0f, 0.46f, 1.0f }
        : Vector4{ 0.15f, 0.72f, 1.0f, 1.0f });
    object_->Update();
}

void CheckpointGimmick::Draw()
{
    if (object_) object_->Draw();
}

void CheckpointGimmick::EnableToonLighting()
{
    if (object_) object_->EnableToonLighting();
}

AABB CheckpointGimmick::GetAABB() const
{
    return { position_, size_ };
}

bool CheckpointGimmick::TryActivateCheckpoint(const AABB& playerAABB)
{
    if (isActivated_ || !CollisionManager::Intersect(playerAABB, GetAABB()).isHit) return false;
    isActivated_ = true;
    return true;
}
