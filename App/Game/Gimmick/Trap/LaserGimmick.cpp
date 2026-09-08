#include "LaserGimmick.h"
#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Logger/Logger.h"
#include "Engine/LevelEditor/GimmickMetaDataManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/3D/LaserBeamRenderer.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace {
constexpr float kLaserThickness = 0.2f;
constexpr float kPlayerDeathDelay = 0.15f;

bool OverlapsLaserWidth(
    const AABB& box,
    const Vector3& emitterPosition,
    int dx,
    int dy)
{
    const Vector3 halfSize = box.size * 0.5f;
    const float halfThickness = kLaserThickness * 0.5f;
    if (std::abs(box.center.z - emitterPosition.z) >
        halfSize.z + halfThickness) {
        return false;
    }

    if (dx != 0) {
        return std::abs(box.center.y - emitterPosition.y) <=
            halfSize.y + halfThickness;
    }
    if (dy != 0) {
        return std::abs(box.center.x - emitterPosition.x) <=
            halfSize.x + halfThickness;
    }
    return false;
}

float GetBlockDistanceFromHardenedSlime(
    const AABB& box,
    const Vector3& emitterPosition,
    int dx,
    int dy)
{
    if (!OverlapsLaserWidth(box, emitterPosition, dx, dy)) {
        return -1.0f;
    }

    const Vector3 halfSize = box.size * 0.5f;
    if (dx > 0) {
        const float muzzle = emitterPosition.x + 0.5f;
        if (box.center.x + halfSize.x < muzzle) return -1.0f;
        return (std::max)(0.0f, box.center.x - halfSize.x - muzzle);
    }
    if (dx < 0) {
        const float muzzle = emitterPosition.x - 0.5f;
        if (box.center.x - halfSize.x > muzzle) return -1.0f;
        return (std::max)(0.0f, muzzle - (box.center.x + halfSize.x));
    }
    if (dy > 0) {
        const float muzzle = emitterPosition.y + 0.5f;
        if (box.center.y + halfSize.y < muzzle) return -1.0f;
        return (std::max)(0.0f, box.center.y - halfSize.y - muzzle);
    }
    if (dy < 0) {
        const float muzzle = emitterPosition.y - 0.5f;
        if (box.center.y - halfSize.y > muzzle) return -1.0f;
        return (std::max)(0.0f, muzzle - (box.center.y + halfSize.y));
    }
    return -1.0f;
}
}

LaserGimmick::LaserGimmick()
{
}

bool LaserGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    position_ = position;

    // --- パラメータの初期化 ---
    if (gimmickParam) {
        param_ = std::make_unique<LaserParam>(*static_cast<const LaserParam*>(gimmickParam));
    } else {
        param_ = std::make_unique<LaserParam>();
    }

    // --- 発射機本体の初期化 ---
    emitterObject_ = std::make_unique<Object3d>();
    emitterObject_->Initialize(Object3dManager::GetInstance());
    
    std::string emitterModel = texturePath;
    if (const auto* metaData = GimmickMetaDataManager::GetInstance()->GetMetaData("LaserEmitter")) {
        emitterModel = metaData->defaultModelPath;
    }

    if (!emitterModel.empty()) {
        ModelManager::GetInstance()->Load(emitterModel);
        emitterObject_->SetModel(emitterModel);
    } else {
        Model* model = ModelManager::GetInstance()->CreateCube();
        if (model) {
            emitterObject_->SetModel(model);
        }
    }
    emitterObject_->SetTranslate(position_);
    emitterObject_->SetScale({1.0f, 1.0f, 1.0f});
    emitterObject_->SetEnableLighting(true);

    // 発射機の回転（デフォルトで左(2)を向いている前提）
    // 方向: 0=Up, 1=Down, 2=Left, 3=Right
    // マップ上の座標系: X右が正, Y上が正
    float rotationZ = 0.0f;
    if (param_->direction_ == 0) {
        rotationZ = -std::numbers::pi_v<float> / 2.0f; // 左(2)から上(0)へ -90度
    } else if (param_->direction_ == 1) {
        rotationZ = std::numbers::pi_v<float> / 2.0f;  // 左(2)から下(1)へ +90度
    } else if (param_->direction_ == 2) {
        rotationZ = 0.0f;                              // 左(2)はそのまま 0度
    } else if (param_->direction_ == 3) {
        rotationZ = std::numbers::pi_v<float>;         // 左(2)から右(3)へ 180度
    }
    emitterObject_->SetRotate({0.0f, 0.0f, rotationZ});
    emitterObject_->Update();

    // レーザーを世界観（自然環境、焚き火）と危険度に合わせてオレンジ〜赤色系に設定
    // スライム（青）に対して補色となるため非常に見やすくなります
    beamParams_.color = { 1.0f, 0.3f, 0.0f, 1.0f };       // オーラ: 濃いオレンジ
    beamParams_.coreColor = { 1.0f, 0.9f, 0.5f, 1.0f };   // コア: まぶしい黄白色
    beamParams_.intensity = 8.0f;
    beamParams_.coreIntensity = 40.0f;
    beamParams_.noiseScale = 1.2f;

    return true;
}

