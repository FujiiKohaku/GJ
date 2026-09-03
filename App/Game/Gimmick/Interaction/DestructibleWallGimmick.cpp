/**
 * @file DestructibleWallGimmick.cpp
 * @brief 爆発によって破壊される壁ギミックの実装
 */
#include "DestructibleWallGimmick.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/Object3d.h"
#include "App/Game/Map/MapChipStage.h"

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

    // 破壊される石ブロックのモデルをロード
    const std::string modelFile = "StoneBlock/StoneBlock.obj";
    ModelManager::GetInstance()->Load(modelFile);
    object_->SetModel(modelFile);

    object_->SetTranslate(position_);
    object_->SetScale(size_);
    object_->SetEnableLighting(true);
    object_->Update();

    return true;
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
    
    // TODO: 破壊時のパーティクル再生やSE再生
}
