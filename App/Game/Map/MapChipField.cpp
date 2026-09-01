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
    uint32_t width = 0;
    for (const std::vector<MapChipType>& row : mapChipData_) {
        const uint32_t rowWidth = static_cast<uint32_t>(row.size());
        if (rowWidth > width) {
            width = rowWidth;
        }
    }
    return width;
}

uint32_t MapChipField::GetBlockHeight() const
{
    return static_cast<uint32_t>(mapChipData_.size());
}
