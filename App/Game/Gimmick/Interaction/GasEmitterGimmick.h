/**
 * @file GasEmitterGimmick.h
 * @brief ガス発生装置の動作クラス
 */
#pragma once
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "App/Game/Gimmick/Interaction/GasEmitterParam.h"
#include <memory>

class Object3d;
class MapChipStage;

/**
 * @brief 指定イベントを受信するとガスを発生し、着火されると爆発するギミック
 */
class GasEmitterGimmick : public BaseMapChipGimmick {
public:
    GasEmitterGimmick();
    ~GasEmitterGimmick() override;

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

    void OnSpark(const Vector3& origin) override;

    /**
     * @brief ガスエリア（範囲）を取得する
     * @return ガスの範囲を表すAABB
     */
    AABB GetGasAABB() const;
    
    /**
     * @brief ガスを放出中かどうか
     */
    bool IsEmitting() const { return isEmitting_; }

private:
    void StartEmitting();

private:
    std::unique_ptr<Object3d> object_;
    std::unique_ptr<GasEmitterParam> param_;
    MapChipStage* stage_;
    
    Vector3 position_;
    Vector3 size_;
    
    bool isEditorMode_;
    bool isEmitting_; ///< ガスを放出中かどうか
};
