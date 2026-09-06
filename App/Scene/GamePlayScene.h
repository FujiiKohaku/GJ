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
#include "Engine/Effect/EffectManager.h"
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
    void UpdateLivesText();
    void LoseLife();
    void StartDeathTransition();
    void UpdateDeathTransition(float deltaTime);
    void RespawnPlayerLeavingCorpse();

    void StartLifeRelay();
    void FinishLifeRelay();

    void StartStageSelectTransition();
    void UpdateStageSelectTransition(float deltaTime);
    void UpdateFantasyMenuEffect(float deltaTime);

    std::unique_ptr<Camera> camera_;
    DebugCameraController debugCameraController_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<Text> instructionText_;
    std::unique_ptr<Text> collisionText_;
    std::unique_ptr<Text> livesText_;
    static constexpr int kInitialLives = 5;
    int remainingLives_ = kInitialLives;
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
    std::unique_ptr<Sprite> menuResumeButtonSprite_;
    std::unique_ptr<Sprite> menuGameOverButtonSprite_;
    std::unique_ptr<Sprite> menuStageSelectButtonSprite_;
    std::unique_ptr<Text> menuTitleText_;
    std::unique_ptr<Text> menuResumeText_;
    std::unique_ptr<Text> menuGameOverText_;
    std::unique_ptr<Text> menuStageSelectText_;
    std::unique_ptr<Sprite> menuTransitionFadeSprite_;
    bool isStageSelectTransitionActive_ = false;
    float stageSelectTransitionTime_ = 0.0f;
    float fantasyMenuEffectStrength_ = 0.0f;
    bool isDeathTransitionActive_ = false;
    float deathTransitionTime_ = 0.0f;
    bool showForces_ = false;
    EffectHandle walkingDustEffectHandle_ = kInvalidEffectHandle;
    float eyeOffsetX_ = 0.0f;
    Vector3 playerStartPosition_ = { 0.0f, 0.0f, 0.0f };
    bool selfDestructSlowActive_ = false;
    float timeScaleBeforeSelfDestruct_ = 1.0f;

    bool isLifeRelayActive_ = false;
    float lifeRelayTimer_ = 0.0f;
    float lifeRelayDuration_ = 1.5f;
    Vector3 lifeRelayOrbStartPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 lifeRelayOrbCurrentPosition_ = { 0.0f, 0.0f, 0.0f };
    EffectHandle lifeRelayOrbEffectHandle_ = kInvalidEffectHandle;
};
