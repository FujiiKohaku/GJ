#pragma once

#include "Engine/Math/MathStruct.h"
#include "Engine/LevelEditor/LevelData.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include <string>

class BaseMapChipGimmick {
public:
    virtual ~BaseMapChipGimmick() = default;

    virtual bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void EnableToonLighting() {}
    virtual void SetEditorMode(bool isEditorMode) {}
    
    virtual AABB GetAABB() const { return AABB(); }
    virtual Vector3 GetDeltaPosition() const { return {0.0f, 0.0f, 0.0f}; }
    // ゴール判定用フラグ（デフォルトは偽）
    virtual bool IsGoal() const { return false; }

    // プレイヤーが衝突する（壁として働く）かどうか
    virtual bool IsSolid() const { return true; }

    /**
     * @brief マップチップステージ（イベントマネージャ等を持つ親）を設定する
     * @param stage ステージのポインタ
     */
    virtual void SetStage(class MapChipStage* stage) {}

    /**
     * @brief イベントを受信した際の処理
     * @param eventName 受信したイベント名
     */
    virtual void OnEvent(const std::string& eventName) {}

    /**
     * @brief 空間的にスパーク（着火）が発生した際の処理
     * @param origin スパークの発生座標
     */
    virtual void OnSpark(const Vector3& origin) {}

    /**
     * @brief 空間的に爆発が発生した際の処理
     * @param origin 爆発の中心座標
     * @param radius 爆発の半径
     */
    virtual void OnExplosion(const Vector3& origin, float radius) {}
};
