/**
 * @file GasEmitterGimmick.cpp
 * @brief ガス発生装置の実装
 */
#include "GasEmitterGimmick.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/Object3d.h"
#include "App/Game/Map/MapChipStage.h"
#include "Engine/Logger/Logger.h"

GasEmitterGimmick::GasEmitterGimmick()
    : stage_(nullptr)
    , position_({0, 0, 0})
    , size_({1, 1, 1})
    , isEditorMode_(false)
    , isEmitting_(false)
{
}

GasEmitterGimmick::~GasEmitterGimmick() = default;

bool GasEmitterGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    position_ = position;

    if (gimmickParam) {
        param_ = std::unique_ptr<GasEmitterParam>(static_cast<GasEmitterParam*>(gimmickParam->Clone().release()));
    } else {
        param_ = std::make_unique<GasEmitterParam>();
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());

    // 指定されたVentモデルをロード
    const std::string modelFile = "Vent/Venct.obj"; // 指定通りのパス
    ModelManager::GetInstance()->Load(modelFile);
    object_->SetModel(modelFile);

    object_->SetTranslate(position_);
    object_->SetScale(size_);
    object_->SetEnableLighting(true);
    object_->Update();

    return true;
}

void GasEmitterGimmick::Update()
{
    if (object_) {
        object_->Update();
    }

    if (isEditorMode_) return;

    if (isEmitting_) {
        // TODO: ガス放出中のエフェクト（パーティクル）の更新処理など
    }
}

void GasEmitterGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

void GasEmitterGimmick::SetEditorMode(bool isEditorMode)
{
    isEditorMode_ = isEditorMode;
}

AABB GasEmitterGimmick::GetAABB() const
{
    AABB aabb;
    aabb.center = position_;
    aabb.size = size_;
    return aabb;
}

AABB GasEmitterGimmick::GetGasAABB() const
{
    AABB aabb;
    if (param_) {
        float sizeX = (param_->leftBlocks_ + param_->rightBlocks_ + 1) * 1.0f;
        float sizeY = (param_->downBlocks_ + param_->upBlocks_ + 1) * 1.0f;
        float sizeZ = 1.0f;
        
        float centerX = position_.x + (static_cast<float>(param_->rightBlocks_) - static_cast<float>(param_->leftBlocks_)) * 0.5f;
        float centerY = position_.y + (static_cast<float>(param_->upBlocks_) - static_cast<float>(param_->downBlocks_)) * 0.5f;
        
        aabb.center = {centerX, centerY, position_.z};
        aabb.size = {sizeX, sizeY, sizeZ};
    } else {
        aabb.center = position_;
        aabb.size = {3.0f, 3.0f, 3.0f}; // フォールバック
    }
    return aabb;
}

void GasEmitterGimmick::SetStage(MapChipStage* stage)
{
    stage_ = stage;
    if (stage_ && param_) {
        // イベント名が設定されていれば、そのイベントを受信した際に StartEmitting() を実行するよう登録
        if (!param_->listenEventName_.empty()) {
            stage_->GetEventManager().Subscribe(param_->listenEventName_, [this]() {
                StartEmitting();
            });
        }
    }
}

void GasEmitterGimmick::StartEmitting()
{
    if (isEmitting_) return;
    isEmitting_ = true;
    Logger::Log("GasEmitterGimmick: Started emitting gas\n");
    // TODO: ガス発生のパーティクル再生開始
}

void GasEmitterGimmick::OnSpark(const Vector3& origin)
{
    if (!isEmitting_ || !stage_ || isEditorMode_) return;

    // スパーク座標が自分のガスエリアに入っているか判定する
    AABB gasArea = GetGasAABB();
    Vector3 diff = origin - gasArea.center;
    // 簡単のため AABB と 点(origin) の内包判定
    bool inRangeX = std::abs(diff.x) <= gasArea.size.x * 0.5f;
    bool inRangeY = std::abs(diff.y) <= gasArea.size.y * 0.5f;
    bool inRangeZ = std::abs(diff.z) <= gasArea.size.z * 0.5f;

    if (inRangeX && inRangeY && inRangeZ) {
        // ガスに引火！大爆発を発生させる
        // 爆発の半径はガスエリアより少し広いか、同等とする
        float explosionRadius = (std::max)({gasArea.size.x, gasArea.size.y, gasArea.size.z});
        
        // 爆発を発生させる
        stage_->CreateExplosion(position_, explosionRadius);
        
        // 爆発後、ガスは消滅する（または放出元が壊れる）
        isEmitting_ = false;
        // TODO: 自身のモデルを非表示にするなどの処理
    }
}
