/**
 * @file SpikeGimmick.h
 * @brief トゲ（Spike）ギミック
 */
#pragma once
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "SpikeParam.h"
#include <memory>

class MapChipPlayer;

/**
 * @brief トゲギミック
 * @details プレイヤーが上に乗った際にダメージ/死亡処理をトリガーする箱。
 * 壁として機能し（IsSolid = true）、プレイヤーがめり込むのを防ぐ。
 */
class SpikeGimmick : public BaseMapChipGimmick {
public:
    SpikeGimmick();
    ~SpikeGimmick() override = default;

    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;
    void Update() override;
    void Draw() override;
    void SetStage(class MapChipStage* stage) override;

    bool IsSolid() const override { return true; } // 壁判定あり

    /**
     * @brief マップチップ空間でのAABBを取得
     */
    AABB GetAABB() const;

public:
    // 全トゲで共有するAABBの設定
    static Vector3 s_spikeAABBSize;
    static Vector3 s_spikeAABBOffset;

private:
    std::unique_ptr<SpikeParam> param_;
    std::unique_ptr<class Object3d> object3d_;
    class MapChipStage* stage_ = nullptr;
    Vector3 position_;
    bool wasPlayerColliding_ = false; // 前フレームでプレイヤーと衝突していたか
};
