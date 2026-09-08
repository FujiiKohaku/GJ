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
    
    // 回転の更新
    if (param_) {
        // 回転速度(度/秒)をラジアンに変換して加算
        float radPerSec = param_->rotationSpeed_ * (3.14159265358979323846f / 180.0f);
        currentRotationZ_ += radPerSec * deltaTime;
        
        // オーバーフロー防止
        if (currentRotationZ_ > 3.1415926535f * 2.0f) {
            currentRotationZ_ -= 3.1415926535f * 2.0f;
        } else if (currentRotationZ_ < -3.1415926535f * 2.0f) {
            currentRotationZ_ += 3.1415926535f * 2.0f;
        }
        
        object3d_->SetRotate({0.0f, 0.0f, currentRotationZ_});
        
        // スケール変更の反映
        object3d_->SetScale({param_->scale_, param_->scale_, param_->scale_});
    }
    
    // モデルの中心を常に維持
    Vector3 centerPos = position_;
    object3d_->SetTranslate(centerPos);
    
    object3d_->Update();

    // プレイヤーとの当たり判定
    if (!stage_) return;

    MapChipPlayer* player = stage_->GetPlayer();
    if (!player) return;

    AABB playerAABB = player->GetAABB();
    
    // 球体(Sphere)とAABBの交差判定
    Sphere gearSphere;
    gearSphere.center = centerPos;
    gearSphere.radius = param_ ? param_->collisionRadius_ : 0.4f;

    CollisionHit hit = CollisionManager::Intersect(gearSphere, playerAABB);

    if (hit.isHit) {
        if (!wasPlayerColliding_) {
            Logger::Log(std::format("[GearGimmick] Player touched the gear at ({:.2f}, {:.2f}, {:.2f})\n",
                                    position_.x, position_.y, position_.z));
            player->Kill();
        }
        wasPlayerColliding_ = true;
    } else {
        wasPlayerColliding_ = false;
    }
}

void GearGimmick::Draw()
{
    if (object3d_) {
        object3d_->Draw();
    }
}

AABB GearGimmick::GetAABB() const
{
    // ISolid() が false なので地形の衝突には使われないが、とりあえずAABBも返す
    AABB aabb;
    aabb.center = position_;
    float r = param_ ? param_->collisionRadius_ : 0.4f;
    aabb.size = {r * 2.0f, r * 2.0f, r * 2.0f};
    return aabb;
}
