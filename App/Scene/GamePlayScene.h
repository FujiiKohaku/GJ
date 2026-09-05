#pragma once

#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Background/RuinsBackground.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "BaseScene.h"
#ifdef USE_IMGUI
#include "Engine/LevelEditor/MapEditor.h"
#endif
#include "Engine/Fluid/GpuSphFluid.h"
#include "Engine/Fluid/FluidForceRenderer.h"
#include "Engine/Fluid/GpuSphFluidRenderer.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/Camera/Camera.h"
#include "Engine/debugcamera/DebugCameraController.h"
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
    void StartDeathTransition();
    void UpdateDeathTransition(float deltaTime);
    void RespawnPlayerLeavingCorpse();

    std::unique_ptr<Camera> camera_;
    DebugCameraController debugCameraController_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<Text> instructionText_;
    std::unique_ptr<Text> collisionText_;
    MapChipStage mapChipStage_;
    RuinsBackground ruinsBackground_;
    std::unique_ptr<MapChipPlayer> player_;
    std::unique_ptr<GpuSphFluid> gpuSphFluid_;
    std::unique_ptr<FluidForceRenderer> fluidForceRenderer_;
    std::unique_ptr<GpuSphFluidRenderer> gpuSphFluidRenderer_;
    PageTransition::RevealOverlay pageReveal_;

    // メニュー関連
    bool isMenuOpen_ = false;
    std::unique_ptr<Sprite> menuBackgroundSprite_;
    std::unique_ptr<Sprite> menuPanelSprite_;
    std::unique_ptr<Text> menuTitleText_;
    std::unique_ptr<Text> menuInstructionText_;
    bool isDeathTransitionActive_ = false;
    float deathTransitionTime_ = 0.0f;
    bool wasLeftMousePressed_ = false;
    uint32_t emittedParticleTotal_ = 0;
    bool showForces_ = false;
    float eyeOffsetX_ = 0.0f;
    Vector3 playerStartPosition_ = { 0.0f, 0.0f, 0.0f };
    bool selfDestructSlowActive_ = false;
    float timeScaleBeforeSelfDestruct_ = 1.0f;
};
