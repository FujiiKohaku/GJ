#include "SpringGimmick.h"

#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/LevelEditor/GimmickMetaDataManager.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr const char* kDefaultSpringModel = "Spring/Spring.obj";
constexpr float kCompressionDuration = 0.08f;
constexpr float kReboundDuration = 0.24f;
constexpr float kCompressedScale = 0.62f;
constexpr float kOvershootScale = 1.14f;
constexpr float kLaunchSpeed = 13.5f;
}

bool SpringGimmick::Initialize(
    const Vector3& position,
    const std::string&,
    const BaseGimmickParam*)
{
    position_ = position;
    std::string modelPath = kDefaultSpringModel;
    const GimmickMetaData* metaData =
        GimmickMetaDataManager::GetInstance()->GetMetaData("Spring");
    if (metaData != nullptr && !metaData->defaultModelPath.empty()) {
        modelPath = metaData->defaultModelPath;
    }

    Model* model = ModelManager::GetInstance()->Load(modelPath);
    if (model == nullptr) {
        return false;
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);
    object_->SetColor({ 1.0f, 0.78f, 0.18f, 1.0f });
    object_->SetEnableLighting(true);
    ApplyTransform(1.0f);
    object_->Update();
    return true;
}

void SpringGimmick::Update()
{
    if (!object_) {
        return;
    }

    if (isEditorMode_) {
        ApplyTransform(1.0f);
        object_->Update();
        return;
    }

    float scaleY = 1.0f;
    if (isActivated_) {
        animationTime_ += TimeManager::GetInstance()->GetDeltaTime();
        if (animationTime_ < kCompressionDuration) {
            const float progress =
                std::clamp(animationTime_ / kCompressionDuration, 0.0f, 1.0f);
            scaleY = 1.0f - (1.0f - kCompressedScale) * progress;
        } else {
            if (!hasLaunchedPlayer_ && stage_ != nullptr) {
                for (MapChipPlayer* player : stage_->GetPlayers()) {
                    const auto box = player->GetAABB();
                    // Launch only players standing on this spring.
                    if (std::abs(box.center.x - position_.x) < 0.5f + box.size.x * 0.5f &&
                        std::abs(box.center.y - box.size.y * 0.5f - (position_.y + 0.5f)) < 0.3f) {
                        player->LaunchUpward(kLaunchSpeed);
                    }
                }
                hasLaunchedPlayer_ = true;
            }

            const float reboundProgress = std::clamp(
                (animationTime_ - kCompressionDuration) / kReboundDuration,
                0.0f,
                1.0f);
            if (reboundProgress < 0.45f) {
                const float riseProgress = reboundProgress / 0.45f;
                scaleY = kCompressedScale +
                    (kOvershootScale - kCompressedScale) * riseProgress;
            } else {
                const float settleProgress =
                    (reboundProgress - 0.45f) / 0.55f;
                scaleY = kOvershootScale +
                    (1.0f - kOvershootScale) * settleProgress;
            }

            if (reboundProgress >= 1.0f) {
                isActivated_ = false;
                scaleY = 1.0f;
            }
        }
    }

    ApplyTransform(scaleY);
    object_->Update();
}

void SpringGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

void SpringGimmick::EnableToonLighting()
{
    if (object_) {
        object_->EnableToonLighting();
    }
}

void SpringGimmick::SetEditorMode(bool isEditorMode)
{
    isEditorMode_ = isEditorMode;
    if (isEditorMode_) {
        isActivated_ = false;
        hasLaunchedPlayer_ = false;
        animationTime_ = 0.0f;
    }
}

void SpringGimmick::SetStage(MapChipStage* stage)
{
    stage_ = stage;
}

AABB SpringGimmick::GetAABB() const
{
    return { position_, { 1.0f, 1.0f, 1.0f } };
}

void SpringGimmick::OnPlayerStepped()
{
    if (isEditorMode_ || isActivated_) {
        return;
    }
    isActivated_ = true;
    hasLaunchedPlayer_ = false;
    animationTime_ = 0.0f;
}

void SpringGimmick::ApplyTransform(float scaleY)
{
    const float centerOffsetY = -(1.0f - scaleY) * 0.5f;
    object_->SetScale({ 1.0f, scaleY, 1.0f });
    object_->SetTranslate(position_ + Vector3{ 0.0f, centerOffsetY, 0.0f });
}
