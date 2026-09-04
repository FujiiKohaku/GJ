/**
 * @file SwitchGimmick.h
 * @brief スイッチおよび着火ギミックの動作クラス
 */
#pragma once
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "App/Game/Gimmick/Interaction/SwitchParam.h"
#include <memory>

class Object3d;
class MapChipStage;

/**
 * @brief プレイヤーや死体の重さでイベントを発火する、または篝火として着火するギミック
 */
class SwitchGimmick : public BaseMapChipGimmick {
public:
    SwitchGimmick();
    ~SwitchGimmick() override;

    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;

    void Update() override;
    void Draw() override;
    void SetEditorMode(bool isEditorMode) override;
    
    AABB GetAABB() const override;
    void SetStage(MapChipStage* stage) override;
    bool IsSolid() const override { return false; }

    // 感圧盤用の共通AABB設定
    static Vector3 s_pressurePlateAABBOffset;
    static Vector3 s_pressurePlateAABBSize;

private:
    std::unique_ptr<Object3d> object_;
    std::unique_ptr<SwitchParam> param_;
    MapChipStage* stage_;
    
    Vector3 position_;
    Vector3 size_;
    
    bool isEditorMode_;
    bool isActive_; ///< スイッチがオン状態かどうか
};
