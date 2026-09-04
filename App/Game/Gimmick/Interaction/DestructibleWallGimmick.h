/**
 * @file DestructibleWallGimmick.h
 * @brief 爆発によって破壊される壁ギミック
 */
#pragma once
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include <memory>
#include <string>

class Object3d;
class MapChipStage;

/**
 * @brief ガス爆発等によって破壊される石ブロック
 */
class DestructibleWallGimmick : public BaseMapChipGimmick {
public:
    DestructibleWallGimmick();
    ~DestructibleWallGimmick() override;

    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;

    void Update() override;
    void Draw() override;
    void SetEditorMode(bool isEditorMode) override;
    
    AABB GetAABB() const override;
    void SetStage(MapChipStage* stage) override;
    
    bool IsSolid() const override { return !isDestroyed_; }

    void OnExplosion(const Vector3& origin, float radius) override;

private:
    std::unique_ptr<Object3d> object_;
    MapChipStage* stage_;
    
    Vector3 position_;
    Vector3 size_;
    
    bool isEditorMode_;
    bool isDestroyed_; ///< 破壊されたかどうか
};
