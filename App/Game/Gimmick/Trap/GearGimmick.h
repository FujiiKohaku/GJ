/**
 * @file GearGimmick.h
 * @brief 歯車（Gear）障害物ギミックのクラス
 */
#pragma once
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "GearParam.h"
#include <memory>
#include <string>
#include <vector>
#include "Engine/Effect/EffectManager.h"
class Object3d;
class MapChipStage;

/**
 * @brief 回転する歯車の障害物ギミック
 * 触れるとプレイヤーが死亡する
 */
class GearGimmick : public BaseMapChipGimmick {
public:
    GearGimmick();
    ~GearGimmick() override;

    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;

    void Update() override;
    void Draw() override;
    
    AABB GetAABB() const override;
    void SetStage(MapChipStage* stage) override;
    
    // 歯車は通常は障害物として扱わないが、スタック中（死体が挟まっている時）は足場になる
    bool IsSolid() const override { return isJammed_; }

private:
    std::unique_ptr<Object3d> object3d_;
    std::unique_ptr<GearParam> param_;
    MapChipStage* stage_;
    
    Vector3 position_;
    float currentRotationZ_ = 0.0f;
    bool wasPlayerColliding_ = false;
    
    // スタックギミック用
    bool isJammed_ = false;
    float jamShakeTimer_ = 0.0f;
    std::vector<EffectHandle> smokeEffectHandles_;
    
    void UpdateJammedEffects();
    void StopJammedEffects();
};
