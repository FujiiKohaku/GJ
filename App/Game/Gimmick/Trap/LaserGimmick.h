#pragma once

#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "App/Game/Gimmick/Trap/LaserParam.h"
#include "Engine/3D/Object3d.h"
#include <memory>

/**
 * @class LaserGimmick
 * @brief レーザーを発射し、触れたプレイヤーにダメージを与えるトラップギミック
 */
class LaserGimmick : public BaseMapChipGimmick {
public:
    LaserGimmick();
    ~LaserGimmick() override = default;

    /**
     * @brief 初期化
     * @param position 配置座標
     * @param texturePath テクスチャ/モデルのパス（未使用）
     * @param gimmickParam パラメータ（LaserParam）
     * @return 成功ならtrue
     */
    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam) override;

    /**
     * @brief 更新処理
     * レーザーの長さを計算し、プレイヤーとの衝突判定を行う
     */
    void Update() override;

    /**
     * @brief 描画処理
     */
    void Draw() override;

    /**
     * @brief 当たり判定用AABBの取得（ギミック本体、1マス分）
     */
    AABB GetAABB() const override;

    /**
     * @brief 所属するステージ（フィールドやプレイヤーへのアクセス用）のセット
     */
    void SetStage(MapChipStage* stage) override;

private:
    std::unique_ptr<Object3d> emitterObject_; // 発射機本体のモデル
    std::unique_ptr<Object3d> beamObject_;    // レーザービーム（Cube）モデル

    std::unique_ptr<LaserParam> param_;       // パラメータ
    MapChipStage* stage_ = nullptr;           // ステージへのポインタ
    Vector3 position_;                        // ギミックの配置座標

    AABB laserAABB_;                          // 現在のレーザービームの当たり判定
    float currentLaserLength_ = 0.0f;         // 現在のレーザーの長さ（マス数）

    bool wasPlayerColliding_ = false;         // 前フレームのプレイヤー衝突フラグ
};
