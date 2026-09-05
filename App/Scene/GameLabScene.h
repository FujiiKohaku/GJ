#pragma once

#include "BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "PageTransition.h"
#include "App/Effect/DeathSlimeShower.h"
#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Background/RuinsBackground.h"

#include <memory>
#include <array>
#include <vector>

class GameLabScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;
    bool WantsImGuiAlways() const override { return true; }

private:
    struct PostEffectToggle {
        PostEffectType type = PostEffectType::Copy;
        bool enabled = false;
    };

    void DrawDebugGrid();
    void InitializePostEffectList();
    void UpdatePostEffectPreviewParameters();
    void UpdateSmokePreview();
    void UpdateFlamePreview();
    void ApplyPostEffectToggle(PostEffectToggle& toggle);

private:
    std::unique_ptr<Camera> camera_;
    DebugCameraController debugCameraController_;
    RuinsBackground ruinsBackground_;
    MapChipStage stagePreview_;
    bool showStageBlocks_ = true;
    bool showLabFloor_ = true;
    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> instructionText_;
    std::vector<PostEffectToggle> postEffectToggles_;
    float previousTimeScale_ = 1.0f;
    float previousArchiveApproach_ = 0.0f;
    float previousVignetteStrength_ = 1.0f;
    float previousPaintProgress_ = 0.0f;
    float previousPaintIntensity_ = 1.0f;
    float previousSonicBoomProgress_ = 0.0f;
    float postEffectPreviewProgress_ = 0.05f;
    EffectHandle smokeEffectHandle_ = kInvalidEffectHandle;
    bool isSmokeEnabled_ = false;
    std::array<EffectHandle, 4> flameEffectHandles_ = {
        kInvalidEffectHandle, kInvalidEffectHandle,
        kInvalidEffectHandle, kInvalidEffectHandle };
    bool isFlameEnabled_ = false;
    Vector3 flamePosition_ = { 0.0f, 0.05f, -2.0f };
    Vector3 fireworkLaunchPosition_ = { 0.0f, 4.0f, 0.0f };
    std::unique_ptr<DeathSlimeShower> deathSlimeShower_;
    PageTransition::RevealOverlay pageReveal_;
};
