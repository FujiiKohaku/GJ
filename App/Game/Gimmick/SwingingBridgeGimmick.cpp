#include "SwingingBridgeGimmick.h"
#include "SwingingBridgeParam.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Math/MathStruct.h"
#include "App/Game/Map/MapChipStage.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Logger/Logger.h"
#include <cmath>
#include <format>
#include <numbers>

bool SwingingBridgeGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    basePosition_ = position;
    
    if (gimmickParam) {
        const SwingingBridgeParam* param = dynamic_cast<const SwingingBridgeParam*>(gimmickParam);
        if (param) {
            length_ = param->length_;
            swingRange_ = param->swingRange_;
            speed_ = param->speed_;
            phase_ = param->phase_;
        }
    }
    
    currentPosition_ = basePosition_;
    previousPosition_ = basePosition_;

    // 足場モデルの初期化
    std::string platformFile = "SwingingBridge/SwingingBridgePlatform.obj";
    Model* platformModel = ModelManager::GetInstance()->Load(platformFile);
    if (!platformModel) {
        // フォールバックとしてキューブを使用
        platformModel = ModelManager::GetInstance()->CreateCube("resources/Textures/white1x1.png");
    }

    if (platformModel) {
        platformObject_ = std::make_unique<Object3d>();
        platformObject_->Initialize(Object3dManager::GetInstance());
        platformObject_->SetModel(platformModel);
        platformObject_->SetTranslate(basePosition_);
        platformObject_->SetEnableLighting(true);
        platformObject_->Update();
    }

    // --- 紐モデルの初期化 ---
    Model* chainModel = ModelManager::GetInstance()->Load("SwingingBridge/SwingingBridgeChain.obj");
    if (!chainModel) {
        // 紐モデルがない場合は仮のキューブを細長くして使用
        chainModel = ModelManager::GetInstance()->CreateCube("resources/Textures/white1x1.png");
    }
    
    if (chainModel) {
        chainObject_ = std::make_unique<Object3d>();
        chainObject_->Initialize(Object3dManager::GetInstance());
        chainObject_->SetModel(chainModel);
        // 紐の長さ（Yスケール）を合わせる
        chainObject_->SetScale({ 0.1f, length_, 0.1f }); 
        chainObject_->SetEnableLighting(true);
        chainObject_->Update();
    }

    return platformObject_ != nullptr;
}

