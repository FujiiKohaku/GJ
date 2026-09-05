#include "GoalGimmick.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Logger/Logger.h"
#include "Engine/LevelEditor/GimmickMetaDataManager.h"

bool GoalGimmick::Initialize(
    const Vector3& position,
    const std::string& modelFile,
    const BaseGimmickParam* gimmickParam)
{
    position_ = position;

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());

    std::string finalModelPath = modelFile;
    if (const auto* metaData = GimmickMetaDataManager::GetInstance()->GetMetaData("Goal")) {
        finalModelPath = metaData->defaultModelPath;
    }

    if (!finalModelPath.empty()) {
        ModelManager::GetInstance()->Load(finalModelPath);
        object_->SetModel(finalModelPath);
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
