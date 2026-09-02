#pragma once

#include "BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "PageTransition.h"

#include <memory>
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
    void ApplyPostEffectToggle(PostEffectToggle& toggle);

private:
    std::unique_ptr<Camera> camera_;
    DebugCameraController debugCameraController_;
    std::unique_ptr<Object3d> floor_;
    std::unique_ptr<Object3d> shipObject_;
    float shipRotationY_ = 0.0f;
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
    Vector3 fireworkLaunchPosition_ = { 0.0f, 4.0f, 0.0f };
    PageTransition::RevealOverlay pageReveal_;
};