void SwingingBridgeGimmick::Update()
{
    if (!platformObject_) return;

    previousPosition_ = currentPosition_;

    auto input = Input::GetInstance();
    
    // TODO: チームメンバーが死体処理を追加する場所
    // （ここで、プレイヤーの死体があるか、または死体がくっついた橋と接触しているかを判定し、
    //   isStuck_ フラグを true にする処理を後日追加してください。）
    
    // 【デバッグ機能】Kキーを押すと強制的にスタック（くっつき）状態になる
    // これは「死体がのりになる」という前提（フラグ）が成立したことを意味する
    if (input->IsKeyTrigger(DIK_K)) {
        if (!isStuck_) {
            Logger::Log("[SwingingBridge] Debug: K key pressed. Ready to stick on next collision!\n");
        }
        isStuck_ = true;
    }

    if (isPermanentlyStuck_) {
        // 壁などにぶつかって完全に固定された場合は一切移動しない
        // (何もしない)
    } else {
        float twoPi = 2.0f * std::numbers::pi_v<float>;
        
        // 次のフレームの時間を計算（timeDirection_ で正逆どちらに進むかが決まる）
        float nextElapsedTime = elapsedTime_ + TimeManager::GetInstance()->GetDeltaTime() * timeDirection_;
        
        // --- 仮想的に次のフレームの座標を計算 ---
        float nextAnglePhase = (nextElapsedTime * speed_) + (phase_ * twoPi);
        float nextSwingAmount = std::sin(nextAnglePhase);
        float nextDistX = nextSwingAmount * static_cast<float>(swingRange_); 
        float nextClampedX = std::clamp(nextDistX, -length_ * 0.99f, length_ * 0.99f);
        float nextRelativeY = -std::sqrt(length_ * length_ - nextClampedX * nextClampedX);
        float nextOffsetY = nextRelativeY + length_;
        
        Vector3 nextPosition = basePosition_;
        nextPosition.x += nextClampedX;
        nextPosition.y += nextOffsetY;
        
        // トンネリング（すり抜け）防止のための分割チェック (Continuous Collision Detection)
        bool isHit = false;
        if (!isEditorMode_) {
            float distX = nextPosition.x - currentPosition_.x;
            float distY = nextPosition.y - currentPosition_.y;
            float distSq = distX * distX + distY * distY;
            
            // 0.4単位ごとに分割してチェック（ブロックサイズ1.0に対して十分細かく）
            int steps = static_cast<int>(std::sqrt(distSq) / 0.4f) + 1;
            
            for (int i = 1; i <= steps; ++i) {
                float t = static_cast<float>(i) / steps;
                Vector3 checkPos = {
                    currentPosition_.x + distX * t,
                    currentPosition_.y + distY * t,
                    currentPosition_.z
                };
                
                AABB checkAABB = GetAABB();
                checkAABB.center = checkPos;
                
                SwingingBridgeGimmick* hitBridge = nullptr;
                if (CheckCollision(checkAABB, &hitBridge)) {
                    isHit = true;
                    // ぶつかった瞬間の時間を計算してめり込みを防ぐ
                    float safeT = static_cast<float>(i - 1) / steps; // 1つ前の安全な位置の割合
                    
                    if (isStuck_) {
                        // のりが付いている場合は完全にくっついて停止する
                        isPermanentlyStuck_ = true;
                        if (hitBridge) {
                            hitBridge->ForceStuck();
                        }
                        // 衝突した瞬間の時間に固定する
                        elapsedTime_ = elapsedTime_ + (nextElapsedTime - elapsedTime_) * safeT;
                    } else {
                        // のりが付いていない場合ははね返る
                        timeDirection_ *= -1.0f;
                        if (hitBridge) {
                            hitBridge->ForceBounce();
                        }
                        // 衝突した瞬間の時間に進める（次フレームから逆方向に進む）
                        elapsedTime_ = elapsedTime_ + (nextElapsedTime - elapsedTime_) * safeT;
                    }
                    break;
                }
            }
        }
        
        if (!isHit && !isEditorMode_) {
            // 衝突しなかったのでそのまま時間を進める
            elapsedTime_ = nextElapsedTime;
        }
    }

    // 確定した時間で最終的な座標を計算（衝突時は前回の座標のまま、はね返り時は進まない等）
    float twoPi = 2.0f * std::numbers::pi_v<float>;
    float finalAnglePhase = (elapsedTime_ * speed_) + (phase_ * twoPi);
    float swingAmount = std::sin(finalAnglePhase);

    // 最大振幅のX距離
    float maxDistX = static_cast<float>(swingRange_); 
    float currentOffsetX = swingAmount * maxDistX;

    // Y座標のオフセットを計算 (ピタゴラスの定理: x^2 + y^2 = length^2 を利用)
    // ただし振幅がlengthを超えるような設定値だと計算が破綻するので、clampする
    float clampedOffsetX = std::clamp(currentOffsetX, -length_ * 0.99f, length_ * 0.99f);
    // 支点(0, length, 0) からの相対位置
    float relativeY = -std::sqrt(length_ * length_ - clampedOffsetX * clampedOffsetX);
    
    // 足場は回転させず（AABB維持）、円弧上の位置に平行移動(Translate)する
    // ( relativeY は負の値で出てくるが、一番下が -length。
    // basePositionを一番下（通常状態）としたいので、lengthを足してオフセット化する )
    float currentOffsetY = relativeY + length_;

    currentPosition_ = basePosition_;
    currentPosition_.x += clampedOffsetX;
    currentPosition_.y += currentOffsetY;

    platformObject_->SetTranslate(currentPosition_);
    platformObject_->Update();

    // 紐は支点から回転・配置させる
    if (chainObject_) {
        // 天井ブロックの底面を紐の固定位置（上端）とする
        Vector3 chainTop = basePosition_;
        chainTop.y += (length_ - 0.5f);
        
        // 足場ブロックの上面を紐の接続位置（下端）とする
        Vector3 chainBottom = currentPosition_;
        chainBottom.y += 0.5f;
        
        // 紐の中心位置を計算（モデルのピボットが中心にあるため）
        Vector3 chainPos = {
            (chainTop.x + chainBottom.x) * 0.5f,
            (chainTop.y + chainBottom.y) * 0.5f,
            (chainTop.z + chainBottom.z) * 0.5f
        };
        chainObject_->SetTranslate(chainPos);
        
        // 実際の紐の長さを計算してスケールを合わせる
        float dx = chainBottom.x - chainTop.x;
        float dy = chainBottom.y - chainTop.y;
        float actualLength = std::sqrt(dx * dx + dy * dy);
        
        // 紐のスケールを適用
        chainObject_->SetScale({ 0.1f, actualLength, 0.1f });
        
        // 回転角を計算。
        // DX左手系でのZ軸回転：未回転時はY軸方向(0,1,0)を向いているモデルを、
        // chainBottom から chainTop に向かうベクトルの方向に向ける。
        float zRot = std::atan2(-dx, -dy);
        
        chainObject_->SetRotate({ 0.0f, 0.0f, zRot });
        chainObject_->Update();
    }
}

