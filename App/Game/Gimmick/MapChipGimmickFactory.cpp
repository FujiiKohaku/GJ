#include "MapChipGimmickFactory.h"

#include "BaseMapChipGimmick.h"
#include "MovingBlockGimmick.h"
#include "Trap/SpikeGimmick.h"
#include "Trap/LaserGimmick.h"

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
    default:
        return nullptr;
    }

    if (gimmick && !gimmick->Initialize(position, texturePath, gimmickParam)) {
        return nullptr;
    }
    return gimmick;
}