void LaserGimmick::SetStage(MapChipStage* stage)
{
    stage_ = stage;
}

void LaserGimmick::Update()
{
    if (emitterObject_) {
        emitterObject_->SetTranslate(position_);
        emitterObject_->Update();
    }

    if (!stage_) return;

    // --- Step 1: 静的な壁までの距離計算（グリッド検索） ---
    int dx = 0;
    int dy = 0;
    if (param_->direction_ == 0) dy = 1;  // Up
    if (param_->direction_ == 1) dy = -1; // Down
    if (param_->direction_ == 2) dx = -1; // Left
    if (param_->direction_ == 3) dx = 1;  // Right

    // 現在のマスのインデックス（Y軸はワールド座標とインデックスで逆転している）
    const auto& field = stage_->GetField();
    int currentXIndex = static_cast<int>(std::round(position_.x));
    int currentYIndex = static_cast<int>(field.GetBlockHeight()) - 1 - static_cast<int>(std::round(position_.y));

    // インデックス空間での探索方向
    int dxIndex = dx;
    int dyIndex = -dy;

    int distance = 0;
    int maxDist = param_->maxDistance_;

    for (int i = 1; i <= maxDist; ++i) {
        int checkX = currentXIndex + dxIndex * i;
        int checkY = currentYIndex + dyIndex * i;

        MapChipType type = field.GetMapChipTypeByIndex(checkX, checkY);
        // 壁とみなすブロック
        if (MapChipRegistry::IsSolidBlock(type)) {
            break; // 障害物に当たったのでストップ
        }
        distance = i;
    }

    float floatDist = static_cast<float>(distance);

    // 硬化したスライムの実形状を遮蔽物として扱い、最も手前でレーザーを止める。
    for (BaseMapChipGimmick* gimmick : stage_->GetGimmicks()) {
        if (!gimmick || !gimmick->IsHardenedSlime()) {
            continue;
        }
        for (const AABB& bodyBox : gimmick->GetCollisionBoxes()) {
            const float blockDistance = GetBlockDistanceFromHardenedSlime(
                bodyBox,
                position_,
                dx,
                dy);
            if (blockDistance >= 0.0f) {
                floatDist = (std::min)(floatDist, blockDistance);
            }
        }
    }
    
    staticLaserLength_ = floatDist;

    // --- Step 2: レーザービームのAABBを仮作成 ---
    // 発射口（本体の中心から少し前）から、障害物の手前までの長さ
    Vector3 laserCenter = position_;
    Vector3 laserSize = {
        kLaserThickness,
        kLaserThickness,
        kLaserThickness };

    if (dx != 0) {
        laserSize.x = floatDist;
        laserCenter.x += (floatDist / 2.0f) * dx;
        // 少し発射機側からオフセットさせる（発射機にめり込まないように）
        laserCenter.x += 0.5f * dx; 
    } else if (dy != 0) {
        laserSize.y = floatDist;
        laserCenter.y += (floatDist / 2.0f) * dy;
        laserCenter.y += 0.5f * dy;
    }

    AABB tempLaserAABB;
    tempLaserAABB.center = laserCenter;
    tempLaserAABB.size = laserSize;

    // --- Step 3: 動的なプレイヤーとの交差判定 ---
    MapChipPlayer* player = stage_->GetPlayer();
    bool hitPlayer = false;

    if (player) {
        AABB playerAABB = player->GetAABB();
        if (CollisionManager::Intersect(playerAABB, tempLaserAABB).isHit) {
            // プレイヤーに当たった場合、レーザーの長さを「プレイヤーの手前」までに短縮する
            hitPlayer = true;
            
            // プレイヤーとの距離（中心座標間の差）
            float distToPlayer = 0.0f;
            if (dx != 0) {
                distToPlayer = std::abs(playerAABB.center.x - position_.x) - (playerAABB.size.x / 2.0f) - 0.5f;
            } else if (dy != 0) {
                distToPlayer = std::abs(playerAABB.center.y - position_.y) - (playerAABB.size.y / 2.0f) - 0.5f;
            }

            // 最低でも0より小さくならないようにする
            floatDist = (std::max)(0.0f, distToPlayer);

            // AABBを再計算
            if (dx != 0) {
                laserSize.x = floatDist;
                laserCenter.x = position_.x + (floatDist / 2.0f) * dx + 0.5f * dx;
            } else if (dy != 0) {
                laserSize.y = floatDist;
                laserCenter.y = position_.y + (floatDist / 2.0f) * dy + 0.5f * dy;
            }
        }
    }

    // 最終的なレーザーの長さとAABBを保持
    currentLaserLength_ = floatDist;
    laserAABB_.center = laserCenter;
    laserAABB_.size = laserSize;

    // ビーム描画用モデル（beamObject_）の更新処理は不要になりました
    // 代わりに LaserBeamRenderer を使用して Draw() で描画します

    // --- Step 5: プレイヤーとの接触イベント処理 ---
    if (hitPlayer) {
        if (!wasPlayerColliding_) {
            Logger::Log(std::format("[LaserGimmick] Player hit by laser at ({}, {}, {})\n",
                                    position_.x, position_.y, position_.z));
        }
        playerHitTime_ += TimeManager::GetInstance()->GetUnscaledDeltaTime();
        if (playerHitTime_ >= kPlayerDeathDelay) {
            player->Kill();
            playerHitTime_ = 0.0f;
        }
        wasPlayerColliding_ = true;
    } else {
        wasPlayerColliding_ = false;
        playerHitTime_ = 0.0f;
    }
}