void SwingingBridgeGimmick::Draw()
{
    if (platformObject_) {
        platformObject_->Draw();
    }
    if (chainObject_) {
        chainObject_->Draw();
    }
}

AABB SwingingBridgeGimmick::GetAABB() const
{
    AABB aabb;
    // モデルの見た目に合わせて、幅(X)を3.0に変更
    aabb.center = currentPosition_;
    aabb.size = { 3.0f, 1.0f, 1.0f };
    return aabb;
}

Vector3 SwingingBridgeGimmick::GetDeltaPosition() const
{
    return {
        currentPosition_.x - previousPosition_.x,
        currentPosition_.y - previousPosition_.y,
        currentPosition_.z - previousPosition_.z
    };
}

bool SwingingBridgeGimmick::CheckCollision(const AABB& aabb, SwingingBridgeGimmick** outHitBridge) const
{
    if (!stage_) {
        Logger::Log("[SwingingBridge] stage_ is null! CheckCollision aborted.\n");
        return false;
    }

    // デバッグ用に数フレームに1回だけログを出すために static カウンタを使う
    static int logCount = 0;
    bool shouldLog = (logCount++ % 60 == 0); // 60回（約1秒）に1回ログを出す
    
    if (shouldLog) {
        Logger::Log(std::format("[SwingingBridge] Checking Collision for AABB Center({:.2f}, {:.2f}, {:.2f})\n", aabb.center.x, aabb.center.y, aabb.center.z));
    }

    // 1. マップチップ（壁・破壊可能壁）との衝突判定
    const MapChipField& field = stage_->GetField();
    uint32_t width = field.GetBlockWidth();
    uint32_t height = field.GetBlockHeight();

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            MapChipType type = field.GetMapChipTypeByIndex(x, y);
            if (type == MapChipType::Block || type == MapChipType::DestructibleWall) {
                Vector3 blockPos = field.GetMapChipPositionByIndex(x, y);
                AABB blockAABB;
                blockAABB.center = blockPos;
                blockAABB.size = { 1.0f, 1.0f, 1.0f };

                if (shouldLog) {
                    float dist = std::sqrt(std::pow(aabb.center.x - blockAABB.center.x, 2) + std::pow(aabb.center.y - blockAABB.center.y, 2));
                    if (dist < 3.0f) { // 近くのブロックだけログ
                        Logger::Log(std::format("  -> vs Block Center({:.2f}, {:.2f}, {:.2f})\n", blockAABB.center.x, blockAABB.center.y, blockAABB.center.z));
                    }
                }

                if (CollisionManager::Intersect(aabb, blockAABB).isHit) {
                    Logger::Log(std::format("[SwingingBridge] Hit Block at ({:.2f}, {:.2f})\n", blockPos.x, blockPos.y));
                    return true;
                }
            }
        }
    }

    // 2. 他の SwingingBridge との衝突判定
    for (BaseMapChipGimmick* other : stage_->GetGimmicks()) {
        if (other == this) continue;
        
        SwingingBridgeGimmick* otherBridge = dynamic_cast<SwingingBridgeGimmick*>(other);
        SwingingBridgeGimmick* bridge = dynamic_cast<SwingingBridgeGimmick*>(other);
        if (bridge) {
            AABB otherAABB = bridge->GetAABB();
            
            if (shouldLog) {
                Logger::Log(std::format("  -> vs Other Bridge Center({:.2f}, {:.2f}, {:.2f})\n", otherAABB.center.x, otherAABB.center.y, otherAABB.center.z));
            }
            
            if (CollisionManager::Intersect(aabb, otherAABB).isHit) {
                Logger::Log(std::format("[SwingingBridge] Hit another SwingingBridge at ({:.2f}, {:.2f})\n", otherAABB.center.x, otherAABB.center.y));
                if (outHitBridge) {
                    *outHitBridge = bridge;
                }
                return true;
            }
        }
    }

    return false;
}
