/**
 * @file GearGimmick.cpp
 * @brief 歯車（Gear）障害物ギミックの実装
 */
#include "GearGimmick.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "App/Game/Map/MapChipStage.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Audio/SoundManager.h"
#include "Engine/LevelEditor/GimmickMetaDataManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include <format>
#include <cmath>

GearGimmick::GearGimmick()
    : stage_(nullptr)
    , position_({0, 0, 0})
    , currentRotationZ_(0.0f)
{
}

GearGimmick::~GearGimmick()
{
    StopJammedEffects();
}

bool GearGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    position_ = position;

    // パラメータの設定
    if (gimmickParam) {
        param_ = std::make_unique<GearParam>(*static_cast<const GearParam*>(gimmickParam));
    } else {
        param_ = std::make_unique<GearParam>();
    }

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(Object3dManager::GetInstance());
    
    // モデルパスの決定
    std::string modelFile = "Gear/Gear.obj"; // Fallback
    if (const auto* metaData = GimmickMetaDataManager::GetInstance()->GetMetaData("Gear")) {
        modelFile = metaData->defaultModelPath;
    }
    
    if (!texturePath.empty() && (texturePath.find(".obj") != std::string::npos || texturePath.find(".gltf") != std::string::npos)) {
        modelFile = texturePath;
    }
    
    ModelManager::GetInstance()->Load(modelFile);
    object3d_->SetModel(modelFile);
    
    // YとZをブロックの中心(0.5)に合わせて配置する場合のオフセット（必要に応じて調整）
    Vector3 centerPos = position_;
    
    object3d_->SetTranslate(centerPos);
    object3d_->SetScale({param_->scale_, param_->scale_, param_->scale_});
    object3d_->SetEnableLighting(true);
    object3d_->Update();

    return true;
}

void GearGimmick::SetStage(MapChipStage* stage)
{
    stage_ = stage;
}

void GearGimmick::Update()
{
    float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    
    // プレイヤーの死体との当たり判定（スタック判定）
    bool isCurrentlyJammed = false;
    if (stage_) {
        AABB gearAABB = GetAABB();
        for (BaseMapChipGimmick* gimmick : stage_->GetGimmicks()) {
            if (gimmick->IsHardenedSlime()) {
                for (const AABB& slimeBox : gimmick->GetCollisionBoxes()) {
                    if (CollisionManager::Intersect(gearAABB, slimeBox).isHit) {
                        isCurrentlyJammed = true;
                        break;
                    }
                }
            }
            if (isCurrentlyJammed) break;
        }
    }
    
    isJammed_ = isCurrentlyJammed;
    
    // モデルの中心を常に維持
    Vector3 centerPos = position_;

    // 回転と微振動の更新
    if (param_) {
        if (isJammed_) {
            jamShakeTimer_ += deltaTime;
            float shakeX = std::sin(jamShakeTimer_ * 50.0f) * 0.05f;
            float shakeY = std::cos(jamShakeTimer_ * 45.0f) * 0.05f;
            centerPos.x += shakeX;
            centerPos.y += shakeY;
        } else {
            jamShakeTimer_ = 0.0f;
            // 回転速度(度/秒)をラジアンに変換して加算
            float radPerSec = param_->rotationSpeed_ * (3.14159265358979323846f / 180.0f);
            currentRotationZ_ += radPerSec * deltaTime;
            
            // オーバーフロー防止
            if (currentRotationZ_ > 3.1415926535f * 2.0f) {
                currentRotationZ_ -= 3.1415926535f * 2.0f;
            } else if (currentRotationZ_ < -3.1415926535f * 2.0f) {
                currentRotationZ_ += 3.1415926535f * 2.0f;
            }
        }
        
        object3d_->SetRotate({0.0f, 0.0f, currentRotationZ_});
        
        // スケール変更の反映
        object3d_->SetScale({param_->scale_, param_->scale_, param_->scale_});
    }
    
    object3d_->SetTranslate(centerPos);
    object3d_->Update();
    
    UpdateJammedEffects();

    // プレイヤーとの当たり判定
    if (!stage_) return;

    bool anyColliding = false;
    for (MapChipPlayer* player : stage_->GetPlayers()) {

        AABB playerAABB = player->GetAABB();
        
        // 球体(Sphere)とAABBの交差判定
        Sphere gearSphere;
        gearSphere.center = centerPos;
        gearSphere.radius = param_ ? param_->collisionRadius_ : 0.4f;

        CollisionHit hit = CollisionManager::Intersect(gearSphere, playerAABB);

        if (hit.isHit) {
            // スタック中（isJammed_がtrue）なら足場となるので、即死やSE再生を行わない
            if (!isJammed_) {
                // プレイヤーがまだ生きていて（死体形成中ではなく）、かつ今回初めて接触した場合のみSEを鳴らす
                if (!player->IsShapingSelfDestruct() && !wasPlayerColliding_) {
                    Logger::Log(std::format("[GearGimmick] Player touched the gear at ({:.2f}, {:.2f}, {:.2f})\n",
                                            position_.x, position_.y, position_.z));
                    SoundManager::GetInstance()->PlaySE("GearHitSlime");
                }
                player->Kill();
            }
            
            anyColliding = true;
        }
    }
    wasPlayerColliding_ = anyColliding;
}

void GearGimmick::Draw()
{
    if (object3d_) {
        object3d_->Draw();
    }
}

AABB GearGimmick::GetAABB() const
{
    AABB aabb;
    aabb.center = position_;
    float r = param_ ? param_->collisionRadius_ : 0.4f;
    aabb.size = {r * 2.0f, r * 2.0f, r * 2.0f};
    return aabb;
}

void GearGimmick::UpdateJammedEffects()
{
    EffectManager* effects = EffectManager::GetInstance();
    
    // スケールに応じてエフェクトの位置（Z座標）を手前にオフセットする
    // Zマイナス方向が画面手前であることを前提に、モデルの厚み分だけ前に出す
    float zOffset = param_ ? (param_->scale_ * 0.6f) : 0.6f;
    Vector3 effectPos = position_;
    effectPos.z -= zOffset;

    if (isJammed_) {
        if (smokeEffectHandles_.empty()) {
            EffectHandle handle = effects->PlayLoopEffect("GearJamSmoke", effectPos);
            if (handle != kInvalidEffectHandle) {
                smokeEffectHandles_.push_back(handle);
                // 歯車のスケールに合わせてエフェクトの大きさも広げる
                if (param_) {
                    effects->SetEffectScale(handle, param_->scale_);
                }
            }
        }
        // 位置とスケールの更新
        for (EffectHandle handle : smokeEffectHandles_) {
            if (effects->IsEffectAlive(handle)) {
                effects->SetEffectPosition(handle, effectPos);
                if (param_) {
                    effects->SetEffectScale(handle, param_->scale_);
                }
            }
        }
    } else {
        StopJammedEffects();
    }
}

void GearGimmick::StopJammedEffects()
{
    if (!smokeEffectHandles_.empty()) {
        EffectManager* effects = EffectManager::GetInstance();
        for (EffectHandle handle : smokeEffectHandles_) {
            effects->StopEffect(handle);
        }
        smokeEffectHandles_.clear();
    }
}
