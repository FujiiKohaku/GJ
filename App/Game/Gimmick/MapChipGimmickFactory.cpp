#include "MapChipGimmickFactory.h"

#include "BaseMapChipGimmick.h"
#include "MovingBlockGimmick.h"
#include "Trap/SpikeGimmick.h"
#include "Trap/LaserGimmick.h"
#include "SwingingBridgeGimmick.h"
#include "Interaction/DoorGimmick.h"
#include "Trap/GearGimmick.h"
#include "CrumblingFloorGimmick.h"

std::unique_ptr<BaseMapChipGimmick> MapChipGimmickFactory::Create(
    MapChipType type,
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    std::unique_ptr<BaseMapChipGimmick> gimmick;

    switch (type) {
    case MapChipType::MovingBlock:
        gimmick = std::make_unique<MovingBlockGimmick>();
        break;
    case MapChipType::Spike:
        gimmick = std::make_unique<SpikeGimmick>();
        break;
    case MapChipType::LaserEmitter:
        gimmick = std::make_unique<LaserGimmick>();
        break;
    case MapChipType::SwingingBridge:
        gimmick = std::make_unique<SwingingBridgeGimmick>();
        break;
    case MapChipType::Door:
        gimmick = std::make_unique<DoorGimmick>();
        break;
    case MapChipType::Gear:
        gimmick = std::make_unique<GearGimmick>();
        break;
    case MapChipType::CrumblingFloor:
        gimmick = std::make_unique<CrumblingFloorGimmick>();
        break;
    default:
        return nullptr;
    }

    if (gimmick && !gimmick->Initialize(position, texturePath, gimmickParam)) {
        return nullptr;
    }
    return gimmick;
}
