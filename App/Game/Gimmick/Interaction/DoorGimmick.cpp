#include "DoorGimmick.h"
#include "DoorParam.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "App/Game/Map/MapChipStage.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Logger/Logger.h"
#include "Engine/Audio/SoundManager.h"
#include <cmath>
#include <numbers>

DoorGimmick::DoorGimmick()
    : stage_(nullptr)
    , basePosition_({0, 0, 0})
    , isEditorMode_(false)
    , isOpen_(false)
    , openProgress_(0.0f)
    , isSolid_(true)
{
}

DoorGimmick::~DoorGimmick() = default;

bool DoorGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    basePosition_ = position;
    
    if (gimmickParam) {
        param_ = std::unique_ptr<DoorParam>(static_cast<DoorParam*>(gimmickParam->Clone().release()));
    } else {
        param_ = std::make_unique<DoorParam>();
    }

    // 1マスあたり高さ1.0、1パーツは0.5なので、必要なパーツ数 = マス数 * 2
    int partCount = param_->heightBlocks_ * 2;
    if (partCount <= 0) partCount = 2;

    // モデルのロード
    ModelManager::GetInstance()->Load("Door/Door_Top.obj");
    ModelManager::GetInstance()->Load("Door/Door_Middle.obj");
    ModelManager::GetInstance()->Load("Door/Door_Bottom.obj");

    for (int i = 0; i < partCount; ++i) {
        auto obj = std::make_unique<Object3d>();
        obj->Initialize(Object3dManager::GetInstance());
        
        std::string modelFile;
        if (i == 0) {
            modelFile = "Door/Door_Bottom.obj";
        } else if (i == partCount - 1) {
            modelFile = "Door/Door_Top.obj";
        } else {
            modelFile = "Door/Door_Middle.obj";
        }
        
        obj->SetModel(modelFile);
        obj->SetEnableLighting(true);
        // 初期座標設定（更新処理で上書きされるが初期設定として）
        Vector3 p = basePosition_;
        p.y += -0.25f + i * 0.5f;
        obj->SetTranslate(p);
        obj->Update();
        
        objects_.push_back(std::move(obj));
    }

    return true;
}

void DoorGimmick::SetStage(MapChipStage* stage)
{
    stage_ = stage;
    // イベント名を使ったSubscribeによる開閉制御は廃止し、
    // Update内での対象ギミック(Switch)のポーリングによる状態同期へ移行
}

void DoorGimmick::Update()
{
    if (!isEditorMode_ && stage_) {
        // リンク名が一致するギミック(感圧盤など)の状態を監視する
        bool shouldOpen = false;
        for (auto* gimmick : stage_->GetGimmicks()) {
            if (!param_->listenEventName_.empty() && gimmick->GetLinkName() == param_->listenEventName_) {
                if (gimmick->IsActive()) {
                    shouldOpen = true;
                    break;
                }
            }
        }
        float previousProgress = openProgress_;

        if (isOpen_ != shouldOpen) {
            if (shouldOpen) {
                SoundManager::GetInstance()->PlaySE("DoorOpen", 0.3f);
            }
            isOpen_ = shouldOpen;
        }

        float dt = TimeManager::GetInstance()->GetDeltaTime();
        float speed = 3.0f; // 開閉スピード

        if (isOpen_) {
            openProgress_ += speed * dt;
            if (openProgress_ > 1.0f) openProgress_ = 1.0f;
        } else {
            openProgress_ -= speed * dt;
            if (openProgress_ < 0.0f) openProgress_ = 0.0f;
        }

        // ドアが閉まりきった瞬間に音を鳴らす
        if (!isOpen_ && previousProgress > 0.0f && openProgress_ == 0.0f) {
            SoundManager::GetInstance()->PlaySE("DoorClose", 0.3f);
        }
    } // !isEditorMode_ && stage_ の終了

    // 閉まる判定になった瞬間（isOpen_ == false）は、即座に当たり判定を復活させて駆け込みをブロックする
    if (!isOpen_) {
        isSolid_ = true;
    } else {
        // 開く時は完全に開ききるまで当たり判定を残す（安全策）
        isSolid_ = (openProgress_ < 0.99f);
    }

    // 回転とオフセットの計算
    float maxAngle = std::numbers::pi_v<float> / 2.0f; // 90度
    float currentRotY = openProgress_ * maxAngle;

    // 蝶番(ピボット)の位置。X=-0.5の位置とする。
    Vector3 pivot = basePosition_;
    pivot.x -= 0.5f;

    // 蝶番から見た中心座標のローカルオフセット
    float offsetX = 0.5f;
    float offsetZ = 0.0f;

    // 回転後の中心オフセット
    float rotatedX = offsetX * std::cos(currentRotY) - offsetZ * std::sin(currentRotY);
    float rotatedZ = offsetX * std::sin(currentRotY) + offsetZ * std::cos(currentRotY);

    for (size_t i = 0; i < objects_.size(); ++i) {
        Vector3 pos = pivot;
        pos.x += rotatedX;
        pos.z += rotatedZ;
        pos.y += -0.25f + i * 0.5f; // 床に接するように -0.25 オフセット + 高さの積み上げ
        
        objects_[i]->SetRotate({0.0f, currentRotY, 0.0f});
        objects_[i]->SetTranslate(pos);
        objects_[i]->Update();
    }
}

