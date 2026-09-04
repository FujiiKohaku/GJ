#include "MapChipPlayer.h"
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "Engine/Logger/Logger.h"
#include "App/Scene/SceneManager.h"
#include "App/Scene/ArchiveScene.h"
#include "App/Scene/ClearScene.h"

#include "App/Game/Map/MapChipField.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kPlayerSize = 1.0f;
constexpr float kMoveSpeed = 5.0f;
constexpr float kJumpSpeed = 8.0f;
constexpr float kGravity = -20.0f;
constexpr float kMaximumDeltaTime = 1.0f / 30.0f;
constexpr float kCollisionEpsilon = 0.0001f;
constexpr float kPlayerHalfHeight = kPlayerSize * 0.5f;
constexpr float kSlimeFluidGroundClearance = 0.14f;
constexpr float kSlimeCoreLift = 0.10f;
constexpr float kSlimeCeilingVisualPadding = 0.24f;
constexpr float kSlimeMinimumVisualHeight = 0.12f;
constexpr float kSlimeCeilingFollowSpeed = 3.0f;
constexpr Vector3 kSlimeBaseRadii = { 0.50f, 0.50f, 0.50f };

float Saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float MoveTowards(float current, float target, float maxDelta)
{
    const float delta = target - current;
    if (std::abs(delta) <= maxDelta) {
        return target;
    }
    return current + (delta > 0.0f ? maxDelta : -maxDelta);
}

void GetGridBounds(const Vector3& center, float extendX, float extendY, uint32_t width, uint32_t height, int32_t& startX, int32_t& endX, int32_t& startY, int32_t& endY) {
    if (width == 0 || height == 0) {
        startX = endX = startY = endY = 0;
        return;
    }
    float minX = center.x - extendX;
    float maxX = center.x + extendX;
    float minY = center.y - extendY;
    float maxY = center.y + extendY;

    int32_t minGridX = static_cast<int32_t>(std::floor(minX + 0.5f));
    int32_t maxGridX = static_cast<int32_t>(std::floor(maxX + 0.5f));
    int32_t yValMin = static_cast<int32_t>(std::floor(minY + 0.5f));
    int32_t yValMax = static_cast<int32_t>(std::floor(maxY + 0.5f));

    int32_t startYIndex = static_cast<int32_t>(height) - 1 - yValMax;
    int32_t endYIndex = static_cast<int32_t>(height) - 1 - yValMin;

    startX = (std::max)(0, minGridX);
    endX = (std::min)(static_cast<int32_t>(width) - 1, maxGridX);
    startY = (std::max)(0, startYIndex);
    endY = (std::min)(static_cast<int32_t>(height) - 1, endYIndex);
}
}

MapChipPlayer::~MapChipPlayer() = default;

void MapChipPlayer::Initialize(const MapChipField* mapChipField, const Vector3& startPosition)
{
    position_ = startPosition;
    mapChipField_ = mapChipField;
    baseScale_ = kSlimeBaseRadii;
    visualScale_ = kSlimeBaseRadii;
    fluidFloorHeight_ = position_.y - kPlayerHalfHeight + kSlimeFluidGroundClearance;
}

