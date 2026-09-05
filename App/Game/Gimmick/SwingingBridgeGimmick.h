#pragma once

#include "BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <memory>

class SwingingBridgeGimmick : public BaseMapChipGimmick {
public:
    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;
    void Update() override;
    void Draw() override;
    void SetEditorMode(bool isEditorMode) override { isEditorMode_ = isEditorMode; }
    void SetStage(class MapChipStage* stage) override { stage_ = stage; }
    
    AABB GetAABB() const override;
    Vector3 GetDeltaPosition() const override;
    
    // 衝突判定用
    bool CheckCollision(const AABB& aabb, class SwingingBridgeGimmick** outHitBridge = nullptr) const;

    // 他の橋から衝突されたときの反応用
    void ApplyAdhesive() { isStuck_ = true; }
    bool HasAdhesive() const { return isStuck_; }
    void ForceStuck() {
        isStuck_ = true;
        isPermanentlyStuck_ = true;
    }
    void ForceBounce() { timeDirection_ *= -1.0f; }

private:
    std::unique_ptr<Object3d> platformObject_;
    std::unique_ptr<Object3d> chainObject_;
    
    Vector3 basePosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 currentPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 previousPosition_ = { 0.0f, 0.0f, 0.0f };
    float elapsedTime_ = 0.0f;
    
    // ギミックパラメータ
    float length_ = 5.0f;
    int swingRange_ = 2;
    float speed_ = 2.0f;
    float phase_ = 0.0f;
    
    bool isEditorMode_ = false;
    bool isStuck_ = false; // 硬化スライムが付着して接着性を持つ状態
    bool isPermanentlyStuck_ = false; // 衝突して完全に固定された状態
    float timeDirection_ = 1.0f; // 時間の進行方向（1.0 or -1.0）
    
    class MapChipStage* stage_ = nullptr;
};
