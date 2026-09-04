/**
 * @file SwitchGimmick.cpp
 * @brief スイッチおよび着火ギミックの実装
 */
#include "SwitchGimmick.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/Object3d.h"
#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Logger/Logger.h"

Vector3 SwitchGimmick::s_pressurePlateAABBOffset = { 0.0f, -0.4f, 0.0f };
Vector3 SwitchGimmick::s_pressurePlateAABBSize = { 0.8f, 0.2f, 0.8f };

SwitchGimmick::SwitchGimmick()
    : stage_(nullptr)
    , position_({0, 0, 0})
    , size_({1, 1, 1})
    , isEditorMode_(false)
    , isActive_(false)
{
}

SwitchGimmick::~SwitchGimmick() = default;

bool SwitchGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    position_ = position;

    // パラメータの取得と保持
    if (gimmickParam) {
        param_ = std::unique_ptr<SwitchParam>(static_cast<SwitchParam*>(gimmickParam->Clone().release()));
    } else {
        param_ = std::make_unique<SwitchParam>();
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());

    // Typeに応じたモデルのロード
    std::string modelFile;
    if (param_->switchType_ == 2) {
        modelFile = "Bonfire/Bonfire.obj";
    } else {
        modelFile = "PressurePlate/PressurePlate.obj";
    }

    ModelManager::GetInstance()->Load(modelFile);
    object_->SetModel(modelFile);

    object_->SetTranslate(position_);
    object_->SetScale(size_);
    object_->SetEnableLighting(true);
    object_->Update();

    return true;
}

void SwitchGimmick::Update()
{
    if (object_) {
        object_->Update();
    }

    // エディタモード時はギミックの判定処理を行わない
    if (isEditorMode_) return;

    if (!stage_) return;

    // Typeに応じた動作
    if (param_->switchType_ == 2) {
        // 篝火(着火源)の場合：毎フレーム（または適度な間隔で）周囲に着火判定を出す
        // ガスエリアが存在すれば誘爆する
        stage_->CreateSpark(position_);
    } else if (param_->switchType_ == 0) {
        // 感圧盤の場合：プレイヤーとの当たり判定をチェックする
        bool isStepped = false;
        if (stage_ && stage_->GetPlayer()) {
            AABB playerBox = stage_->GetPlayer()->GetAABB();
            if (CollisionManager::Intersect(GetAABB(), playerBox).isHit) {
                isStepped = true;
            }
        }

        // プレイヤーが復帰した後も、硬化した死体の重さで感圧板を維持する。
        if (!isStepped) {
            for (BaseMapChipGimmick* gimmick : stage_->GetGimmicks()) {
                if (gimmick != this && gimmick->IsHardenedSlime()) {
                    for (const AABB& bodyBox : gimmick->GetCollisionBoxes()) {
                        if (CollisionManager::Intersect(GetAABB(), bodyBox).isHit) {
                            isStepped = true;
                            break;
                        }
                    }
                    if (isStepped) break;
                }
            }
        }
        
        if (isStepped) { // TODO: 本来は param_->requiredWeight_ などを考慮する
            if (!isActive_) {
                Logger::Log("SwitchGimmick(PressurePlate): Event Fired -> " + param_->fireEventName_ + "\n");
                stage_->GetEventManager().Publish(param_->fireEventName_);
                isActive_ = true;
            }
        } else {
            isActive_ = false;
        }
    }
}

void SwitchGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

void SwitchGimmick::SetEditorMode(bool isEditorMode)
{
    isEditorMode_ = isEditorMode;
}

AABB SwitchGimmick::GetAABB() const
{
    AABB aabb;
    if (param_ && param_->switchType_ == 0) {
        // 感圧盤の場合
        aabb.center = position_ + s_pressurePlateAABBOffset;
        aabb.size = s_pressurePlateAABBSize;
    } else {
        aabb.center = position_;
        aabb.size = size_;
    }
    return aabb;
}

void SwitchGimmick::SetStage(MapChipStage* stage)
{
    stage_ = stage;
}
