#include "MapChipStage.h"

#include "App/Game/Gimmick/MapChipGimmickFactory.h"
#include "App/Game/Gimmick/GoalGimmick.h"
#include "App/Game/Gimmick/SwingingBridgeGimmick.h"
#include "App/Game/Gimmick/Interaction/SwitchGimmick.h"
#include "App/Game/Gimmick/Interaction/GasEmitterGimmick.h"
#include "App/Game/Gimmick/Interaction/DestructibleWallGimmick.h"
#include "App/Game/Gimmick/Trap/SpikeGimmick.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/Object3dManager.h"
#include <algorithm>

MapChipStage::~MapChipStage() = default;

void MapChipStage::Initialize(
    const LevelData& levelData,
    const std::string& texturePath)
{
    // ギミックは数が少ないので毎回作り直す
    gimmicks_.clear();
    eventManager_.Clear();
    
    if (levelData.tileMaps.empty()) return;
    field_.Initialize(levelData.tileMaps[0]);

    Model* blockModel =
        ModelManager::GetInstance()->CreateCube(texturePath);
    const uint32_t height = field_.GetBlockHeight();
    const uint32_t width = field_.GetBlockWidth();

    size_t currentBlockIndex = 0;

    for (uint32_t yIndex = 0; yIndex < height; ++yIndex) {
        for (uint32_t xIndex = 0; xIndex < width; ++xIndex) {
            const MapChipType type =
                field_.GetMapChipTypeByIndex(xIndex, yIndex);
            if (type == MapChipType::Blank) {
                continue;
            }

            const Vector3 position =
                field_.GetMapChipPositionByIndex(xIndex, yIndex);

            if (!MapChipRegistry::IsSolidBlock(type) || MapChipRegistry::GetConfig(type).isGimmick) {
                // 同じ座標の ObjectData を探す
                const BaseGimmickParam* gimmickParam = nullptr;
                for (const auto& obj : levelData.objects) {
                    if (std::abs(obj.translation.x - position.x) < 0.1f &&
                        std::abs(obj.translation.y - position.y) < 0.1f) {
                        gimmickParam = obj.gimmickParam.get();
                        break;
                    }
                }
                
                std::unique_ptr<BaseMapChipGimmick> gimmick =
                    MapChipGimmickFactory::Create(
                        type,
                        position,
                        texturePath,
                        gimmickParam);
                if (gimmick) {
                    gimmick->SetStage(this);
                    gimmicks_.push_back(std::move(gimmick));
                }
                continue;
            }

            const auto& config = MapChipRegistry::GetConfig(type);
            Model* model = blockModel;
            // "Cube" などの特別な識別子を判定するか、既存のLoadを使う
            // 今回は "StoneBlock/StoneBlock.obj" が設定されているはずなのでそれをロードする
            if (!config.modelPath.empty()) {
                model = ModelManager::GetInstance()->Load(config.modelPath);
            } else if (!config.texturePath.empty()) {
                model = ModelManager::GetInstance()->CreateCube(config.texturePath);
            }

            // GPUメモリ枯渇(VRAMリーク)を防ぐため、既存の Object3d を再利用する
            if (currentBlockIndex < blockObjects_.size()) {
                blockObjects_[currentBlockIndex]->SetModel(model); // 追加: 正しいモデルを設定
                blockObjects_[currentBlockIndex]->SetTranslate(position);
                blockObjects_[currentBlockIndex]->Update();
            } else {
                std::unique_ptr<Object3d> block =
                    std::make_unique<Object3d>();
                block->Initialize(Object3dManager::GetInstance());
                block->SetModel(model);
                block->SetTranslate(position);
                block->SetEnableLighting(true);
                block->Update();
                blockObjects_.push_back(std::move(block));
            }
            // Reused objects must lose the material mode of their previous tile.
            Object3d* placedBlock = blockObjects_[currentBlockIndex].get();
            placedBlock->SetEnableLighting(true);
            placedBlock->SetEnableEnvironmentMap(false);
            placedBlock->SetEnvironmentMapStrength(0.0f);
            if (type == MapChipType::Foundation) {
                placedBlock->GetMaterial()->enableLighting = 8;
            }
            currentBlockIndex++;
        }
    }

    // 余った(使われなくなった)ブロックを配列から削除
    if (currentBlockIndex < blockObjects_.size()) {
        blockObjects_.erase(blockObjects_.begin() + currentBlockIndex, blockObjects_.end());
    }

    // objects 配列に独立して存在するギミック（例: Goal）を追加で作成
    for (const auto& obj : levelData.objects) {
        std::unique_ptr<BaseMapChipGimmick> gimmick = nullptr;
        std::string modelFile;

        if (obj.type == "Goal") {
            gimmick = std::make_unique<GoalGimmick>();
            modelFile = obj.fileName;
        } else if (obj.type == "Switch") {
            gimmick = std::make_unique<SwitchGimmick>();
            modelFile = obj.fileName;
        } else if (obj.type == "GasEmitter") {
            gimmick = std::make_unique<GasEmitterGimmick>();
            modelFile = obj.fileName;
        } else if (obj.type == "DestructibleWall") {
            gimmick = std::make_unique<DestructibleWallGimmick>();
            modelFile = obj.fileName;
        } else if (obj.type == "Spike") {
            gimmick = std::make_unique<SpikeGimmick>();
            modelFile = obj.fileName;
        }

        if (gimmick) {
            if (gimmick->Initialize(obj.translation, modelFile, obj.gimmickParam.get())) {
                gimmick->SetStage(this);
                gimmicks_.push_back(std::move(gimmick));
            }
        }
    }
}

