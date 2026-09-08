#include "LevelDataLoader.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include "Engine/Logger/Logger.h"
#include "Engine/Logger/Logger.h"
#include "GimmickParamFactory.h"
#include "GimmickMetaDataManager.h"
LevelData LevelDataLoader::Load(const std::string& filePath)
{
    std::ifstream file(filePath);

    if (file.fail()) {
        assert(false);
    }

    nlohmann::json jsonData;
    file >> jsonData;

    LevelData levelData;

    if (jsonData.contains("scene")) {
        for (const nlohmann::json& objectJson : jsonData["scene"]) {
            LoadObject(objectJson, levelData);
        }
    }

    if (jsonData.contains("objects")) {
        for (const nlohmann::json& objectJson : jsonData["objects"]) {
            LoadObject(objectJson, levelData);
        }
    }

    if (jsonData.contains("tileMaps")) {
        for (const nlohmann::json& mapJson : jsonData["tileMaps"]) {
            LevelData::TileMapData tileMap {};
            if (mapJson.contains("name")) {
                tileMap.name = mapJson["name"].get<std::string>();
            }
            if (mapJson.contains("width")) {
                tileMap.width = mapJson["width"].get<uint32_t>();
            }
            if (mapJson.contains("height")) {
                tileMap.height = mapJson["height"].get<uint32_t>();
            }
            if (mapJson.contains("data")) {
                for (const auto& dataVal : mapJson["data"]) {
                    tileMap.data.push_back(dataVal.get<int32_t>());
                }
            }
            levelData.tileMaps.push_back(tileMap);
        }
    }

    return levelData;
}

