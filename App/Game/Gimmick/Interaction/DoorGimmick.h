#pragma once

#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <memory>
#include <vector>
#include <string>

class DoorParam;
class MapChipStage;

class DoorGimmick : public BaseMapChipGimmick {
public:
    DoorGimmick();
    ~DoorGimmick() override;

    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;

    void Update() override;
    void Draw() override;
    void EnableToonLighting() override;
    void SetEditorMode(bool isEditorMode) override { isEditorMode_ = isEditorMode; }
    
    AABB GetAABB() const override;
    bool IsSolid() const override { return isSolid_; }
    void SetStage(MapChipStage* stage) override;

private:
    std::vector<std::unique_ptr<Object3d>> objects_;
    std::unique_ptr<DoorParam> param_;
    MapChipStage* stage_;
    
    Vector3 basePosition_;
    bool isEditorMode_;
    
    bool isOpen_;
    float openProgress_; // 0.0 (Closed) ~ 1.0 (Open)
    bool isSolid_;
};
