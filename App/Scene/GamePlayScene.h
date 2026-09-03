#pragma once

#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "BaseScene.h"
#ifdef USE_IMGUI
#include "Engine/LevelEditor/MapEditor.h"
#endif
#include "Engine/Fluid/GpuSphFluid.h"
#include "Engine/Fluid/FluidForceRenderer.h"
#include "Engine/Fluid/GpuSphFluidRenderer.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/Camera/Camera.h"
#include "PageTransition.h"
#include <memory>

class GamePlayScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    void UpdateFollowCamera();
    void UpdateCollisionText();

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<Text> instructionText_;
    std::unique_ptr<Text> collisionText_;
    MapChipStage mapChipStage_;
    std::unique_ptr<MapChipPlayer> player_;
    std::unique_ptr<GpuSphFluid> gpuSphFluid_;
    std::unique_ptr<FluidForceRenderer> fluidForceRenderer_;
    std::unique_ptr<GpuSphFluidRenderer> gpuSphFluidRenderer_;
    PageTransition::RevealOverlay pageReveal_;
    bool wasPlayerCrushed_ = false;
    bool wasLeftMousePressed_ = false;
    uint32_t emittedParticleTotal_ = 0;
    bool showForces_ = false;
};
