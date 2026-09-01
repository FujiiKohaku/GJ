#include "MapChipStage.h"

#include "App/Game/Gimmick/MapChipGimmickFactory.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"

MapChipStage::~MapChipStage() = default;

void MapChipStage::Initialize(
    const LevelData::TileMapData& tileMapData,
    const std::string& texturePath)
{
    blockObjects_.clear();
    gimmicks_.clear();
    field_.Initialize(tileMapData);

    Model* blockModel =
        ModelManager::GetInstance()->CreateCube(texturePath);
    const uint32_t height = field_.GetBlockHeight();
    const uint32_t width = field_.GetBlockWidth();

    for (uint32_t yIndex = 0; yIndex < height; ++yIndex) {
        for (uint32_t xIndex = 0; xIndex < width; ++xIndex) {
            const MapChipType type =
                field_.GetMapChipTypeByIndex(xIndex, yIndex);
            if (type == MapChipType::Blank) {
                continue;
            }

            const Vector3 position =
                field_.GetMapChipPositionByIndex(xIndex, yIndex);

            if (type != MapChipType::Block) {
                std::unique_ptr<BaseMapChipGimmick> gimmick =
                    MapChipGimmickFactory::Create(
                        type,
                        position,
                        texturePath);
                if (gimmick) {
                    gimmicks_.push_back(std::move(gimmick));
                }
                continue;
            }

            std::unique_ptr<Object3d> block =
                std::make_unique<Object3d>();
            block->Initialize(Object3dManager::GetInstance());
            block->SetModel(blockModel);
            block->SetTranslate(position);
            block->SetEnableLighting(true);
            block->Update();
            blockObjects_.push_back(std::move(block));
        }
    }
}

void MapChipStage::Update()
{
    for (std::unique_ptr<Object3d>& block : blockObjects_) {
        block->Update();
    }
    for (std::unique_ptr<BaseMapChipGimmick>& gimmick : gimmicks_) {
        gimmick->Update();
    }
}

void MapChipStage::Draw()
{
    for (std::unique_ptr<Object3d>& block : blockObjects_) {
        block->Draw();
    }
    for (std::unique_ptr<BaseMapChipGimmick>& gimmick : gimmicks_) {
        gimmick->Draw();
    }
}

const MapChipField& MapChipStage::GetField() const
{
    return field_;
}
