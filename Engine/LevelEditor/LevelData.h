#pragma once
#include "../math/EngineStruct.h"
#include <string>
#include <vector>
#include <memory>
#include "BaseGimmickParam.h"

struct LevelData {

    struct ObjectData {

        std::string name;
        std::string type;
        std::string fileName;

        Vector3 translation;
        Vector3 rotation;
        Vector3 scale;

        bool disabled = false;
        
        struct ScoreItemData {
            bool exists = false;
            int score = 0;
        } scoreItem;

        struct ColliderData {
            bool exists = false;
            std::string type;
            Vector3 center;
            Vector3 size;
        } collider;

        struct TriggerData {
            bool exists = false;
            std::string type;
            std::string name;
            Vector3 center;
            Vector3 size;
            Vector3 force;
        } trigger;

        struct HazardData {
            bool exists = false;
            std::string type;
            int damage = 1;
        } hazard;

        struct GimmickData {
            bool exists = false;
            std::string type;
            float speed = 0.0f;
            Vector3 range;
            Vector3 axis = { 0.0f, 1.0f, 0.0f };
        } gimmick; // TODO: 段階的に削除予定

        std::unique_ptr<BaseGimmickParam> gimmickParam;

        struct DestructibleData {
            bool exists = false;
            float hp = 1.0f;
        } destructible;

        struct CameraPointData {
            bool exists = false;
            std::string name;
            Vector3 target;
            float moveTime = 0.0f;
        } cameraPoint;

        struct CameraFovPointData {
            bool exists = false;
            float fov = 0.0f;
            float time = 0.0f;
        } cameraFovPoint;

        struct PatrolRouteData {
            bool exists = false;
            std::vector<Vector3> waypoints;
        } patrolRoute;

        struct TerrainData {
            bool exists = false;
            std::string file;
            float width = 0.0f;
            float height = 0.0f;
        } terrain;

        bool meshSync = false;

        ObjectData() = default;
        ~ObjectData() = default;

        // コピーコンストラクタ
        ObjectData(const ObjectData& other) {
            *this = other; // コピー代入演算子に委譲
        }

        // コピー代入演算子
        ObjectData& operator=(const ObjectData& other) {
            if (this == &other) return *this;
            name = other.name;
            type = other.type;
            fileName = other.fileName;
            translation = other.translation;
            rotation = other.rotation;
            scale = other.scale;
            disabled = other.disabled;
            scoreItem = other.scoreItem;
            collider = other.collider;
            trigger = other.trigger;
            hazard = other.hazard;
            gimmick = other.gimmick;
            destructible = other.destructible;
            cameraPoint = other.cameraPoint;
            cameraFovPoint = other.cameraFovPoint;
            patrolRoute = other.patrolRoute;
            terrain = other.terrain;
            meshSync = other.meshSync;

            if (other.gimmickParam) {
                gimmickParam = other.gimmickParam->Clone();
            } else {
                gimmickParam.reset();
            }
            return *this;
        }

        // ムーブはデフォルト
        ObjectData(ObjectData&&) = default;
        ObjectData& operator=(ObjectData&&) = default;
    };

    struct PlayerSpawnData {
        Vector3 translation;
        Vector3 rotation;
    };

    struct EnemySpawnData {
        std::string fileName;
        Vector3 translation;
        Vector3 rotation;
    };

    struct TileMapData {
        std::string name;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<int32_t> data;
    };

    std::vector<ObjectData> objects;
    std::vector<PlayerSpawnData> playerSpawns;
    std::vector<EnemySpawnData> enemies;
    std::vector<TileMapData> tileMaps;
};
