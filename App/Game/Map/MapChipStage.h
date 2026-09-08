#pragma once

#include "MapChipField.h"
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Event/EventManager.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

class MapChipPlayer;

struct StageSnapshot {
    std::unordered_map<BaseMapChipGimmick*, std::shared_ptr<IGimmickState>> gimmickStates;
};

class MapChipStage {
public:
    ~MapChipStage();

    void Initialize(
        const LevelData& levelData,
        const std::string& texturePath =
            "resources/Textures/checkerboard.png",
        const Vector3& worldOffset = {0.0f, 0.0f, 0.0f});
    void Update();
    void Draw();
    void ApplyMaterialProperties();

    const MapChipField& GetField() const;
    MapChipField& GetField();
    const Vector3& GetWorldOffset() const { return worldOffset_; }
    
    void SetEditorMode(bool isEditor) { isEditorMode_ = isEditor; }
    
    void SetPlayer(MapChipPlayer* player) { player_ = player; }
    MapChipPlayer* GetPlayer() const { return player_; }
    
    std::vector<BaseMapChipGimmick*> GetGimmicks() const;
    void AddGimmick(std::unique_ptr<BaseMapChipGimmick> gimmick);
    void LimitHardenedSlimeCount(size_t maximumCount);
    bool RemoveLatestHardenedSlime();

    /**
     * @brief 現在のステージの全揮発性ギミックの状態を収集したスナップショットを作成する
     * @return スナップショットオブジェクト
     */
    StageSnapshot CreateStageSnapshot() const;

    /**
     * @brief スナップショットを使ってステージの全ギミックの状態を一斉に復元する
     * @param snapshot 復元元のスナップショット
     */
    void RestoreStageSnapshot(const StageSnapshot& snapshot);

    /**
     * @brief イベントマネージャを取得する
     * @return イベントマネージャの参照
     */
    IrufemiEngine::EventManager& GetEventManager() { return eventManager_; }

    /**
     * @brief 指定した座標を中心とする半径内のギミックを取得する
     * @param center 中心座標
     * @param radius 半径
     * @return 範囲内のギミックのリスト
     */
    std::vector<BaseMapChipGimmick*> GetGimmicksInSphere(const Vector3& center, float radius);

    /**
     * @brief 着火イベント（Spark）を空間に発生させる
     * @details この座標を内包するガスエリアがあれば大爆発（CreateExplosion）を誘発する
     * @param origin 発生座標
     */
    void CreateSpark(const Vector3& origin);

    /**
     * @brief 爆発イベント（Explosion）を空間に発生させる
     * @details 範囲内の全ギミックの OnExplosion() を呼び出す
     * @param origin 爆発の中心座標
     * @param radius 爆発の半径
     */
    void CreateExplosion(const Vector3& origin, float radius);

    /**
     * @brief マス目（グリッド）指定で爆発イベントを発生させる
     * @details origin を基準に、上下左右の指定マス数内にいるギミックを爆破する
     */
    void CreateExplosionGrid(const Vector3& origin, uint32_t left, uint32_t right, uint32_t up, uint32_t down);

private:
    void ResolveHardenedSlimeAdhesion(
        const BaseMapChipGimmick& hardenedSlime);

    MapChipField field_;
    std::vector<std::unique_ptr<Object3d>> blockObjects_;
    std::vector<std::unique_ptr<BaseMapChipGimmick>> gimmicks_;
    IrufemiEngine::EventManager eventManager_;
    bool isEditorMode_ = false;
    MapChipPlayer* player_ = nullptr;
    Vector3 worldOffset_ = {0.0f, 0.0f, 0.0f};
};
