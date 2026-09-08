#include "MovingBlockGimmick.h"
#include "MovingBlockParam.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>
#include <numbers>
#include <algorithm>
#include "App/Game/Map/MapChipStage.h"
#include "Engine/CollisionManager/CollisionManager.h"
namespace {
constexpr int32_t kMovingBlockWoodMaterialMode = 14;
}

bool MovingBlockGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    basePosition_ = position;
    
    if (gimmickParam) {
        const MovingBlockParam* param = dynamic_cast<const MovingBlockParam*>(gimmickParam);
        if (param) {
            speed_ = param->speed_;
            range_ = param->range_;
            axis_ = param->axis_;
        }
    }
    
    currentPosition_ = basePosition_;
    previousPosition_ = basePosition_;

    Model* model =
        ModelManager::GetInstance()->CreateCube(texturePath);
    if (model == nullptr) {
        return false;
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);
    object_->SetTranslate(basePosition_);
    ApplyWoodMaterial();
    object_->Update();
    return true;
}

void MovingBlockGimmick::ApplyWoodMaterial()
{
    if (!object_) {
        return;
    }
    object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    Material* material = object_->GetMaterial();
    material->enableLighting = kMovingBlockWoodMaterialMode;
    material->enableEnvironmentMap = 0;
    material->environmentCoefficient = 0.0f;
}

void MovingBlockGimmick::EnableToonLighting()
{
    ApplyWoodMaterial();
}

void MovingBlockGimmick::Update()
{
    if (!object_) {
        return;
    }

    previousPosition_ = currentPosition_;

    float deltaTime = isEditorMode_ ? 0.0f : TimeManager::GetInstance()->GetDeltaTime();

    // 1. 位相の進行と折り返し
    phase_ += phaseDir_ * speed_ * deltaTime;
    if (phase_ >= std::numbers::pi_v<float>) {
        phase_ = std::numbers::pi_v<float>;
        phaseDir_ = -1.0f;
    } else if (phase_ <= 0.0f) {
        phase_ = 0.0f;
        phaseDir_ = 1.0f;
    }

    float kBlockSize = 1.0f;
    float distance = range_.x * kBlockSize;
    if (distance <= 0.0f) distance = 0.001f; // ゼロ除算防止

    // 2. 本来行きたい座標（Tentative Position）の計算
    float wave = (1.0f - std::cos(phase_)) * 0.5f;
    Vector3 tentativePosition = basePosition_;
    tentativePosition.x += axis_.x * wave * distance;
    tentativePosition.y += axis_.y * wave * distance;
    tentativePosition.z += axis_.z * wave * distance;

    // 3. 死体との衝突判定 (BeginOverlap方式)
    bool currentlyOverlapping = false;

    if (stage_ && !isEditorMode_) {
        AABB tentativeAABB;
        tentativeAABB.center = tentativePosition;
        tentativeAABB.size = {1.0f, 1.0f, 1.0f};

        for (BaseMapChipGimmick* gimmick : stage_->GetGimmicks()) {
            if (gimmick && gimmick->IsHardenedSlime()) {
                CollisionHit hit = CollisionManager::Intersect(tentativeAABB, gimmick->GetAABB());
                if (hit.isHit) {
                    currentlyOverlapping = true;
                    break;
                }
            }
        }
    }

    if (currentlyOverlapping) {
        if (!isOverlappingCorpse_) {
            // 初めて重なった瞬間（BeginOverlap）にのみ1回だけ反転する
            phaseDir_ = -phaseDir_;
            
            // このフレームの移動はキャンセルし、現在位置から逆再生を始める
            tentativePosition = currentPosition_;
            
            // 現在位置に合わせて位相（Phase）を逆算して同期
            Vector3 delta = tentativePosition - basePosition_;
            float currentDist = (delta.x * axis_.x) + (delta.y * axis_.y) + (delta.z * axis_.z);
            
            float newWave = std::clamp(currentDist / distance, 0.0f, 1.0f);
            phase_ = std::acos(std::clamp(1.0f - 2.0f * newWave, -1.0f, 1.0f));
            
            isOverlappingCorpse_ = true;
        }
    } else {
        // 重なりから抜け出した瞬間（EndOverlap）にフラグをリセット
        isOverlappingCorpse_ = false;
    }

    currentPosition_ = tentativePosition;
    object_->SetTranslate(currentPosition_);
    object_->Update();
}

void MovingBlockGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

AABB MovingBlockGimmick::GetAABB() const
{
    // ブロックのサイズは現状 1.0f x 1.0f x 1.0f と仮定
    AABB aabb;
    aabb.center = currentPosition_;
    aabb.size = {1.0f, 1.0f, 1.0f};
    return aabb;
}

Vector3 MovingBlockGimmick::GetDeltaPosition() const
{
    return currentPosition_ - previousPosition_;
}
