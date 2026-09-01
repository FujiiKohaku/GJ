#include "MapEditor.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include "Engine/input/Input.h"
#include <fstream>
#include <iostream>
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void MapEditor::Initialize()
{
    isActive_ = false;
    currentFilePath_ = "";
}

void MapEditor::Update()
{
#ifdef USE_IMGUI
    // F12キーでエディタの表示/非表示を切り替え
    if (Input::GetInstance()->IsKeyTrigger(DIK_F12)) {
        isActive_ = !isActive_;
    }

    if (!isActive_) {
        return;
    }

    // ここにエディタ操作中の独自のUpdate処理を記述
#endif
}

void MapEditor::DrawImGui()
{
#ifdef USE_IMGUI
    if (!isActive_) {
        return;
    }

    DrawMapEditorWindow();
#endif
}

void MapEditor::DrawMapEditorWindow()
{
#ifdef USE_IMGUI
    ImGui::Begin("Map Editor");

    ImGui::Text("File:");
    ImGui::SameLine();
    static char filePathBuffer[256] = "resources/Maps/stage1.json";
    ImGui::InputText("##FilePath", filePathBuffer, sizeof(filePathBuffer));

    if (ImGui::Button("Load")) {
        Load(filePathBuffer);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        Save(filePathBuffer);
    }

    ImGui::Separator();

    if (levelData_.tileMaps.empty()) {
        ImGui::Text("No TileMap Data.");
        if (ImGui::Button("Create New TileMap")) {
            LevelData::TileMapData newMap;
            newMap.name = "TileMap1";
            newMap.width = inputWidth_;
            newMap.height = inputHeight_;
            newMap.data.resize(inputWidth_ * inputHeight_, 0);
            levelData_.tileMaps.push_back(newMap);
        }
    } else {
        auto& mapData = levelData_.tileMaps[0];
        
        ImGui::Text("Map Size: %d x %d", mapData.width, mapData.height);
        ImGui::InputInt("Width", &inputWidth_);
        ImGui::InputInt("Height", &inputHeight_);
        
        if (ImGui::Button("Resize Map")) {
            ResizeMap();
        }

        ImGui::Separator();
        ImGui::Text("Palette:");
        ImGui::RadioButton("Eraser (0)", &selectedTileType_, 0); ImGui::SameLine();
        ImGui::RadioButton("Block (1)", &selectedTileType_, 1); ImGui::SameLine();
        ImGui::RadioButton("Player Spawn (99)", &selectedTileType_, 99);

        ImGui::Separator();
        ImGui::Text("Map Grid (Click to paint)");

        // 簡易的なグリッド描画
        ImGui::BeginChild("MapGrid", ImVec2(0, 400), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        float tileSize = 20.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        
        for (int y = 0; y < (int)mapData.height; ++y) {
            for (int x = 0; x < (int)mapData.width; ++x) {
                int index = y * mapData.width + x;
                int tileType = mapData.data[index];

                ImVec2 pMin = ImVec2(p.x + x * tileSize, p.y + y * tileSize);
                ImVec2 pMax = ImVec2(pMin.x + tileSize, pMin.y + tileSize);

                // タイルの色
                ImU32 color = IM_COL32(50, 50, 50, 255);
                if (tileType == 1) {
                    color = IM_COL32(200, 200, 200, 255);
                } else if (tileType == 99) {
                    color = IM_COL32(50, 200, 50, 255);
                }

                drawList->AddRectFilled(pMin, pMax, color);
                drawList->AddRect(pMin, pMax, IM_COL32(100, 100, 100, 255));

                // マウスクリック判定
                ImGui::SetCursorScreenPos(pMin);
                ImGui::InvisibleButton((std::string("##tile") + std::to_string(index)).c_str(), ImVec2(tileSize, tileSize));
                if (ImGui::IsItemActive()) {
                    mapData.data[index] = selectedTileType_;
                }
            }
        }
        
        ImGui::EndChild();
    }

    ImGui::End();
#endif
}

void MapEditor::ResizeMap()
{
    if (levelData_.tileMaps.empty()) return;
    
    auto& mapData = levelData_.tileMaps[0];
    std::vector<int32_t> newData(inputWidth_ * inputHeight_, 0);

    for (int y = 0; y < (std::min)((int)mapData.height, inputHeight_); ++y) {
        for (int x = 0; x < (std::min)((int)mapData.width, inputWidth_); ++x) {
            newData[y * inputWidth_ + x] = mapData.data[y * mapData.width + x];
        }
    }

    mapData.width = inputWidth_;
    mapData.height = inputHeight_;
    mapData.data = std::move(newData);
}

void MapEditor::Load(const std::string& filePath)
{
    LevelDataLoader loader;
    levelData_ = loader.Load(filePath);
    currentFilePath_ = filePath;
    
    if (!levelData_.tileMaps.empty()) {
        inputWidth_ = levelData_.tileMaps[0].width;
        inputHeight_ = levelData_.tileMaps[0].height;
    }
}

void MapEditor::SetLevelData(const LevelData& levelData, const std::string& filePath)
{
    levelData_ = levelData;
    currentFilePath_ = filePath;
    
    if (!levelData_.tileMaps.empty()) {
        inputWidth_ = levelData_.tileMaps[0].width;
        inputHeight_ = levelData_.tileMaps[0].height;
    }
}

void MapEditor::Save(const std::string& filePath)
{
    LevelDataLoader loader;
    loader.Save(filePath, levelData_);
}
