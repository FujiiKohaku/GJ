#include "MapChipField.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

bool MapChipField::LoadMapChipCsv(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    ResetMapChipData();

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream lineStream(line);
        std::vector<MapChipType> row;
        std::string value;

        while (std::getline(lineStream, value, ',')) {
            int typeId = 0;
            try {
                typeId = std::stoi(value);
            } catch (const std::invalid_argument&) {
                typeId = 0;
            } catch (const std::out_of_range&) {
                typeId = 0;
            }
            MapChipType type = static_cast<MapChipType>(typeId);
            row.push_back(type);
        }

        if (!row.empty()) {
            mapChipData_.push_back(row);
        }
    }

    return !mapChipData_.empty();
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
