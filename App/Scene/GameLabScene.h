#pragma once

#include "BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include "PageTransition.h"

#include <memory>

class GameLabScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    void DrawDebugGrid();

private:
    std::unique_ptr<Camera> camera_;
    DebugCameraController debugCameraController_;
    std::unique_ptr<Object3d> floor_;
    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> instructionText_;
    PageTransition::RevealOverlay pageReveal_;
};