void LevelDataLoader::LoadObject(const nlohmann::json& objectJson, LevelData& levelData)
{
    bool disabled = false;
    if (objectJson.contains("disabled")) {
        disabled = objectJson["disabled"].get<bool>();
    }

    if (disabled) {
        return;
    }

    if (!objectJson.contains("type")) {
        return;
    }

    std::string typeStr = objectJson["type"].get<std::string>();
    if (typeStr == "PlayerSpawn") {
        LevelData::PlayerSpawnData spawnData {};

        if (objectJson.contains("transform")) {
            const nlohmann::json& transform = objectJson["transform"];

            if (transform.contains("translation")) {
                spawnData.translation.x = transform["translation"][0].get<float>();
                spawnData.translation.y = transform["translation"][2].get<float>();
                spawnData.translation.z = transform["translation"][1].get<float>();
            }

            if (transform.contains("rotation")) {
                float degreeToRadian = 3.1415926535f / 180.0f;
                spawnData.rotation.x = -transform["rotation"][0].get<float>() * degreeToRadian;
                spawnData.rotation.y = -transform["rotation"][2].get<float>() * degreeToRadian;
                spawnData.rotation.z = -transform["rotation"][1].get<float>() * degreeToRadian;
            }
        }

        levelData.playerSpawns.push_back(spawnData);
        return;
    }
    else if (typeStr == "EnemySpawn") {
        LevelData::EnemySpawnData spawnData {};

        if (objectJson.contains("file_name")) {
            spawnData.fileName = objectJson["file_name"].get<std::string>();
        }

        if (objectJson.contains("transform")) {
            const nlohmann::json& transform = objectJson["transform"];

            if (transform.contains("translation")) {
                spawnData.translation.x = transform["translation"][0].get<float>();
                spawnData.translation.y = transform["translation"][2].get<float>();
                spawnData.translation.z = transform["translation"][1].get<float>();
            }

            if (transform.contains("rotation")) {
                float degreeToRadian = 3.1415926535f / 180.0f;
                spawnData.rotation.x = -transform["rotation"][0].get<float>() * degreeToRadian;
                spawnData.rotation.y = -transform["rotation"][2].get<float>() * degreeToRadian;
                spawnData.rotation.z = -transform["rotation"][1].get<float>() * degreeToRadian;
            }
        }

        levelData.enemies.push_back(spawnData);
    }

    LevelData::ObjectData objectData;

    objectData.type = objectJson["type"].get<std::string>();

    if (objectJson.contains("name")) {
        objectData.name = objectJson["name"].get<std::string>();
    }

    std::string metaKey = objectData.type;
    if (metaKey == "Switch" && objectJson.contains("gimmick") && objectJson["gimmick"].contains("switchType")) {
        int switchType = objectJson["gimmick"]["switchType"].get<int>();
        if (switchType == 2) {
            metaKey = "Switch_Bonfire";
        } else {
            metaKey = "Switch_PressurePlate";
        }
    }

    if (objectJson.contains("file_name") && !objectJson["file_name"].get<std::string>().empty()) {
        std::string jsonFileName = objectJson["file_name"].get<std::string>();
        objectData.fileName = jsonFileName;

        // もし古い形式(スラッシュが含まれない単なるファイル名)の場合はメタデータから最新のパスを引く
        if (jsonFileName.find('/') == std::string::npos && jsonFileName.find('\\') == std::string::npos) {
            const auto* metaData = GimmickMetaDataManager::GetInstance()->GetMetaData(metaKey);
            if (metaData) {
                objectData.fileName = metaData->defaultModelPath;
            }
        }
    } else {
        const auto* metaData = GimmickMetaDataManager::GetInstance()->GetMetaData(metaKey);
        if (metaData) {
            objectData.fileName = metaData->defaultModelPath;
        }

        if (objectData.name.empty()) {
            objectData.name = objectData.type;
        }
    }

    if (objectJson.contains("transform")) {
        const nlohmann::json& transform = objectJson["transform"];

        if (transform.contains("translation")) {
            objectData.translation.x = transform["translation"][0].get<float>();
            objectData.translation.y = transform["translation"][2].get<float>();
            objectData.translation.z = transform["translation"][1].get<float>();
        }

        if (transform.contains("rotation")) {
            objectData.rotation.x = -transform["rotation"][0].get<float>();
            objectData.rotation.y = -transform["rotation"][2].get<float>();
            objectData.rotation.z = -transform["rotation"][1].get<float>();
        }

        if (transform.contains("scale")) {
            objectData.scale.x = transform["scale"][0].get<float>();
            objectData.scale.y = transform["scale"][2].get<float>();
            objectData.scale.z = transform["scale"][1].get<float>();
        }

        if (transform.contains("scaling")) {
            objectData.scale.x = transform["scaling"][0].get<float>();
            objectData.scale.y = transform["scaling"][2].get<float>();
            objectData.scale.z = transform["scaling"][1].get<float>();
        }
    }

    if (objectJson.contains("disabled")) {
        objectData.disabled = objectJson["disabled"].get<bool>();
    }

    if (objectJson.contains("collider")) {
        const nlohmann::json& collider = objectJson["collider"];
        objectData.collider.exists = true;
        if (collider.contains("type")) {
            objectData.collider.type = collider["type"].get<std::string>();
        }
        if (collider.contains("center")) {
            objectData.collider.center.x = collider["center"][0].get<float>();
            objectData.collider.center.y = collider["center"][2].get<float>();
            objectData.collider.center.z = collider["center"][1].get<float>();
        }
        if (collider.contains("size")) {
            objectData.collider.size.x = collider["size"][0].get<float>();
            objectData.collider.size.y = collider["size"][2].get<float>();
            objectData.collider.size.z = collider["size"][1].get<float>();
        }
    }

    if (objectJson.contains("trigger")) {
        const nlohmann::json& trigger = objectJson["trigger"];
        objectData.trigger.exists = true;
        if (trigger.contains("type")) {
            objectData.trigger.type = trigger["type"].get<std::string>();
        }
        if (trigger.contains("name")) {
            objectData.trigger.name = trigger["name"].get<std::string>();
        }
        if (trigger.contains("center")) {
            objectData.trigger.center.x = trigger["center"][0].get<float>();
            objectData.trigger.center.y = trigger["center"][2].get<float>();
            objectData.trigger.center.z = trigger["center"][1].get<float>();
        }
        if (trigger.contains("size")) {
            objectData.trigger.size.x = trigger["size"][0].get<float>();
            objectData.trigger.size.y = trigger["size"][2].get<float>();
            objectData.trigger.size.z = trigger["size"][1].get<float>();
        }
        if (trigger.contains("force")) {
            objectData.trigger.force.x = trigger["force"][0].get<float>();
            objectData.trigger.force.y = trigger["force"][2].get<float>();
            objectData.trigger.force.z = trigger["force"][1].get<float>();
        }
    }

    if (objectJson.contains("hazard")) {
        objectData.hazard.exists = true;
        if (objectJson["hazard"].contains("type")) {
            objectData.hazard.type = objectJson["hazard"]["type"].get<std::string>();
        }
        if (objectJson["hazard"].contains("damage")) {
            objectData.hazard.damage = objectJson["hazard"]["damage"].get<int>();
        }
    }

    // 案Cに基づくファクトリによる動的パース処理
    // 古いデータや保存漏れに備えて、まずは親の type でインスタンスを生成する
    objectData.gimmickParam = GimmickParamFactory::GetInstance()->Create(objectData.type);

    if (objectJson.contains("gimmick")) {
        const nlohmann::json& gimmickJson = objectJson["gimmick"];
        
        std::string gimmickType = objectData.type;
        if (gimmickJson.contains("type")) {
            gimmickType = gimmickJson["type"].get<std::string>();
            // 親と異なる場合のみ再生成
            if (gimmickType != objectData.type) {
                objectData.gimmickParam = GimmickParamFactory::GetInstance()->Create(gimmickType);
            }
        }
        
        if (objectData.gimmickParam) {
            objectData.gimmickParam->Parse(gimmickJson);
        } else {
            // 未対応ギミックの場合は従来のフォールバック処理（移行期間用）
            objectData.gimmick.exists = true;
            objectData.gimmick.type = gimmickType;
            if (gimmickJson.contains("speed")) {
                objectData.gimmick.speed = gimmickJson["speed"].get<float>();
            }
            if (gimmickJson.contains("range")) {
                objectData.gimmick.range.x = gimmickJson["range"][0].get<float>();
                objectData.gimmick.range.y = gimmickJson["range"][2].get<float>();
                objectData.gimmick.range.z = gimmickJson["range"][1].get<float>();
            }
            if (gimmickJson.contains("axis")) {
                objectData.gimmick.axis.x = gimmickJson["axis"][0].get<float>();
                objectData.gimmick.axis.y = gimmickJson["axis"][2].get<float>();
                objectData.gimmick.axis.z = gimmickJson["axis"][1].get<float>();
            }
        }
    }

    if (objectJson.contains("destructible")) {
        const nlohmann::json& destructible = objectJson["destructible"];
        objectData.destructible.exists = true;
        if (destructible.contains("hp")) {
            objectData.destructible.hp = destructible["hp"].get<float>();
        }
    }

    if (objectJson.contains("camera_point")) {
        const nlohmann::json& cam = objectJson["camera_point"];
        objectData.cameraPoint.exists = true;
        if (cam.contains("name")) {
            objectData.cameraPoint.name = cam["name"].get<std::string>();
        }
        if (cam.contains("target")) {
            objectData.cameraPoint.target.x = cam["target"][0].get<float>();
            objectData.cameraPoint.target.y = cam["target"][2].get<float>();
            objectData.cameraPoint.target.z = cam["target"][1].get<float>();
        }
        if (cam.contains("move_time")) {
            objectData.cameraPoint.moveTime = cam["move_time"].get<float>();
        }
    }

    if (objectJson.contains("camera_fov_point")) {
        const nlohmann::json& fovPoint = objectJson["camera_fov_point"];
        objectData.cameraFovPoint.exists = true;
        if (fovPoint.contains("fov")) {
            objectData.cameraFovPoint.fov = fovPoint["fov"].get<float>();
        }
        if (fovPoint.contains("time")) {
            objectData.cameraFovPoint.time = fovPoint["time"].get<float>();
        }
    }

    if (objectJson.contains("patrol_route")) {
        const nlohmann::json& patrol = objectJson["patrol_route"];
        objectData.patrolRoute.exists = true;
        if (patrol.contains("waypoints")) {
            for (const auto& wpJson : patrol["waypoints"]) {
                Vector3 wp;
                wp.x = wpJson[0].get<float>();
                wp.y = wpJson[2].get<float>();
                wp.z = wpJson[1].get<float>();
                objectData.patrolRoute.waypoints.push_back(wp);
            }
        }
    }

    if (objectJson.contains("terrain")) {
        const nlohmann::json& terr = objectJson["terrain"];
        objectData.terrain.exists = true;
        if (terr.contains("file")) {
            objectData.terrain.file = terr["file"].get<std::string>();
        }
        if (terr.contains("width")) {
            objectData.terrain.width = terr["width"].get<float>();
        }
        if (terr.contains("height")) {
            objectData.terrain.height = terr["height"].get<float>();
        }
    }

    if (objectJson.contains("mesh_sync")) {
        objectData.meshSync = objectJson["mesh_sync"].get<bool>();
    }

    levelData.objects.push_back(objectData);

    if (objectJson.contains("children")) {
        for (const nlohmann::json& childJson : objectJson["children"]) {
            LoadObject(childJson, levelData);
        }
    }
}

