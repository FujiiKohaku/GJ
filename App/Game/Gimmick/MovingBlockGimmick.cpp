#include "MovingBlockGimmick.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>

namespace {
constexpr float kMoveRange = 1.5f;
constexpr float kMoveSpeed = 2.0f;
}

bool MovingBlockGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath)
{
    basePosition_ = position;
    Model* model =
        ModelManager::GetInstance()->CreateCube(texturePath);
    if (model == nullptr) {
        return false;
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);
    object_->SetTranslate(basePosition_);
    object_->SetColor({ 1.0f, 0.45f, 0.1f, 1.0f });
    object_->SetEnableLighting(true);
    object_->Update();
    return true;
}

void MovingBlockGimmick::Update()
{
    if (!object_) {
        return;
    }

    elapsedTime_ += TimeManager::GetInstance()->GetDeltaTime();
    Vector3 position = basePosition_;
    position.y += std::sin(elapsedTime_ * kMoveSpeed) * kMoveRange;
    object_->SetTranslate(position);
    object_->Update();
}

void MovingBlockGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}
