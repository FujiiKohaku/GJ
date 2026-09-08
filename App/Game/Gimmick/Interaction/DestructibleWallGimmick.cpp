/**
 * @file DestructibleWallGimmick.cpp
 * @brief 爆発によって破壊される壁ギミックの実装
 */
#include "DestructibleWallGimmick.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Audio/SoundManager.h"
#include "App/Game/Map/MapChipStage.h"
#include "Engine/LevelEditor/GimmickMetaDataManager.h"

namespace {
constexpr int32_t kDestructibleWallMaterialMode = 16;
}

DestructibleWallGimmick::DestructibleWallGimmick()
    : stage_(nullptr)
    , position_({0, 0, 0})
    , size_({1, 1, 1})
    , isEditorMode_(false)
    , isDestroyed_(false)
{
}

DestructibleWallGimmick::~DestructibleWallGimmick() = default;

bool DestructibleWallGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    position_ = position;

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());

    std::string finalModelPath = texturePath;
    if (const auto* metaData = GimmickMetaDataManager::GetInstance()->GetMetaData("DestructibleWall")) {
        finalModelPath = metaData->defaultModelPath;
    }

    if (!finalModelPath.empty()) {
        ModelManager::GetInstance()->Load(finalModelPath);
        object_->SetModel(finalModelPath);
    } else {
        Model* model = ModelManager::GetInstance()->CreateCube();
        if (model) {
            object_->SetModel(model);
        }
    }

    object_->SetTranslate(position_);
    object_->SetScale(size_);
    object_->SetEnableLighting(true);
    object_->GetMaterial()->enableLighting = kDestructibleWallMaterialMode;
    object_->GetMaterial()->enableEnvironmentMap = 0;
    object_->Update();

    return true;
}

std::shared_ptr<IGimmickState> DestructibleWallGimmick::CreateSnapshot() const
{
    auto state = std::make_shared<DestructibleWallState>();
    state->isDestroyed = isDestroyed_;
    return state;
}

void DestructibleWallGimmick::RestoreFromSnapshot(const IGimmickState* state)
{
    if (const auto* wallState = dynamic_cast<const DestructibleWallState*>(state)) {
        isDestroyed_ = wallState->isDestroyed;
        if (!isDestroyed_) {
            object_->SetScale(size_);
        } else {
            object_->SetScale({0.0f, 0.0f, 0.0f});
        }
        object_->Update();
    }
}

void DestructibleWallGimmick::EnableToonLighting()
{
    if (object_) {
        object_->GetMaterial()->enableLighting = kDestructibleWallMaterialMode;
    }
}

void DestructibleWallGimmick::Update()
{
    // 破壊済みなら何もしない
    if (isDestroyed_) return;

    if (object_) {
        object_->Update();
    }
}

void DestructibleWallGimmick::Draw()
{
    // 破壊済みなら描画しない（エディタモード時は半透明等で表示するとなお良い）
    if (isDestroyed_ && !isEditorMode_) return;

    if (object_) {
        object_->Draw();
    }
}

void DestructibleWallGimmick::SetEditorMode(bool isEditorMode)
{
    isEditorMode_ = isEditorMode;
}

AABB DestructibleWallGimmick::GetAABB() const
{
    // 破壊済みなら当たり判定を無くす
    if (isDestroyed_) {
        return { position_, {0.0f, 0.0f, 0.0f} };
    }

    AABB aabb;
    aabb.center = position_;
    aabb.size = size_;
    return aabb;
}

void DestructibleWallGimmick::SetStage(MapChipStage* stage)
{
    stage_ = stage;
}

void DestructibleWallGimmick::OnExplosion(const Vector3& origin, float radius)
{
    if (isDestroyed_ || isEditorMode_) return;

    // 自身が爆発の範囲内（半径内）にいるかどうかの判定は MapChipStage 側で
    // GetGimmicksInSphere によって行われているため、このメソッドが呼ばれた時点で被害確定。

    isDestroyed_ = true;
    
    // 破壊時のSE再生
    SoundManager::GetInstance()->PlaySE("WallDestroy", 0.5f);
}