void LevelDataLoader::Save(const std::string& filePath, const LevelData& levelData)
{
    nlohmann::json root;
    
    if (!levelData.tileMaps.empty()) {
        nlohmann::json tileMapsArray = nlohmann::json::array();
        for (const auto& mapData : levelData.tileMaps) {
            nlohmann::json mapJson;
            mapJson["name"] = mapData.name;
            mapJson["width"] = mapData.width;
            mapJson["height"] = mapData.height;
            mapJson["data"] = mapData.data;
            tileMapsArray.push_back(mapJson);
        }
        root["tileMaps"] = tileMapsArray;
    }

    if (!levelData.playerSpawns.empty()) {
        nlohmann::json spawnsArray = nlohmann::json::array();
        for (const auto& spawn : levelData.playerSpawns) {
            nlohmann::json spawnJson;
            spawnJson["type"] = "PlayerSpawn";
            nlohmann::json transform;
            // ロード時の仕様 (x=[0], y=[2], z=[1]) に合わせて保存
            transform["translation"] = { spawn.translation.x, spawn.translation.z, spawn.translation.y };
            spawnJson["transform"] = transform;
            spawnsArray.push_back(spawnJson);
        }
        root["objects"] = spawnsArray;
    }

    // objects等の動的オブジェクトも拡張する場合はここに追記
    if (!levelData.objects.empty()) {
        if (!root.contains("objects")) {
            root["objects"] = nlohmann::json::array();
        }
        for (const auto& obj : levelData.objects) {
            nlohmann::json objJson;
            objJson["type"] = obj.type;
            if (!obj.name.empty()) {
                objJson["name"] = obj.name;
            }
            if (!obj.fileName.empty()) {
                objJson["file_name"] = obj.fileName;
            }
            nlohmann::json transform;
            // ロード時の仕様に合わせて保存
            transform["translation"] = { obj.translation.x, obj.translation.z, obj.translation.y };
            objJson["transform"] = transform;
            
            if (obj.hazard.exists) {
                nlohmann::json hazard;
                hazard["type"] = obj.hazard.type;
                hazard["damage"] = obj.hazard.damage;
                objJson["hazard"] = hazard;
            }

            // 新しいポリモーフィズム設計による保存
            if (obj.gimmickParam) {
                objJson["gimmick"] = obj.gimmickParam->Serialize();
            } 
            // 従来の互換性用
            else if (obj.gimmick.exists) {
                nlohmann::json gimmick;
                gimmick["type"] = obj.gimmick.type;
                gimmick["speed"] = obj.gimmick.speed;
                gimmick["range"] = { obj.gimmick.range.x, obj.gimmick.range.z, obj.gimmick.range.y };
                gimmick["axis"] = { obj.gimmick.axis.x, obj.gimmick.axis.z, obj.gimmick.axis.y };
                objJson["gimmick"] = gimmick;
            }
            
            // 他のプロパティが必要な場合は追加
            root["objects"].push_back(objJson);
        }
    }

    std::ofstream file(filePath);
    if (file.is_open()) {
        file << root.dump(2);
        file.close();
    }
}
