#include "MapChipPlayer.h"
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "Engine/Logger/Logger.h"

#include "App/Game/Map/MapChipField.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>

namespace {
constexpr float kPlayerSize = 1.0f;
constexpr float kMoveSpeed = 5.0f;
constexpr float kJumpSpeed = 8.0f;
constexpr float kGravity = -20.0f;
constexpr float kMaximumDeltaTime = 1.0f / 30.0f;
constexpr float kCollisionEpsilon = 0.0001f;
}

MapChipPlayer::~MapChipPlayer() = default;

void MapChipPlayer::Initialize(Model* model, const MapChipField* mapChipField, const Vector3& startPosition)
{
    position_ = startPosition;
    mapChipField_ = mapChipField;
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);
    object_->SetScale({ kPlayerSize, kPlayerSize, 1.0f });
    object_->SetTranslate(position_);
    object_->SetColor({ 0.1f, 0.65f, 1.0f, 1.0f });
    object_->SetEnableLighting(false);
    object_->Update();
}

void MapChipPlayer::Update(const std::vector<BaseMapChipGimmick*>& dynamicGimmicks)
{
    if (!object_ || !mapChipField_) {
        return;
    }

    // フェーズ1: Base Movement (足場の追従)
    if (baseGimmick_) {
        Vector3 delta = baseGimmick_->GetDeltaPosition();
        position_.x += delta.x;
        position_.y += delta.y;
        position_.z += delta.z;
        
        // 追従後に壁にめり込んでいるかチェックし、Crush判定（Cプラン: ログ出力のみ）
        Vector3 checkPos = position_;
        if (ResolveHorizontalCollision(checkPos) || ResolveVerticalCollision(checkPos)) {
            Logger::Log("Crush! (Player squished between moving block and wall)\n");
            // Cプラン: ゲーム進行を止めないため、ログのみで実際には壁から押し出された位置を適用する
            position_ = checkPos;
        }
    }

    float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    deltaTime = (std::min)(deltaTime, kMaximumDeltaTime);
    if (deltaTime < 0.0f) {
        deltaTime = 0.0f;
    }

    Input* input = Input::GetInstance();
    velocity_.x = 0.0f;
    if (input->IsKeyPressed(DIK_A) || input->IsKeyPressed(DIK_LEFT)) {
        velocity_.x -= kMoveSpeed;
    }
    if (input->IsKeyPressed(DIK_D) || input->IsKeyPressed(DIK_RIGHT)) {
        velocity_.x += kMoveSpeed;
    }

    if (isGrounded_) {
        const bool jumpRequested =
            input->IsKeyTrigger(DIK_SPACE) ||
            input->IsKeyTrigger(DIK_W) ||
            input->IsKeyTrigger(DIK_UP);
        if (jumpRequested) {
            velocity_.y = kJumpSpeed;
            isGrounded_ = false;
        }
    }

    isColliding_ = false;
    baseGimmick_ = nullptr; // 毎フレーム着地判定で更新するためクリア
    
    MoveHorizontal(deltaTime, dynamicGimmicks);
    MoveVertical(deltaTime, dynamicGimmicks);

    object_->SetTranslate(position_);
    object_->Update();
}

void MapChipPlayer::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

const Vector3& MapChipPlayer::GetPosition() const
{
    return position_;
}

bool MapChipPlayer::IsGrounded() const
{
    return isGrounded_;
}

bool MapChipPlayer::IsColliding() const
{
    return isColliding_;
}

void MapChipPlayer::MoveHorizontal(float deltaTime, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks)
{
    Vector3 nextPosition = position_;
    nextPosition.x += velocity_.x * deltaTime;
    
    if (ResolveDynamicCollision(nextPosition, dynamicGimmicks, true)) {
        velocity_.x = 0.0f;
        isColliding_ = true;
    }
    
    if (ResolveHorizontalCollision(nextPosition)) {
        velocity_.x = 0.0f;
        isColliding_ = true;
    }
    position_.x = nextPosition.x;
}

void MapChipPlayer::MoveVertical(float deltaTime, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks)
{
    velocity_.y += kGravity * deltaTime;
    Vector3 nextPosition = position_;
    nextPosition.y += velocity_.y * deltaTime;
    isGrounded_ = false;
    
    if (ResolveDynamicCollision(nextPosition, dynamicGimmicks, false)) {
        velocity_.y = 0.0f;
        isColliding_ = true;
    }
    
    if (ResolveVerticalCollision(nextPosition)) {
        velocity_.y = 0.0f;
        isColliding_ = true;
    }
    position_.y = nextPosition.y;
}

