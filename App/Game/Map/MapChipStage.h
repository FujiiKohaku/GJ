#pragma once

#include "MapChipField.h"
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Event/EventManager.h"
#include <memory>
#include <string>
#include <vector>

class MapChipPlayer;

class MapChipStage {
public:
    ~MapChipStage();

    void Initialize(
        const LevelData& levelData,
        const std::string& texturePath =
            "resources/Textures/checkerboard.png");
    void Update();
    void Draw();
    void EnableToonLighting();
    void EnableMossTerrain();

    const MapChipField& GetField() const;
    MapChipField& GetField();
    
    void SetEditorMode(bool isEditor) { isEditorMode_ = isEditor; }
    
    void SetPlayer(MapChipPlayer* player) { player_ = player; }
    MapChipPlayer* GetPlayer() const { return player_; }
    
    std::vector<BaseMapChipGimmick*> GetGimmicks() const;
    void AddGimmick(std::unique_ptr<BaseMapChipGimmick> gimmick);

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

private:
    void ResolveHardenedSlimeAdhesion(
        const BaseMapChipGimmick& hardenedSlime);

    MapChipField field_;
    std::vector<std::unique_ptr<Object3d>> blockObjects_;
    std::vector<std::unique_ptr<BaseMapChipGimmick>> gimmicks_;
    IrufemiEngine::EventManager eventManager_;
    bool isEditorMode_ = false;
    MapChipPlayer* player_ = nullptr;
};
