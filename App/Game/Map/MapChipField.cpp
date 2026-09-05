#include "MapChipField.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include "Engine/LevelEditor/GimmickMetaDataManager.h"

void MapChipField::Initialize(const LevelData::TileMapData& tileMapData)
{
    ResetMapChipData();

    if (tileMapData.width == 0 || tileMapData.height == 0) {
        return;
    }

    uint32_t index = 0;
    for (uint32_t y = 0; y < tileMapData.height; ++y) {
        std::vector<MapChipType> row;
        for (uint32_t x = 0; x < tileMapData.width; ++x) {
            if (index < tileMapData.data.size()) {
                row.push_back(static_cast<MapChipType>(tileMapData.data[index]));
            } else {
                row.push_back(MapChipType::Blank);
            }
            index++;
        }
        mapChipData_.push_back(row);
    }
}

void MapChipField::ResetMapChipData()
{
    mapChipData_.clear();
}

MapChipType MapChipField::GetMapChipTypeByIndex(
    uint32_t xIndex,
    uint32_t yIndex) const
{
    if (yIndex >= mapChipData_.size()) {
        return MapChipType::Blank;
    }
    if (xIndex >= mapChipData_[yIndex].size()) {
        return MapChipType::Blank;
    }
    return mapChipData_[yIndex][xIndex];
}

void MapChipField::SetMapChipTypeByIndex(
    uint32_t xIndex,
    uint32_t yIndex,
    MapChipType type)
{
    if (yIndex >= mapChipData_.size()) {
        return;
    }
    if (xIndex >= mapChipData_[yIndex].size()) {
        return;
    }
    mapChipData_[yIndex][xIndex] = type;
}

Vector3 MapChipField::GetMapChipPositionByIndex(
    uint32_t xIndex,
    uint32_t yIndex) const
{
    const uint32_t height = GetBlockHeight();
    const float x = kChipWidth * static_cast<float>(xIndex);
    float y = 0.0f;
    if (height > 0) {
        y = kChipHeight * static_cast<float>(height - 1 - yIndex);
    }
    return { x, y, 0.0f };
}

uint32_t MapChipField::GetBlockWidth() const
{
    if (mapChipData_.empty()) {
        return 0;
    }
    return static_cast<uint32_t>(mapChipData_[0].size());
}

uint32_t MapChipField::GetBlockHeight() const
{
    return static_cast<uint32_t>(mapChipData_.size());
}

LevelData::TileMapData MapChipField::GetTileMapData() const
{
    LevelData::TileMapData tileData;
    tileData.name = "TileMap";
    tileData.height = GetBlockHeight();
    tileData.width = GetBlockWidth();
    tileData.data.resize(tileData.width * tileData.height, 0);

    for (uint32_t y = 0; y < tileData.height; ++y) {
        for (uint32_t x = 0; x < tileData.width; ++x) {
            uint32_t index = y * tileData.width + x;
            tileData.data[index] = static_cast<int32_t>(mapChipData_[y][x]);
        }
    }
    return tileData;
}

std::unordered_map<MapChipType, MapChipConfig> MapChipRegistry::configs_;

void MapChipRegistry::Initialize()
{
    configs_.clear();

    auto Register = [](MapChipType type, const std::string& name, bool isSolid, bool isGimmick, const std::string& fallbackModelPath) {
        std::string modelPath = fallbackModelPath;
        std::string materialType = "";
        std::string texturePath = "";
        if (const auto* metaData = GimmickMetaDataManager::GetInstance()->GetMetaData(name)) {
            modelPath = metaData->defaultModelPath;
            materialType = metaData->materialType;
            texturePath = metaData->defaultTexturePath;
        }
        configs_[type] = { type, name, isSolid, isGimmick, modelPath, materialType, texturePath };
    };

    // ----------------------------------------------------
    // レジストリ（ここで地形ブロックの仕様を一元管理します）
    // ----------------------------------------------------
    
    // 基本的な地形（当たり判定あり、静的描画）
    Register(MapChipType::Block, "Floor", true, false, ""); // 空パスで標準キューブを使用
    Register(MapChipType::Wall,  "Wall",  true, false, ""); // 空パスで標準キューブを使用
    configs_[MapChipType::Foundation] = { MapChipType::Foundation, "Foundation", true, false, "" };
    
    // 【拡張例】もし氷の床を作りたくなったら、ここに1行追加するだけ！
    // Register(MapChipType::IceFloor, "Ice Floor", true, "IceBlock/IceBlock.obj");
    
    // 特殊な壁（ギミックとして処理される）
    Register(MapChipType::DestructibleWall, "DestructibleWall", true, true, "");
}

const MapChipConfig& MapChipRegistry::GetConfig(MapChipType type)
{
    static const MapChipConfig defaultConfig = { MapChipType::Blank, "Unknown", false, false, "", "", "" };
    auto it = configs_.find(type);
    if (it != configs_.end()) {
        return it->second;
    }
    return defaultConfig;
}

bool MapChipRegistry::IsSolidBlock(MapChipType type)
{
    return GetConfig(type).isSolid;
}

const char* MapChipRegistry::GetName(MapChipType type)
{
    return GetConfig(type).name.c_str();
}