void DoorGimmick::Draw()
{
    for (const auto& obj : objects_) {
        obj->Draw();
    }
}

void DoorGimmick::EnableToonLighting()
{
    for (const auto& obj : objects_) {
        obj->EnableToonLighting();
    }
}

AABB DoorGimmick::GetAABB() const
{
    AABB aabb;
    float height = static_cast<float>(param_->heightBlocks_);
    
    // 蝶番(ピボット)の位置
    Vector3 pivot = basePosition_;
    pivot.x -= 0.5f;
    
    float maxAngle = std::numbers::pi_v<float> / 2.0f; // 90度
    float currentRotY = openProgress_ * maxAngle;
    
    // 扉の中心のローカルオフセット（蝶番から見て）
    float offsetX = 0.5f;
    float offsetZ = 0.0f;
    
    // 回転後の中心のオフセット
    float rotatedX = offsetX * std::cos(currentRotY) - offsetZ * std::sin(currentRotY);
    float rotatedZ = offsetX * std::sin(currentRotY) + offsetZ * std::cos(currentRotY);
    
    aabb.center = pivot;
    aabb.center.x += rotatedX;
    aabb.center.z += rotatedZ;
    aabb.center.y += -0.5f + (height / 2.0f); // 描画の下端が basePosition.y - 0.5 なので中心は +height/2
    
    // 開閉状況に応じてサイズ(AABB)を補間する
    // 閉じている時は横幅(X)をモデルに合わせた 0.1f とし、奥行き(Z)を 1.0f とする
    Vector3 closedSize = {0.1f, height, 1.0f};
    
    // 完全に開いた時は90度奥に回転しているため、横幅と奥行きが入れ替わる
    Vector3 openSize = {1.0f, height, 0.1f};
    
    if (!isOpen_) {
        // 閉まる判定になった時は、アニメーションの途中でも当たり判定を即座に「完全に閉まった状態」に戻す
        // これにより、閉まりかけの薄い判定をすり抜ける駆け込みバグを防止し、確実に手前へ弾き返す
        aabb.center = basePosition_;
        aabb.center.y += -0.5f + (height / 2.0f);
        aabb.size = closedSize;
    } else {
        // 開く時はアニメーションに追従する
        aabb.size.x = std::lerp(closedSize.x, openSize.x, openProgress_);
        aabb.size.y = height;
        aabb.size.z = std::lerp(closedSize.z, openSize.z, openProgress_);
    }
    
    return aabb;
}
