#include "MovingBlockGimmick.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>

bool MovingBlockGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const LevelData::ObjectData::GimmickData* gimmickData)
{
    basePosition_ = position;
    
    if (gimmickData) {
        speed_ = gimmickData->speed;
        range_ = gimmickData->range;
        axis_ = gimmickData->axis;
    }
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

    if (!isEditorMode_) {
        elapsedTime_ += TimeManager::GetInstance()->GetDeltaTime();
    }
    Vector3 position = basePosition_;
    
    float wave = (1.0f - std::cos(elapsedTime_ * speed_)) * 0.5f;
    // 1ブロックの実際のワールドサイズ（現状は1.0f）
    float kBlockSize = 1.0f;
    // UI上はマス数で設定し、実際の距離に変換する
    float distance = range_.x * kBlockSize;
    
    position.x += axis_.x * wave * distance;
    position.y += axis_.y * wave * distance;
    position.z += axis_.z * wave * distance;
    
    object_->SetTranslate(position);
    object_->Update();
}

void MovingBlockGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}