void LaserGimmick::Draw()
{
    if (emitterObject_) {
        emitterObject_->Draw();
    }
    
    // レーザーの長さがある場合のみビームを描画
    if (staticLaserLength_ > 0.01f) {
        int dx = 0;
        int dy = 0;
        if (param_->direction_ == 0) dy = 1;
        if (param_->direction_ == 1) dy = -1;
        if (param_->direction_ == 2) dx = -1;
        if (param_->direction_ == 3) dx = 1;

        float visualLaserLength = staticLaserLength_;

        // --- 描画タイミングでの最新のプレイヤー位置を用いて長さを切り詰める ---
        if (stage_) {
            MapChipPlayer* player = stage_->GetPlayer();
            if (player) {
                AABB playerAABB = player->GetAABB();
                
                Vector3 laserCenter = position_;
                Vector3 laserSize = { kLaserThickness, kLaserThickness, kLaserThickness };
                if (dx != 0) {
                    laserSize.x = visualLaserLength;
                    laserCenter.x += (visualLaserLength / 2.0f) * dx + 0.5f * dx; 
                } else if (dy != 0) {
                    laserSize.y = visualLaserLength;
                    laserCenter.y += (visualLaserLength / 2.0f) * dy + 0.5f * dy;
                }
                
                AABB tempLaserAABB;
                tempLaserAABB.center = laserCenter;
                tempLaserAABB.size = laserSize;

                if (CollisionManager::Intersect(playerAABB, tempLaserAABB).isHit) {
                    float distToPlayer = 0.0f;
                    if (dx != 0) {
                        distToPlayer = std::abs(playerAABB.center.x - position_.x) - (playerAABB.size.x / 2.0f) - 0.5f;
                    } else if (dy != 0) {
                        distToPlayer = std::abs(playerAABB.center.y - position_.y) - (playerAABB.size.y / 2.0f) - 0.5f;
                    }
                    visualLaserLength = (std::max)(0.0f, distToPlayer);
                }
            }
        }

        // 発射口の位置（0.5マス分オフセット）
        Vector3 startPos = position_;
        startPos.x += 0.5f * dx;
        startPos.y += 0.5f * dy;

        // 先端の位置
        Vector3 endPos = startPos;
        endPos.x += visualLaserLength * dx;
        endPos.y += visualLaserLength * dy;

        LaserBeamRenderer::GetInstance()->Draw(startPos, endPos, kLaserThickness, beamParams_);
    }
}

AABB LaserGimmick::GetAABB() const
{
    // 本体（発射機）のAABB
    AABB aabb;
    aabb.center = position_;
    aabb.size = { 1.0f, 1.0f, 1.0f };
    return aabb;
}