bool MapChipPlayer::ResolveHorizontalCollision(Vector3& nextPosition)
{
    bool resolved = false;
    const uint32_t height = mapChipField_->GetBlockHeight();
    const uint32_t width = mapChipField_->GetBlockWidth();

    for (uint32_t yIndex = 0; yIndex < height; ++yIndex) {
        for (uint32_t xIndex = 0; xIndex < width; ++xIndex) {
            if (mapChipField_->GetMapChipTypeByIndex(xIndex, yIndex) !=
                MapChipType::Block) {
                continue;
            }

            const Vector3 blockPosition =
                mapChipField_->GetMapChipPositionByIndex(xIndex, yIndex);
            AABB playerBox = {
                nextPosition,
                { kPlayerSize, kPlayerSize, 0.2f }
            };
            const AABB blockBox = {
                blockPosition,
                { 1.0f, 1.0f, 1.0f }
            };
            const CollisionHit hit =
                CollisionManager::Intersect(playerBox, blockBox);
            if (!hit.isHit || hit.penetration <= kCollisionEpsilon) {
                continue;
            }

            if (std::abs(hit.normal.x) > 0.0f) {
                nextPosition.x -= hit.normal.x * hit.penetration;
                resolved = true;
            }
            // 連続する衝突のため更新
            playerBox.center = nextPosition;
        }
    }
    return resolved;
}

bool MapChipPlayer::ResolveVerticalCollision(Vector3& nextPosition)
{
    bool resolved = false;
    const uint32_t height = mapChipField_->GetBlockHeight();
    const uint32_t width = mapChipField_->GetBlockWidth();

    for (uint32_t yIndex = 0; yIndex < height; ++yIndex) {
        for (uint32_t xIndex = 0; xIndex < width; ++xIndex) {
            if (mapChipField_->GetMapChipTypeByIndex(xIndex, yIndex) !=
                MapChipType::Block) {
                continue;
            }

            const Vector3 blockPosition =
                mapChipField_->GetMapChipPositionByIndex(xIndex, yIndex);
            AABB playerBox = {
                nextPosition,
                { kPlayerSize, kPlayerSize, 0.2f }
            };
            const AABB blockBox = {
                blockPosition,
                { 1.0f, 1.0f, 1.0f }
            };
            const CollisionHit hit =
                CollisionManager::Intersect(playerBox, blockBox);
            if (!hit.isHit || hit.penetration <= kCollisionEpsilon) {
                continue;
            }

            if (std::abs(hit.normal.y) > 0.0f) {
                nextPosition.y -= hit.normal.y * hit.penetration;
                resolved = true;
                if (hit.normal.y < 0.0f && velocity_.y <= 0.0f) {
                    isGrounded_ = true;
                }
            }
            // 連続する衝突のため更新
            playerBox.center = nextPosition;
        }
    }
    return resolved;
}

bool MapChipPlayer::ResolveDynamicCollision(Vector3& nextPosition, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks, bool isHorizontal)
{
    bool resolved = false;
    AABB playerBox = {
        nextPosition,
        { kPlayerSize, kPlayerSize, 0.2f }
    };
    
    for (BaseMapChipGimmick* gimmick : dynamicGimmicks) {
        AABB blockBox = gimmick->GetAABB();
        CollisionHit hit = CollisionManager::Intersect(playerBox, blockBox);
        if (!hit.isHit || hit.penetration <= kCollisionEpsilon) {
            continue;
        }
        
        if (isHorizontal) {
            if (std::abs(hit.normal.x) > 0.0f) {
                // ブロックの方向（hit.normal）の逆へ押し出す
                nextPosition.x -= hit.normal.x * hit.penetration;
                resolved = true;
            }
        } else {
            if (std::abs(hit.normal.y) > 0.0f) {
                nextPosition.y -= hit.normal.y * hit.penetration;
                resolved = true;
                
                // ブロックがプレイヤーの下にある（hit.normal.y が負）場合に着地判定
                if (hit.normal.y < 0.0f && velocity_.y <= 0.0f) {
                    isGrounded_ = true;
                    baseGimmick_ = gimmick;
                }
            }
        }
        
        playerBox.center = nextPosition; // 連続する衝突のため更新
    }
    
    return resolved;
}
