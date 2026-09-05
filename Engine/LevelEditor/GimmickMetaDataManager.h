#pragma once

#include <string>
#include <unordered_map>
#include <memory>

struct GimmickMetaData {
    std::string defaultModelPath;
    std::string materialType;
    std::string defaultTexturePath;
};

class GimmickMetaDataManager {
public:
    static GimmickMetaDataManager* GetInstance();

    void Initialize(const std::string& masterFilePath = "resources/Data/GimmickMaster.json");

    const GimmickMetaData* GetMetaData(const std::string& type) const;

private:
    GimmickMetaDataManager() = default;
    ~GimmickMetaDataManager() = default;
    GimmickMetaDataManager(const GimmickMetaDataManager&) = delete;
    GimmickMetaDataManager& operator=(const GimmickMetaDataManager&) = delete;

    std::unordered_map<std::string, GimmickMetaData> metaDatas_;
};
