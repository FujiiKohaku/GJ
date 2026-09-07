#pragma once

#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "App/Game/Gimmick/Trap/LaserParam.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/LaserBeamRenderer.h"
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
    // beamObject_ は削除し、LaserBeamRenderer を使用します

    LaserBeamRenderParams beamParams_;        // レーザーの描画パラメータ

    std::unique_ptr<LaserParam> param_;       // パラメータ
    MapChipStage* stage_ = nullptr;           // ステージへのポインタ
    Vector3 position_;                        // ギミックの配置座標

    float currentLaserLength_ = 0.0f;         // 現在のレーザーの長さ（マス数）
    float staticLaserLength_ = 0.0f; // プレイヤーとの判定を行う前の、静的な壁までの距離
    AABB laserAABB_{};                          // 現在のレーザービームの当たり判定

    bool wasPlayerColliding_ = false;         // 前フレームのプレイヤー衝突フラグ
    float playerHitTime_ = 0.0f;              // レーザー接触から死亡までの経過時間
};
