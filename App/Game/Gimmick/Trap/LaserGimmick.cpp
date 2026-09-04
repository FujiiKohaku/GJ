#include "LaserGimmick.h"
#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Logger/Logger.h"
#include <algorithm>
#include <format>
#include <numbers>

LaserGimmick::LaserGimmick()
{
}

bool LaserGimmick::Initialize(
    const Vector3& position,
    const std::string& /*texturePath*/, // 外部からのテクスチャパスは無視
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
    
    std::string emitterModel = "LaserEmitter/LaserEmitter.obj";
    ModelManager::GetInstance()->Load(emitterModel);
    emitterObject_->SetModel(emitterModel);
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

    // --- レーザービーム(Cube)の初期化 ---
    beamObject_ = std::make_unique<Object3d>();
    beamObject_->Initialize(Object3dManager::GetInstance());
    
    // レーザービームを赤いCubeとして描画
    std::string beamTexture = "resources/Textures/white.png"; // 白いテクスチャ(エンジンでプリロード済み)
    Model* beamModel = ModelManager::GetInstance()->CreateCube(beamTexture);
    beamObject_->SetModel(beamModel);
    beamObject_->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // 赤色に設定
    beamObject_->SetEnableLighting(true);
    // レーザーを赤っぽくする場合は Material を弄るなど。ここでは一旦そのまま使用。

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

    // --- Step 2: レーザービームのAABBを仮作成 ---
    // 発射口（本体の中心から少し前）から、障害物の手前までの長さ
    float laserThickness = 0.2f;
    Vector3 laserCenter = position_;
    Vector3 laserSize = { laserThickness, laserThickness, laserThickness };

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

    // --- Step 4: ビームの描画（Object3d）のスケールと位置を更新 ---
    beamObject_->SetTranslate(laserCenter);
    // スケールは size と一致させる（Cube は元サイズが 1.0x1.0x1.0 を想定）
    beamObject_->SetScale(laserSize);
    beamObject_->Update();

    // --- Step 5: プレイヤーとの接触イベント処理 ---
    if (hitPlayer) {
        if (!wasPlayerColliding_) {
            Logger::Log(std::format("[LaserGimmick] Player hit by laser at ({}, {}, {})\n",
                                    position_.x, position_.y, position_.z));
            
            // =====================================================================================
            // TODO: ここにチームメンバーが「プレイヤーへのダメージ」や「死体のスポーン」処理を実装する
            // =====================================================================================
            // 例: player->TakeDamage(1);
            // =====================================================================================
        }
        wasPlayerColliding_ = true;
    } else {
        wasPlayerColliding_ = false;
    }
}

void LaserGimmick::Draw()
{
    if (emitterObject_) {
        emitterObject_->Draw();
    }
    
    // レーザーの長さがある場合のみビームを描画
    if (beamObject_ && currentLaserLength_ > 0.01f) {
        beamObject_->Draw();
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
