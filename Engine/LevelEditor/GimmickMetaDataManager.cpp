#include "GimmickMetaDataManager.h"
#include <fstream>
#include <cassert>
#include "../externals/json.hpp"
#include "Engine/Logger/Logger.h"

GimmickMetaDataManager* GimmickMetaDataManager::GetInstance()
{
    static GimmickMetaDataManager instance;
    return &instance;
}

void GimmickMetaDataManager::Initialize(const std::string& masterFilePath)
{
    metaDatas_.clear();

    std::ifstream file(masterFilePath);
    if (file.fail()) {
        Logger::Log("Failed to load GimmickMaster.json: " + masterFilePath);
        return;
    }

    nlohmann::json root;
    file >> root;

    for (auto it = root.begin(); it != root.end(); ++it) {
        std::string type = it.key();
        const nlohmann::json& objJson = it.value();

        GimmickMetaData metaData;
        if (objJson.contains("file_name")) {
            metaData.defaultModelPath = objJson["file_name"].get<std::string>();
        }

        metaDatas_[type] = metaData;
    }
}

const GimmickMetaData* GimmickMetaDataManager::GetMetaData(const std::string& type) const
{
    auto it = metaDatas_.find(type);
    if (it != metaDatas_.end()) {
        return &it->second;
    }
    return nullptr;
}
