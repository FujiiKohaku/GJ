#pragma once

#include "MapChipField.h"
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <memory>
#include <string>
#include <vector>

class MapChipStage {
public:
    ~MapChipStage();

    void Initialize(
        const LevelData::TileMapData& tileMapData,
        const std::string& texturePath =
            "resources/Textures/checkerboard.png");
    void Update();
    void Draw();

    const MapChipField& GetField() const;
    MapChipField& GetField();

private:
    MapChipField field_;
    std::vector<std::unique_ptr<Object3d>> blockObjects_;
    std::vector<std::unique_ptr<BaseMapChipGimmick>> gimmicks_;
};
