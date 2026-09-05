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
constexpr float kPlayerSize = 0.9f;
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
constexpr Vector3 kSlimeBaseRadii = { 0.45f, 0.45f, 0.45f };
constexpr float kMousePullSensitivity = 0.006f;
constexpr float kStickPullSpeed = 3.5f;
constexpr float kMaximumPull = 2.5f;
constexpr float kPullDeadZone = 0.06f;

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
    velocity_ = { 0.0f, 0.0f, 0.0f };
    isShapingSelfDestruct_ = false;
    hardenedBodyReady_ = false;
    deathRequested_ = false;
    baseGimmick_ = nullptr;
    isCrushed_ = false;
    isGrounded_ = false;
    wasGrounded_ = false;
    isColliding_ = false;
    verticalCompression01_ = 0.0f;
    wallSquash_ = landSquash_ = ceilingSquash_ = 0.0f;
    fluidCeilingHeight_ = 1000.0f;
    selfDestructRawPull_ = { 0.0f, 0.0f };
    selfDestructPull_ = { 0.0f, 0.0f };
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

    Input* input = Input::GetInstance();

    float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    deltaTime = (std::min)(deltaTime, kMaximumDeltaTime);
    if (deltaTime < 0.0f) {
        deltaTime = 0.0f;
    }

    if (input->IsKeyTrigger(DIK_T)) {
        if (isShapingSelfDestruct_) {
            hardenedBody_ = GetAABB();
            hardenedBodyReady_ = true;
            isShapingSelfDestruct_ = false;
            return;
        }

        isShapingSelfDestruct_ = true;
        selfDestructRawPull_ = { 0.0f, 0.0f };
        selfDestructPull_ = { 0.0f, 0.0f };
        velocity_ = { 0.0f, 0.0f, 0.0f };
    }

    if (isShapingSelfDestruct_) {
        UpdateSelfDestructShape(
            TimeManager::GetInstance()->GetUnscaledDeltaTime());
        return;
    }

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
    if (isShapingSelfDestruct_) {
        const float sizeX = kPlayerSize + std::abs(selfDestructPull_.x);
        const float sizeY = kPlayerSize + std::abs(selfDestructPull_.y);
        const float depthHalfSize = std::clamp(
            baseScale_.z / std::sqrt(sizeX * sizeY),
            0.22f,
            baseScale_.z);
        const Vector3 size = {
            sizeX,
            sizeY,
            depthHalfSize * 2.0f
        };
        return {
            position_ + Vector3{
                selfDestructPull_.x * 0.5f,
                selfDestructPull_.y * 0.5f,
                0.0f },
            size
        };
    }
    return {
        position_,
        { kPlayerSize, kPlayerSize, 0.2f }
    };
}

bool MapChipPlayer::ConsumeHardenedBody(AABB& outBody)
{
    if (!hardenedBodyReady_) {
        return false;
    }
    outBody = hardenedBody_;
    hardenedBodyReady_ = false;
    return true;
}

bool MapChipPlayer::ConsumeDeathRequest()
{
    if (!deathRequested_) {
        return false;
    }
    deathRequested_ = false;
    return true;
}

void MapChipPlayer::UpdateSelfDestructShape(float unscaledDeltaTime)
{
    Input* input = Input::GetInstance();
    // 自爆形状は左ドラッグ中だけつまんで伸ばす。
    if (input->IsMousePressed(0)) {
        selfDestructRawPull_.x +=
            static_cast<float>(input->GetMouseDeltaX()) * kMousePullSensitivity;
        selfDestructRawPull_.y -=
            static_cast<float>(input->GetMouseDeltaY()) * kMousePullSensitivity;
    }
    selfDestructRawPull_.x +=
        input->GetGamepadRightStickX() * kStickPullSpeed * unscaledDeltaTime;
    selfDestructRawPull_.y +=
        input->GetGamepadRightStickY() * kStickPullSpeed * unscaledDeltaTime;

    const float length = std::sqrt(
        selfDestructRawPull_.x * selfDestructRawPull_.x +
        selfDestructRawPull_.y * selfDestructRawPull_.y);
    if (length > kMaximumPull) {
        selfDestructRawPull_.x *= kMaximumPull / length;
        selfDestructRawPull_.y *= kMaximumPull / length;
    }

    selfDestructPull_ = selfDestructRawPull_;
    if (length < kPullDeadZone) {
        selfDestructPull_ = { 0.0f, 0.0f };
    } else {
        // 横壁・縦壁を簡単に作れるよう、主方向が明確なら副方向を滑らかに抑える。
        const float absoluteX = std::abs(selfDestructPull_.x);
        const float absoluteY = std::abs(selfDestructPull_.y);
        if (absoluteX > absoluteY && absoluteX > 0.0f) {
            const float dominance = absoluteX / (absoluteY + 0.0001f);
            const float assist = Saturate((dominance - 1.05f) / 0.75f);
            selfDestructPull_.y *= 1.0f - assist * 0.9f;
        } else if (absoluteY > 0.0f) {
            const float dominance = absoluteY / (absoluteX + 0.0001f);
            const float assist = Saturate((dominance - 1.05f) / 0.75f);
            selfDestructPull_.x *= 1.0f - assist * 0.9f;
        }
    }

    velocity_ = { 0.0f, 0.0f, 0.0f };
    const float sizeX = kPlayerSize + std::abs(selfDestructPull_.x);
    const float sizeY = kPlayerSize + std::abs(selfDestructPull_.y);
    visualScale_ = {
        sizeX * 0.5f,
        sizeY * 0.5f,
        std::clamp(
            baseScale_.z / std::sqrt(sizeX * sizeY),
            0.22f,
            baseScale_.z)
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
        if (gimmick->IsGoal()) {
            const CollisionHit goalHit =
                CollisionManager::Intersect(playerBox, gimmick->GetAABB());
            if (goalHit.isHit) {
                Logger::Log("Goal reached\n");
                SceneManager::GetInstance()->SetNextScene(std::make_unique<ClearScene>());
                return true;
            }
        }

        if (!gimmick->IsSolid()) {
            continue;
        }

        for (const AABB& blockBox : gimmick->GetCollisionBoxes()) {
            const CollisionHit hit = CollisionManager::Intersect(playerBox, blockBox);
            if (!hit.isHit || hit.penetration <= kCollisionEpsilon) {
                continue;
            }

            if (isHorizontal) {
                if (std::abs(hit.normal.x) > 0.0f) {
                    nextPosition.x -= hit.normal.x * hit.penetration;
                    resolved = true;
                }
            } else if (std::abs(hit.normal.y) > 0.0f) {
                nextPosition.y -= hit.normal.y * hit.penetration;
                resolved = true;
                if (hit.normal.y < 0.0f && velocity_.y <= 0.0f) {
                    isGrounded_ = true;
                    baseGimmick_ = gimmick;
                } else if (hit.normal.y > 0.0f) {
                    ceilingSquash_ = (std::max)(ceilingSquash_, 0.35f);
                }
            }

            playerBox.center = nextPosition;
        }
    }
    
    return resolved;
}

