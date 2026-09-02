#include "GoalGimmick.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Logger/Logger.h"

bool GoalGimmick::Initialize(
    const Vector3& position,
    const std::string& modelFile,
    const BaseGimmickParam* gimmickParam)
{
    position_ = position;

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());

    if (!modelFile.empty()) {
        ModelManager::GetInstance()->Load(modelFile);
        object_->SetModel(modelFile);
    } else {
        // デフォルトはプレーン
        Model* model = ModelManager::GetInstance()->CreatePlane();
        if (model) {
            object_->SetModel(model);
        }
    }

    object_->SetTranslate(position_);
    object_->SetScale(size_);
    object_->SetEnableLighting(true);
    object_->Update();

    Logger::Log(std::string("GoalGimmick initialized at: ") +
        std::to_string(position_.x) + ", " + std::to_string(position_.y) + ", " + std::to_string(position_.z));
    return true;
}

void GoalGimmick::Update()
{
    if (object_) {
        object_->Update();
    }
}

void GoalGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

AABB GoalGimmick::GetAABB() const
{
    AABB aabb;
    aabb.center = position_;
    aabb.size = size_;
    return aabb;
}
