#include "CheckpointGimmick.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Audio/SoundManager.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>

namespace {
constexpr float kFlagStartX = 0.17f;
constexpr float kFlagCenterY = 0.18f;
constexpr float kFlagSegmentWidth = 1.0f / 12.0f;
constexpr float kFlagHeight = 1.0f;
constexpr float kFlagWaveSpeed = 5.4f;
constexpr float kFlagPhaseStep = 0.58f;
constexpr const char* kCheckpointSoundName = "Gimmick.Checkpoint.Activate";
constexpr const char* kCheckpointSoundPath = "resources/Audio/Scene/checkpoint_activate.wav";
}

bool CheckpointGimmick::Initialize(const Vector3& position, const std::string&, const BaseGimmickParam*)
{
    position_ = position;
    Model* standModel = ModelManager::GetInstance()->Load("Checkpoint/CheckpointStand.obj");
    Model* segmentModel = ModelManager::GetInstance()->Load("Checkpoint/CheckpointFlagSegment.obj");
    if (!standModel || !segmentModel) {
        return false;
    }
    SoundManager::GetInstance()->Load(
        kCheckpointSoundName, kCheckpointSoundPath, AudioCategory::SE);

    standObject_ = std::make_unique<Object3d>();
    standObject_->Initialize(Object3dManager::GetInstance());
    standObject_->SetModel(standModel);
    standObject_->SetTranslate(position_);
    standObject_->SetEnableLighting(true);
    standObject_->EnableToonLighting();
    standObject_->SetColor({ 0.24f, 0.30f, 0.38f, 1.0f });
    standObject_->Update();

    for (size_t index = 0; index < kFlagSegmentCount; ++index) {
        flagObjects_[index] = std::make_unique<Object3d>();
        flagObjects_[index]->Initialize(Object3dManager::GetInstance());
        flagObjects_[index]->SetModel(segmentModel);
        flagObjects_[index]->SetScale({ kFlagSegmentWidth + 0.012f, kFlagHeight, 1.0f });
        flagObjects_[index]->SetEnableLighting(true);
        flagObjects_[index]->EnableToonLighting();
        flagObjects_[index]->SetColor({ 0.15f, 0.72f, 1.0f, 1.0f });
        flagObjects_[index]->Update();
    }
    return true;
}

void CheckpointGimmick::Update()
{
    if (!standObject_) {
        return;
    }

    time_ += TimeManager::GetInstance()->GetDeltaTime();
    Vector4 flagColor = { 0.15f, 0.72f, 1.0f, 1.0f };
    if (isActivated_) {
        flagColor = { 1.0f, 0.72f, 0.12f, 1.0f };
    }

    for (size_t index = 0; index < kFlagSegmentCount; ++index) {
        const float segmentIndex = static_cast<float>(index);
        const float distanceRatio = segmentIndex / static_cast<float>(kFlagSegmentCount - 1);
        const float phase = time_ * kFlagWaveSpeed - segmentIndex * kFlagPhaseStep;
        const float waveStrength = 0.025f + distanceRatio * 0.11f;
        const float yOffset = std::sin(phase) * waveStrength;
        const float zOffset = std::sin(phase - 0.8f) * waveStrength * 1.35f;
        const float rotateZ = std::cos(phase) * (0.035f + distanceRatio * 0.12f);
        const float rotateY = std::cos(phase - 0.8f) * (0.08f + distanceRatio * 0.28f);

        Vector3 segmentPosition = position_;
        segmentPosition.x += kFlagStartX + kFlagSegmentWidth * (segmentIndex + 0.5f);
        segmentPosition.y += kFlagCenterY + yOffset;
        segmentPosition.z += zOffset;
        flagObjects_[index]->SetTranslate(segmentPosition);
        flagObjects_[index]->SetRotate({ 0.0f, rotateY, rotateZ });
        flagObjects_[index]->SetColor(flagColor);
        flagObjects_[index]->Update();
    }

    standObject_->Update();
}

void CheckpointGimmick::Draw()
{
    if (standObject_) {
        standObject_->Draw();
    }
    for (const std::unique_ptr<Object3d>& flagObject : flagObjects_) {
        if (flagObject) {
            flagObject->Draw();
        }
    }
}

void CheckpointGimmick::EnableToonLighting()
{
    if (standObject_) {
        standObject_->EnableToonLighting();
    }
    for (const std::unique_ptr<Object3d>& flagObject : flagObjects_) {
        if (flagObject) {
            flagObject->EnableToonLighting();
        }
    }
}

AABB CheckpointGimmick::GetAABB() const
{
    return { position_, size_ };
}

bool CheckpointGimmick::TryActivateCheckpoint(const AABB& playerAABB)
{
    if (isActivated_ || !CollisionManager::Intersect(playerAABB, GetAABB()).isHit) return false;
    isActivated_ = true;
    SoundManager::GetInstance()->PlaySE(kCheckpointSoundName, 0.65f);
    return true;
}

std::shared_ptr<IGimmickState> CheckpointGimmick::CreateSnapshot() const
{
    auto state = std::make_shared<CheckpointState>();
    state->isActivated = isActivated_;
    return state;
}

void CheckpointGimmick::RestoreFromSnapshot(const IGimmickState* state)
{
    if (const auto* checkpointState =
            dynamic_cast<const CheckpointState*>(state)) {
        isActivated_ = checkpointState->isActivated;
    }
}
