#pragma once
#include "../externals/json.hpp"
#include "LevelData.h"
#include <string>

class LevelDataLoader {
public:
    LevelData Load(const std::string& filePath);
    void Save(const std::string& filePath, const LevelData& levelData);

private:
    void LoadObject(const nlohmann::json& objectJson, LevelData& levelData);
};