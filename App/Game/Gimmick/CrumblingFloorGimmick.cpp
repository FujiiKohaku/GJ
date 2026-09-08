#include "CrumblingFloorGimmick.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int32_t kCrumblingRockMaterialMode = 17;
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr float kShakeDuration = 0.95f;
constexpr float kFallDuration = 1.25f;
constexpr float kPieceWidth = 0.32f;
constexpr float kPieceHeight = 0.48f;
}

bool CrumblingFloorGimmick::Initialize(
    const Vector3& position,
    const std::string&,
    const BaseGimmickParam*)
{
    position_ = position;
    Model* model = ModelManager::GetInstance()->CreateCube(kWhiteTexture);
    if (!model) {
        return false;
    }

    for (size_t index = 0; index < kPieceCount; ++index) {
        pieces_[index] = std::make_unique<Object3d>();
        pieces_[index]->Initialize(Object3dManager::GetInstance());
        pieces_[index]->SetModel(model);
        pieces_[index]->SetScale({ kPieceWidth, kPieceHeight, 1.0f });
        pieces_[index]->SetTranslate(GetPieceBasePosition(index));
        ApplyRockMaterial(index);
        pieces_[index]->Update();
    }
    return true;
}

void CrumblingFloorGimmick::ApplyRockMaterial(size_t index)
{
    if (!pieces_[index]) {
        return;
    }
    pieces_[index]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    Material* material = pieces_[index]->GetMaterial();
    material->enableLighting = kCrumblingRockMaterialMode;
    material->enableEnvironmentMap = 0;
    material->environmentCoefficient = static_cast<float>(index) * 17.31f;
}

Vector3 CrumblingFloorGimmick::GetPieceBasePosition(size_t index) const
{
    const size_t column = index % 3;
    const size_t row = index / 3;
    float xOffset = -0.34f;
    if (column == 1) {
        xOffset = 0.0f;
    } else if (column == 2) {
        xOffset = 0.34f;
    }
    float yOffset = 0.255f;
    if (row == 1) {
        yOffset = -0.255f;
    }
    return position_ + Vector3{ xOffset, yOffset, 0.0f };
}

void CrumblingFloorGimmick::Update()
{
    if (isEditorMode_) {
        for (size_t index = 0; index < kPieceCount; ++index) {
            pieces_[index]->SetTranslate(GetPieceBasePosition(index));
            pieces_[index]->SetRotate({ 0.0f, 0.0f, 0.0f });
            pieces_[index]->Update();
        }
        return;
    }

    if (state_ == State::Idle) {
        for (const std::unique_ptr<Object3d>& piece : pieces_) {
            if (piece) {
                piece->Update();
            }
        }
        return;
    }

    if (state_ == State::Gone) {
        return;
    }

    stateTime_ += TimeManager::GetInstance()->GetDeltaTime();
    if (state_ == State::Shaking) {
        const float progress = std::clamp(stateTime_ / kShakeDuration, 0.0f, 1.0f);
        const float strength = 0.012f + progress * 0.055f;
        for (size_t index = 0; index < kPieceCount; ++index) {
            const float pieceIndex = static_cast<float>(index);
            Vector3 piecePosition = GetPieceBasePosition(index);
            piecePosition.x += std::sin(stateTime_ * 48.0f + pieceIndex * 2.1f) * strength;
            piecePosition.y += std::cos(stateTime_ * 41.0f + pieceIndex * 1.7f) * strength * 0.32f;
            pieces_[index]->SetTranslate(piecePosition);
            pieces_[index]->SetRotate({ 0.0f, 0.0f,
                std::sin(stateTime_ * 35.0f + pieceIndex) * strength * 0.7f });
            pieces_[index]->Update();
        }
        if (stateTime_ >= kShakeDuration) {
            state_ = State::Falling;
            stateTime_ = 0.0f;
        }
        return;
    }

    if (state_ == State::Falling) {
        for (size_t index = 0; index < kPieceCount; ++index) {
            const float pieceIndex = static_cast<float>(index);
            const float column = static_cast<float>(index % 3) - 1.0f;
            float scatterDirection = -1.0f;
            if (index % 2 != 0) {
                scatterDirection = 1.0f;
            }
            Vector3 piecePosition = GetPieceBasePosition(index);
            piecePosition.x += (column * 0.42f + scatterDirection * 0.12f) * stateTime_;
            piecePosition.y -= 2.8f * stateTime_ * stateTime_ + pieceIndex * 0.035f * stateTime_;
            piecePosition.z += scatterDirection * 0.10f * stateTime_;
            pieces_[index]->SetTranslate(piecePosition);
            pieces_[index]->SetRotate({
                scatterDirection * stateTime_ * (0.7f + pieceIndex * 0.08f),
                column * stateTime_ * 0.45f,
                scatterDirection * stateTime_ * (1.2f + pieceIndex * 0.10f) });
            pieces_[index]->Update();
        }
        if (stateTime_ >= kFallDuration) {
            state_ = State::Gone;
        }
    }
}

void CrumblingFloorGimmick::Draw()
{
    if (state_ == State::Gone && !isEditorMode_) {
        return;
    }
    for (const std::unique_ptr<Object3d>& piece : pieces_) {
        if (piece) {
            piece->Draw();
        }
    }
}

void CrumblingFloorGimmick::EnableToonLighting()
{
    for (size_t index = 0; index < kPieceCount; ++index) {
        ApplyRockMaterial(index);
    }
}

void CrumblingFloorGimmick::SetEditorMode(bool isEditorMode)
{
    isEditorMode_ = isEditorMode;
}

AABB CrumblingFloorGimmick::GetAABB() const
{
    return { position_, { 1.0f, 1.0f, 1.0f } };
}

bool CrumblingFloorGimmick::IsSolid() const
{
    if (state_ == State::Falling || state_ == State::Gone) {
        return false;
    }
    return true;
}

std::shared_ptr<IGimmickState> CrumblingFloorGimmick::CreateSnapshot() const
{
    auto state = std::make_shared<CrumblingFloorState>();
    state->state = state_;
    state->stateTime = stateTime_;
    return state;
}

void CrumblingFloorGimmick::RestoreFromSnapshot(const IGimmickState* state)
{
    if (const auto* floorState = dynamic_cast<const CrumblingFloorState*>(state)) {
        state_ = floorState->state;
        stateTime_ = floorState->stateTime;
        
        // Idle または Gone の状態に応じて位置を初期化・非表示化する
        // Fallingの途中の状態の復元までは求められていないため、安全にIdleかGoneにスナップする
        if (state_ == State::Idle || state_ == State::Shaking) {
            state_ = State::Idle;
            stateTime_ = 0.0f;
            for (size_t index = 0; index < kPieceCount; ++index) {
                if (pieces_[index]) {
                    pieces_[index]->SetTranslate(GetPieceBasePosition(index));
                    pieces_[index]->SetScale({ kPieceWidth, kPieceHeight, 1.0f });
                    pieces_[index]->SetRotate({ 0.0f, 0.0f, 0.0f });
                    pieces_[index]->Update();
                }
            }
        } else {
            state_ = State::Gone;
            stateTime_ = 0.0f;
            for (size_t index = 0; index < kPieceCount; ++index) {
                if (pieces_[index]) {
                    pieces_[index]->SetScale({ 0.0f, 0.0f, 0.0f });
                    pieces_[index]->Update();
                }
            }
        }
    }
}

void CrumblingFloorGimmick::OnPlayerStepped()
{
    if (!isEditorMode_ && state_ == State::Idle) {
        state_ = State::Shaking;
        stateTime_ = 0.0f;
    }
}
