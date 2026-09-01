#pragma once
#include "Engine/LevelEditor/LevelData.h"
#include <string>

class MapEditor {
public:
    MapEditor() = default;
    ~MapEditor() = default;

    void Initialize();
    void Update();
    void DrawImGui();

    void Load(const std::string& filePath);
    void Save(const std::string& filePath);

    void SetLevelData(const LevelData& levelData, const std::string& filePath);

    bool IsActive() const { return isActive_; }
    void SetActive(bool active) { isActive_ = active; }

    const LevelData& GetLevelData() const { return levelData_; }

private:
    void DrawMapEditorWindow();
    void ResizeMap();

private:
    bool isActive_ = false;
    std::string currentFilePath_;
    LevelData levelData_;

    // ImGui用の一時変数
    int inputWidth_ = 100;
    int inputHeight_ = 20;
    int selectedTileType_ = 1; // 0=Empty, 1=Block
};
