import re

with open(r'c:\Users\k024g\source\repos\GJ\App\Game\Player\MapChipPlayer.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Replace MoveHorizontal
move_horiz_pattern = r'void MapChipPlayer::MoveHorizontal.*?position_\.x = nextPosition\.x;\n\}'
move_horiz_repl = '''void MapChipPlayer::MoveHorizontal(float deltaTime, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks)
{
    float prevVelX = velocity_.x;
    Vector3 nextPosition = position_;
    nextPosition.x += velocity_.x * deltaTime;
    
    if (ResolveDynamicCollision(nextPosition, dynamicGimmicks, true)) {
        velocity_.x = 0.0f;
        isColliding_ = true;
        wallSquash_ = (std::max)(wallSquash_, Saturate(std::abs(prevVelX) / kMoveSpeed));
    }
    
    if (ResolveHorizontalCollision(nextPosition)) {
        velocity_.x = 0.0f;
        isColliding_ = true;
        wallSquash_ = (std::max)(wallSquash_, Saturate(std::abs(prevVelX) / kMoveSpeed));
    }
    position_.x = nextPosition.x;
}'''
content = re.sub(move_horiz_pattern, move_horiz_repl, content, flags=re.DOTALL)

# Replace MoveVertical
move_vert_pattern = r'void MapChipPlayer::MoveVertical.*?position_\.y = nextPosition\.y;\n\}'
move_vert_repl = '''void MapChipPlayer::MoveVertical(float deltaTime, const std::vector<BaseMapChipGimmick*>& dynamicGimmicks)
{
    float prevVelY = velocity_.y;
    velocity_.y += kGravity * deltaTime;
    Vector3 nextPosition = position_;
    nextPosition.y += velocity_.y * deltaTime;
    wasGrounded_ = isGrounded_;
    isGrounded_ = false;
    
    if (ResolveDynamicCollision(nextPosition, dynamicGimmicks, false)) {
        if (velocity_.y > 0.0f) {
            ceilingSquash_ = (std::max)(ceilingSquash_, Saturate(std::abs(prevVelY) / kJumpSpeed));
        }
        velocity_.y = 0.0f;
        isColliding_ = true;
    }
    
    if (ResolveVerticalCollision(nextPosition)) {
        if (velocity_.y > 0.0f) {
            ceilingSquash_ = (std::max)(ceilingSquash_, Saturate(std::abs(prevVelY) / kJumpSpeed));
        }
        velocity_.y = 0.0f;
        isColliding_ = true;
    }
    
    if (!wasGrounded_ && isGrounded_) {
        landSquash_ = (std::max)(landSquash_, Saturate(std::abs(prevVelY) / kJumpSpeed));
    }
    position_.y = nextPosition.y;
}'''
content = re.sub(move_vert_pattern, move_vert_repl, content, flags=re.DOTALL)

# Add missing functions
missing_funcs = '''
const Vector3& MapChipPlayer::GetVelocity() const { return velocity_; }
const Vector3& MapChipPlayer::GetForward() const { return forward_; }
const Vector3& MapChipPlayer::GetVisualScale() const { return visualScale_; }

Vector3 MapChipPlayer::GetFluidCorePosition() const
{
    return position_ + Vector3{0.0f, kSlimeCoreLift, 0.0f};
}

float MapChipPlayer::GetFluidFloorHeight() const
{
    return position_.y - kPlayerHalfHeight;
}

void MapChipPlayer::GetWallBoundaries(float& outMinX, float& outMaxX, float& outMaxY) const
{
    outMinX = -1000.0f;
    outMaxX = 1000.0f;
    outMaxY = 1000.0f;
    
    if (!mapChipField_) return;

    const uint32_t width = mapChipField_->GetBlockWidth();
    const uint32_t height = mapChipField_->GetBlockHeight();

    const float pMinY = position_.y - kPlayerHalfHeight + 0.1f;
    const float pMaxY = position_.y + kPlayerHalfHeight - 0.1f;
    const float pMinX = position_.x - kPlayerHalfHeight + 0.1f;
    const float pMaxX = position_.x + kPlayerHalfHeight - 0.1f;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            if (mapChipField_->GetMapChipTypeByIndex(x, y) != MapChipType::Block) continue;
            
            Vector3 blockPos = mapChipField_->GetMapChipPositionByIndex(x, y);
            float bMinX = blockPos.x - 0.5f;
            float bMaxX = blockPos.x + 0.5f;
            float bMinY = blockPos.y - 0.5f;
            float bMaxY = blockPos.y + 0.5f;

            if (bMinY < pMaxY && bMaxY > pMinY) {
                if (bMaxX <= position_.x && bMaxX > outMinX) outMinX = bMaxX;
                if (bMinX >= position_.x && bMinX < outMaxX) outMaxX = bMinX;
            }
            if (bMinX < pMaxX && bMaxX > pMinX) {
                if (bMinY >= position_.y && bMinY < outMaxY) outMaxY = bMinY;
            }
        }
    }
}

void MapChipPlayer::UpdateVisualShape(float deltaTime)
{
    time_ += deltaTime;

    const float horizontalSpeed01 = Saturate(std::abs(velocity_.x) / kMoveSpeed);
    const float verticalSpeed01 = Saturate(std::abs(velocity_.y) / kJumpSpeed);

    wobble_ = std::sin(time_ * 15.0f) * 0.1f * horizontalSpeed01;

    wallSquash_ = (std::max)(0.0f, wallSquash_ - deltaTime * 5.0f);
    landSquash_ = (std::max)(0.0f, landSquash_ - deltaTime * 5.0f);
    ceilingSquash_ = (std::max)(0.0f, ceilingSquash_ - deltaTime * 5.0f);

    visualScale_.x = baseScale_.x - (wallSquash_ * 0.2f) + (landSquash_ * 0.2f) + wobble_;
    visualScale_.y = baseScale_.y + (wallSquash_ * 0.2f) - (landSquash_ * 0.2f) - (ceilingSquash_ * 0.2f) - wobble_;
    visualScale_.z = baseScale_.z;
}
'''

content += missing_funcs

with open(r'c:\Users\k024g\source\repos\GJ\App\Game\Player\MapChipPlayer.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
