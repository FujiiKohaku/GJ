#pragma once

#include "BaseMapChipGimmick.h"
#include "Engine/3D/Object3d.h"
#include <memory>

class MovingBlockGimmick : public BaseMapChipGimmick {
public:
    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const LevelData::ObjectData::GimmickData* gimmickData = nullptr) override;
    void Update() override;
    void Draw() override;
    void SetEditorMode(bool isEditorMode) override { isEditorMode_ = isEditorMode; }

private:
    std::unique_ptr<Object3d> object_;
    Vector3 basePosition_ = { 0.0f, 0.0f, 0.0f };
    float elapsedTime_ = 0.0f;
    
    // ギミックパラメータ
    float speed_ = 2.0f;
    Vector3 range_ = { 1.5f, 0.0f, 0.0f };
    Vector3 axis_ = { 0.0f, 1.0f, 0.0f };
    bool isEditorMode_ = false;
};
