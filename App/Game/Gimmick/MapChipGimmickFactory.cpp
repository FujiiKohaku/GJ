#include "MapChipGimmickFactory.h"

#include "BaseMapChipGimmick.h"
#include "MovingBlockGimmick.h"

std::unique_ptr<BaseMapChipGimmick> MapChipGimmickFactory::Create(
    MapChipType type,
    const Vector3& position,
    const std::string& texturePath)
{
    std::unique_ptr<BaseMapChipGimmick> gimmick;

    switch (type) {
    case MapChipType::MovingBlock:
        gimmick = std::make_unique<MovingBlockGimmick>();
        break;
    default:
        return nullptr;
    }

    if (!gimmick->Initialize(position, texturePath)) {
        return nullptr;
    }
    return gimmick;
}
