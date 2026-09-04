#include "SpikeGimmick.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "App/Game/Map/MapChipStage.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Logger/Logger.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/LevelEditor/GimmickMetaDataManager.h"
#include <format>

// 初期AABB設定: 横1、高さ0.7、底面(Y=-0.5)にくっつくようにY=-0.15オフセット
Vector3 SpikeGimmick::s_spikeAABBSize = { 1.0f, 0.7f, 1.0f };
Vector3 SpikeGimmick::s_spikeAABBOffset = { 0.0f, -0.15f, 0.0f };

SpikeGimmick::SpikeGimmick()
{
}

bool SpikeGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    position_ = position;

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(Object3dManager::GetInstance());
    
    // モデルのロード
    std::string modelFile = "Thorn/Thorn.obj"; // Fallback
    if (const auto* metaData = GimmickMetaDataManager::GetInstance()->GetMetaData("Spike")) {
        modelFile = metaData->defaultModelPath;
    }
    
    // texturePath（checkerboard.png等）を渡してくるパターンがあるため、objやgltf以外は無視するか、ここではメタデータを優先する。
    // MapChipStageから直接objファイル名が渡された場合のみ上書きする
    if (!texturePath.empty() && (texturePath.find(".obj") != std::string::npos || texturePath.find(".gltf") != std::string::npos)) {
        modelFile = texturePath;
    }
    ModelManager::GetInstance()->Load(modelFile);
    
    object3d_->SetModel(modelFile);
    object3d_->SetTranslate(position_);
    object3d_->SetScale({1.0f, 1.0f, 1.0f});
    object3d_->SetEnableLighting(true);
    object3d_->Update();

    if (gimmickParam) {
        param_ = std::make_unique<SpikeParam>(*static_cast<const SpikeParam*>(gimmickParam));
    } else {
        param_ = std::make_unique<SpikeParam>();
    }

    return true;
}

void SpikeGimmick::SetStage(MapChipStage* stage)
{
    stage_ = stage;
}

void SpikeGimmick::Update()
{
    object3d_->SetTranslate(position_);
    object3d_->Update();

    if (!stage_) return;

    MapChipPlayer* player = stage_->GetPlayer();
    if (!player) return;

    // プレイヤーのAABBとトゲのAABBの交差判定
    AABB playerAABB = player->GetAABB();
    AABB spikeAABB = GetAABB();

    bool isColliding = CollisionManager::Intersect(playerAABB, spikeAABB).isHit;

    if (isColliding) {
        if (!wasPlayerColliding_) {
            // トゲに新しく触れた（または上から乗った）瞬間の処理
            Logger::Log(std::format("[SpikeGimmick] Player touched the spike at ({:.2f}, {:.2f}, {:.2f})\n",
                                    position_.x, position_.y, position_.z));

            // =====================================================================================
            // TODO: ここにチームメンバーが「プレイヤーへのダメージ」や「死体のスポーン」処理を実装する
            // =====================================================================================
            // 例: player->TakeDamage(1);
            // 例: stage_->GetEventManager().PublishEvent("PlayerKilledBySpike", &position_);
            // =====================================================================================
        }
        wasPlayerColliding_ = true;
    } else {
        wasPlayerColliding_ = false;
    }
}

void SpikeGimmick::Draw()
{
    if (object3d_) {
        object3d_->Draw();
    }
}

AABB SpikeGimmick::GetAABB() const
{
    AABB aabb;
    aabb.center = position_ + s_spikeAABBOffset;
    aabb.size = s_spikeAABBSize;
    return aabb;
}
