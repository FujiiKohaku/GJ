#pragma once

#include "Engine/LevelEditor/LevelData.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>
#include <string>
#include <vector>

enum class MapChipType {
    Blank = 0,
    Block = 1,
    MovingBlock = 2,
    Spike = 3,
    Goal = 4,
    PressurePlate = 5,
    GasEmitter = 6,
    Bonfire = 7,
    DestructibleWall = 8,
    LaserEmitter = 9,
};

class MapChipField {
public:
    void Initialize(const LevelData::TileMapData& tileMapData);
    void ResetMapChipData();

    MapChipType GetMapChipTypeByIndex(
        uint32_t xIndex,
        uint32_t yIndex) const;
    void SetMapChipTypeByIndex(
        uint32_t xIndex,
        uint32_t yIndex,
        MapChipType type);
    Vector3 GetMapChipPositionByIndex(
        uint32_t xIndex,
        uint32_t yIndex) const;

    LevelData::TileMapData GetTileMapData() const;

    uint32_t GetBlockWidth() const;
    uint32_t GetBlockHeight() const;

private:
    static constexpr float kChipWidth = 1.0f;
    static constexpr float kChipHeight = 1.0f;

    std::vector<std::vector<MapChipType>> mapChipData_;
};
