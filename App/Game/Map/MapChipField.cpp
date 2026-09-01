#include "MapChipField.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

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