void MapChipStage::ApplyMaterialProperties()
{
    // 全オブジェクトにToonLightingを適用
    for (const std::unique_ptr<Object3d>& block : blockObjects_) {
        // Foundation keeps its procedural stone material in gameplay as well.
        if (block->GetMaterial()->enableLighting == 8) {
            continue;
        }
        block->EnableToonLighting();
    }
    for (const std::unique_ptr<BaseMapChipGimmick>& gimmick : gimmicks_) {
        gimmick->EnableToonLighting();
    }

    // マテリアルタイプに応じた特殊処理 (例: Moss の表面高さ計算)
    const uint32_t width = field_.GetBlockWidth();
    const uint32_t height = field_.GetBlockHeight();
    std::vector<float> surfaceHeights(width, 0.0f);
    
    size_t blockIndex = 0;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            MapChipType type = field_.GetMapChipTypeByIndex(x, y);
            if (!MapChipRegistry::IsSolidBlock(type) || MapChipRegistry::GetConfig(type).isGimmick) {
                continue; // ソリッドな静的ブロック以外はスキップ
            }
            
            const auto& config = MapChipRegistry::GetConfig(type);
            
            // "Moss" マテリアルの特殊なシェーダー設定
            if (config.materialType == "Moss") {
                // 上面に空きがあるか判定し、高さを記録
                if (y == 0 || field_.GetMapChipTypeByIndex(x, y - 1) != type) {
                    surfaceHeights[x] = field_.GetMapChipPositionByIndex(x, y).y + 0.5f;
                }
                
                if (blockIndex < blockObjects_.size()) {
                    Material* material = blockObjects_[blockIndex]->GetMaterial();
                    material->enableLighting = 7; // Moss用のライティングモード
                    material->environmentCoefficient = surfaceHeights[x];
                }
            }
            
            ++blockIndex;
        }
    }
}

void MapChipStage::Update()
{
    for (std::unique_ptr<Object3d>& block : blockObjects_) {
        block->Update();
    }
    
    for (std::unique_ptr<BaseMapChipGimmick>& gimmick : gimmicks_) {
        gimmick->SetEditorMode(isEditorMode_);
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

MapChipField& MapChipStage::GetField()
{
    return field_;
}

std::vector<BaseMapChipGimmick*> MapChipStage::GetGimmicks() const
{
    std::vector<BaseMapChipGimmick*> result;
    result.reserve(gimmicks_.size());
    for (const auto& gimmick : gimmicks_) {
        result.push_back(gimmick.get());
    }
    return result;
}

void MapChipStage::AddGimmick(std::unique_ptr<BaseMapChipGimmick> gimmick)
{
    if (!gimmick) {
        return;
    }
    gimmick->SetStage(this);
    gimmicks_.push_back(std::move(gimmick));
    if (gimmicks_.back()->IsHardenedSlime()) {
        ResolveHardenedSlimeAdhesion(*gimmicks_.back());
    }
}

void MapChipStage::ResolveHardenedSlimeAdhesion(
    const BaseMapChipGimmick& hardenedSlime)
{
    const std::vector<AABB> bodyBoxes = hardenedSlime.GetCollisionBoxes();
    std::vector<SwingingBridgeGimmick*> touchedBridges;
    bool touchesTerrain = false;

    for (const AABB& bodyBox : bodyBoxes) {
        for (uint32_t y = 0; y < field_.GetBlockHeight(); ++y) {
            for (uint32_t x = 0; x < field_.GetBlockWidth(); ++x) {
                const MapChipType type = field_.GetMapChipTypeByIndex(x, y);
                if (!MapChipRegistry::IsSolidBlock(type)) {
                    continue;
                }
                const AABB terrainBox = {
                    field_.GetMapChipPositionByIndex(x, y),
                    { 1.0f, 1.0f, 1.0f } };
                if (CollisionManager::Intersect(bodyBox, terrainBox).isHit) {
                    touchesTerrain = true;
                }
            }
        }

        for (const std::unique_ptr<BaseMapChipGimmick>& gimmick : gimmicks_) {
            auto* bridge = dynamic_cast<SwingingBridgeGimmick*>(gimmick.get());
            if (!bridge ||
                !CollisionManager::Intersect(bodyBox, bridge->GetAABB()).isHit) {
                continue;
            }
            if (std::find(
                    touchedBridges.begin(),
                    touchedBridges.end(),
                    bridge) == touchedBridges.end()) {
                touchedBridges.push_back(bridge);
            }
        }
    }

    for (SwingingBridgeGimmick* bridge : touchedBridges) {
        bridge->ApplyAdhesive();
    }

    if (touchesTerrain || touchedBridges.size() >= 2) {
        for (SwingingBridgeGimmick* bridge : touchedBridges) {
            bridge->ForceStuck();
        }
    }
}

std::vector<BaseMapChipGimmick*> MapChipStage::GetGimmicksInSphere(const Vector3& center, float radius)
{
    std::vector<BaseMapChipGimmick*> result;
    float radiusSq = radius * radius;
    for (const auto& gimmick : gimmicks_) {
        AABB aabb = gimmick->GetAABB();
        Vector3 diff = aabb.center - center;
        if (Vector3LengthSquared(diff) <= radiusSq) {
            result.push_back(gimmick.get());
        }
    }
    return result;
}

void MapChipStage::CreateSpark(const Vector3& origin)
{
    // 全ギミックにスパークが発生したことを通知する
    // ガス発生装置などがこれを受け取って引火判定を行う
    for (const auto& gimmick : gimmicks_) {
        gimmick->OnSpark(origin);
    }
}

void MapChipStage::CreateExplosion(const Vector3& origin, float radius)
{
    // 範囲内の全ギミックに爆発の被害を与える
    auto targets = GetGimmicksInSphere(origin, radius);
    for (auto* target : targets) {
        target->OnExplosion(origin, radius);
    }
}