const Vector3& MapChipPlayer::GetVelocity() const { return velocity_; }
const Vector3& MapChipPlayer::GetForward() const { return forward_; }
const Vector3& MapChipPlayer::GetVisualScale() const { return visualScale_; }

Vector2 MapChipPlayer::GetEyeOffset() const
{
    if (isShapingSelfDestruct_) {
        // 流体の中心は伸ばした形の中央へ移るが、目は元の頭部寄りに残す。
        return {
            selfDestructPull_.x * -0.15f,
            selfDestructPull_.y * -0.15f
        };
    }

    return { 0.0f, 0.0f };
}

Vector3 MapChipPlayer::GetFluidCorePosition() const
{
    if (isShapingSelfDestruct_) {
        return position_ + Vector3{
            selfDestructPull_.x * 0.5f,
            selfDestructPull_.y * 0.5f + kSlimeCoreLift,
            0.0f };
    }
    return position_ + Vector3{ 0.0f, kSlimeCoreLift, 0.0f };
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
        for (const AABB& box : gimmick->GetCollisionBoxes()) {
            checkBoundary(box.center.x - box.size.x * 0.5f, box.center.x + box.size.x * 0.5f,
                          box.center.y - box.size.y * 0.5f, box.center.y + box.size.y * 0.5f);
        }
    }
}

void MapChipPlayer::UpdateVerticalConfinement(const std::vector<BaseMapChipGimmick*>& dynamicGimmicks)
{
    float floorTop = -1000.0f;
    float ceilingBottom = 1000.0f;
    bool floorIsHardenedSlime = false;
    bool ceilingIsHardenedSlime = false;

    const float pMinX = position_.x - kPlayerHalfHeight + 0.08f;
    const float pMaxX = position_.x + kPlayerHalfHeight - 0.08f;

    auto checkObstacle = [&](float bMinX, float bMaxX, float bMinY, float bMaxY,
        bool isHardenedSlime) {
        if (bMaxX <= pMinX || bMinX >= pMaxX) {
            return;
        }
        if (bMaxY <= position_.y && bMaxY > floorTop) {
            floorTop = bMaxY;
            floorIsHardenedSlime = isHardenedSlime;
        }
        if (bMinY >= position_.y && bMinY < ceilingBottom) {
            ceilingBottom = bMinY;
            ceilingIsHardenedSlime = isHardenedSlime;
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
                blockPos.y + 0.5f,
                false);
        }
    }

    for (BaseMapChipGimmick* gimmick : dynamicGimmicks) {
        if (!gimmick->IsSolid()) continue;
        for (const AABB& box : gimmick->GetCollisionBoxes()) {
            checkObstacle(
                box.center.x - box.size.x * 0.5f,
                box.center.x + box.size.x * 0.5f,
                box.center.y - box.size.y * 0.5f,
                box.center.y + box.size.y * 0.5f,
                gimmick->IsHardenedSlime());
        }
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
        // A hardened slime corpse can be used as a platform or ceiling, but
        // must never turn a narrow gap into an instant player death.
        if (gap <= kPlayerSize - kCollisionEpsilon &&
            !floorIsHardenedSlime && !ceilingIsHardenedSlime) {
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

    visualScale_.x =
        baseScale_.x - (wallSquash_ * 0.2f) +
        (landSquash_ * 0.2f) + wobble_ + speedStretch;
    visualScale_.y = std::clamp(
        desiredHeight,
        kSlimeMinimumVisualHeight,
        baseScale_.y);
    visualScale_.z = baseScale_.z;
}