void MapChipPlayer::Update(const std::vector<BaseMapChipGimmick*>& dynamicGimmicks)
{
    if (!mapChipField_) {
        return;
    }

    isCrushed_ = false;

    // フェーズ1: Base Movement (足場の追従)
    if (baseGimmick_) {
        Vector3 delta = baseGimmick_->GetDeltaPosition();
        position_.x += delta.x;
        position_.y += delta.y;
        position_.z += delta.z;
        
        Vector3 checkPos = position_;
        if (ResolveHorizontalCollision(checkPos) || ResolveVerticalCollision(checkPos)) {
            Logger::Log("Crush! (Player squished between moving block and wall)\n");
            position_ = checkPos;
            isCrushed_ = true;
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
    UpdateVerticalConfinement(dynamicGimmicks);

    if (std::abs(velocity_.x) > 0.1f) {
        forward_ = { velocity_.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f };
    }

    UpdateVisualShape(deltaTime);
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

bool MapChipPlayer::IsCrushed() const
{
    return isCrushed_;
}

AABB MapChipPlayer::GetAABB() const
{
    return {
        position_,
        { kPlayerSize, kPlayerSize, 0.2f }
    };
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

    int32_t startX, endX, startY, endY;
    GetGridBounds(nextPosition, kPlayerHalfHeight + 1.0f, kPlayerHalfHeight + 1.0f, width, height, startX, endX, startY, endY);

    for (int32_t yIndex = startY; yIndex <= endY; ++yIndex) {
        for (int32_t xIndex = startX; xIndex <= endX; ++xIndex) {
            if (!MapChipRegistry::IsSolidBlock(mapChipField_->GetMapChipTypeByIndex(xIndex, yIndex))) {
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

    int32_t startX, endX, startY, endY;
    GetGridBounds(nextPosition, kPlayerHalfHeight + 1.0f, kPlayerHalfHeight + 1.0f, width, height, startX, endX, startY, endY);

    for (int32_t yIndex = startY; yIndex <= endY; ++yIndex) {
        for (int32_t xIndex = startX; xIndex <= endX; ++xIndex) {
            if (!MapChipRegistry::IsSolidBlock(mapChipField_->GetMapChipTypeByIndex(xIndex, yIndex))) {
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
                } else if (hit.normal.y > 0.0f) {
                    ceilingSquash_ = (std::max)(ceilingSquash_, 0.35f);
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
        // ゴール判定
        if (gimmick->IsGoal()) {
            Logger::Log("Goal reached\n");
            // クリアシーンへ遷移
            SceneManager::GetInstance()->SetNextScene(std::make_unique<ClearScene>());
            return true;
        }
        
        // 物理的な衝突（壁や床としての機能）を持たないギミックは押し出し判定をスキップする
        if (!gimmick->IsSolid()) {
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
                } else if (hit.normal.y > 0.0f) {
                    ceilingSquash_ = (std::max)(ceilingSquash_, 0.35f);
                }
            }
        }
        
        playerBox.center = nextPosition; // 連続する衝突のため更新
    }
    
    return resolved;
}

const Vector3& MapChipPlayer::GetVelocity() const { return velocity_; }
const Vector3& MapChipPlayer::GetForward() const { return forward_; }
const Vector3& MapChipPlayer::GetVisualScale() const { return visualScale_; }

Vector3 MapChipPlayer::GetFluidCorePosition() const
{
    return position_ + Vector3{0.0f, kSlimeCoreLift, 0.0f};
}

float MapChipPlayer::GetFluidFloorHeight() const
{
    return fluidFloorHeight_;
}

float MapChipPlayer::GetFluidCeilingHeight() const
{
    return fluidCeilingHeight_;
}

void MapChipPlayer::GetWallBoundaries(float& outMinX, float& outMaxX, float& outMaxY, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks) const
{
    outMinX = -1000.0f;
    outMaxX = 1000.0f;
    outMaxY = fluidCeilingHeight_;
    
    const float pMinY = position_.y - kPlayerHalfHeight + 0.1f;
    const float pMaxY = position_.y + kPlayerHalfHeight - 0.1f;
    const float pMinX = position_.x - kPlayerHalfHeight + 0.1f;
    const float pMaxX = position_.x + kPlayerHalfHeight - 0.1f;

    auto checkBoundary = [&](float bMinX, float bMaxX, float bMinY, float bMaxY) {
        if (bMinY < pMaxY && bMaxY > pMinY) {
            if (bMaxX <= position_.x && bMaxX > outMinX) outMinX = bMaxX;
            if (bMinX >= position_.x && bMinX < outMaxX) outMaxX = bMinX;
        }
        if (bMinX < pMaxX && bMaxX > pMinX) {
            if (bMinY >= position_.y && bMinY < outMaxY) outMaxY = bMinY;
        }
    };

    if (mapChipField_) {
        const uint32_t width = mapChipField_->GetBlockWidth();
        const uint32_t height = mapChipField_->GetBlockHeight();

        int32_t startX, endX, startY, endY;
        GetGridBounds(position_, kPlayerHalfHeight + 1.0f, fluidCeilingHeight_ + 1.0f, width, height, startX, endX, startY, endY);

        for (int32_t y = startY; y <= endY; ++y) {
            for (int32_t x = startX; x <= endX; ++x) {
                if (!MapChipRegistry::IsSolidBlock(mapChipField_->GetMapChipTypeByIndex(x, y))) continue;
                
                Vector3 blockPos = mapChipField_->GetMapChipPositionByIndex(x, y);
                checkBoundary(blockPos.x - 0.5f, blockPos.x + 0.5f, blockPos.y - 0.5f, blockPos.y + 0.5f);
            }
        }
    }

    for (BaseMapChipGimmick* gimmick : dynamicGimmicks) {
        if (!gimmick->IsSolid()) continue;
        AABB box = gimmick->GetAABB();
        checkBoundary(box.center.x - box.size.x * 0.5f, box.center.x + box.size.x * 0.5f,
                      box.center.y - box.size.y * 0.5f, box.center.y + box.size.y * 0.5f);
    }
}

void MapChipPlayer::UpdateVerticalConfinement(const std::vector<BaseMapChipGimmick*>& dynamicGimmicks)
{
    float floorTop = -1000.0f;
    float ceilingBottom = 1000.0f;

    const float pMinX = position_.x - kPlayerHalfHeight + 0.08f;
    const float pMaxX = position_.x + kPlayerHalfHeight - 0.08f;

    auto checkObstacle = [&](float bMinX, float bMaxX, float bMinY, float bMaxY) {
        if (bMaxX <= pMinX || bMinX >= pMaxX) {
            return;
        }
        if (bMaxY <= position_.y && bMaxY > floorTop) {
            floorTop = bMaxY;
        }
        if (bMinY >= position_.y && bMinY < ceilingBottom) {
            ceilingBottom = bMinY;
        }
    };

    const uint32_t width = mapChipField_->GetBlockWidth();
    const uint32_t height = mapChipField_->GetBlockHeight();
    
    int32_t startX, endX, startY, endY;
    GetGridBounds(position_, kPlayerHalfHeight + 1.0f, 5.0f, width, height, startX, endX, startY, endY);
    
    for (int32_t y = startY; y <= endY; ++y) {
        for (int32_t x = startX; x <= endX; ++x) {
            if (!MapChipRegistry::IsSolidBlock(mapChipField_->GetMapChipTypeByIndex(x, y))) {
                continue;
            }
            const Vector3 blockPos = mapChipField_->GetMapChipPositionByIndex(x, y);
            checkObstacle(
                blockPos.x - 0.5f,
                blockPos.x + 0.5f,
                blockPos.y - 0.5f,
                blockPos.y + 0.5f);
        }
    }

    for (BaseMapChipGimmick* gimmick : dynamicGimmicks) {
        if (!gimmick->IsSolid()) continue;
        const AABB box = gimmick->GetAABB();
        checkObstacle(
            box.center.x - box.size.x * 0.5f,
            box.center.x + box.size.x * 0.5f,
            box.center.y - box.size.y * 0.5f,
            box.center.y + box.size.y * 0.5f);
    }

    fluidCeilingHeight_ = ceilingBottom;
    fluidFloorHeight_ =
        floorTop > -999.0f
            ? floorTop
            : position_.y - kPlayerHalfHeight;
    verticalCompression01_ = 0.0f;
    if (floorTop > -999.0f && ceilingBottom < 999.0f) {
        const float gap = ceilingBottom - floorTop;
        verticalCompression01_ = Saturate((kPlayerSize - gap) / kPlayerSize);
        if (gap <= kPlayerSize - kCollisionEpsilon) {
            isCrushed_ = true;
        }
    }
}

void MapChipPlayer::UpdateVisualShape(float deltaTime)
{
    time_ += deltaTime;

    const float horizontalSpeed01 = Saturate(std::abs(velocity_.x) / kMoveSpeed);

    wobble_ = std::sin(time_ * 15.0f) * 0.1f * horizontalSpeed01;

    wallSquash_ = (std::max)(0.0f, wallSquash_ - deltaTime * 5.0f);
    landSquash_ = (std::max)(0.0f, landSquash_ - deltaTime * 5.0f);
    ceilingSquash_ = (std::max)(0.0f, ceilingSquash_ - deltaTime * 5.0f);

    const float speedStretch = horizontalSpeed01 * 0.15f;
    const float compressedHeight =
        fluidCeilingHeight_ < 999.0f
            ? fluidCeilingHeight_ - GetFluidCorePosition().y - kSlimeCeilingVisualPadding
            : baseScale_.y;
    const float ceilingLimitedHeight =
        std::clamp(compressedHeight, kSlimeMinimumVisualHeight, baseScale_.y);
    const float desiredHeight =
        ceilingLimitedHeight - ceilingSquash_ * 0.08f - verticalCompression01_ * 0.10f;

    visualScale_.x = baseScale_.x - (wallSquash_ * 0.2f) + (landSquash_ * 0.2f) + wobble_ + speedStretch;
    visualScale_.y = std::clamp(desiredHeight, kSlimeMinimumVisualHeight, baseScale_.y);
    visualScale_.z = baseScale_.z;
}
