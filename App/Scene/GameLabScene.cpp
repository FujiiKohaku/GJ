#include "GameLabScene.h"

#include "ArchiveScene.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/Time/TimeManager.h"
#include "SceneManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kArchiveRoomModel = "StageSelectBook/ArchiveRoom.obj";
constexpr int32_t kGridHalfExtent = 10;
constexpr float kGridSpacing = 1.0f;
constexpr Vector3 kSmokePreviewPosition = { 3.0f, 0.05f, 2.0f };
constexpr const char* kFlameEffects[] = {
    "FlameSmoke", "Flame", "FlameCore", "FlameSparks"
};
const PostEffectType kPostEffectTypes[] = {
    PostEffectType::Copy,
    PostEffectType::GrayScale,
    PostEffectType::Vignette,
    PostEffectType::DepthOfField,
    PostEffectType::MotionBlur,
    PostEffectType::ChromaticAberration,
    PostEffectType::LensDistortion,
    PostEffectType::FilmGrain,
    PostEffectType::LensDirt,
    PostEffectType::CameraShake,
    PostEffectType::BokehShape,
    PostEffectType::Fisheye,
    PostEffectType::Pixelate,
    PostEffectType::ColorAdjust,
    PostEffectType::smoothing,
    PostEffectType::GaussianFilter,
    PostEffectType::LuminanceBasedOutline,
    PostEffectType::DepthOutline,
    PostEffectType::RadialBlur,
    PostEffectType::Dissolve,
    PostEffectType::Random,
    PostEffectType::Bloom,
    PostEffectType::LensFlare,
    PostEffectType::Glare,
    PostEffectType::LightShafts,
    PostEffectType::VolumetricLight,
    PostEffectType::AnamorphicFlare,
    PostEffectType::Halo,
    PostEffectType::LightStreak,
    PostEffectType::NeonGlow,
    PostEffectType::GhostImage,
    PostEffectType::Outline,
    PostEffectType::Fog,
    PostEffectType::FocusLine,
    PostEffectType::Paint,
    PostEffectType::GlassCrack,
    PostEffectType::Shockwave,
    PostEffectType::HeatHaze,
    PostEffectType::SonicBoom,
    PostEffectType::RainDrops,
    PostEffectType::CyberScanline,
    PostEffectType::HexShield,
    PostEffectType::BlackHoleDistortion,
    PostEffectType::ArchiveAtmosphere,
};
}

void GameLabScene::Initialize()
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    previousTimeScale_ = TimeManager::GetInstance()->GetTimeScale();
    previousArchiveApproach_ = sceneManager->GetArchiveApproach();
    previousVignetteStrength_ = sceneManager->GetVignetteStrength();
    previousPaintProgress_ = sceneManager->GetPaintProgress();
    previousPaintIntensity_ = sceneManager->GetPaintIntensity();
    previousSonicBoomProgress_ = sceneManager->GetSonicBoomProgress();
    postEffectPreviewProgress_ = 0.05f;
    sceneManager->ClearPostEffects();
    sceneManager->SetPostEffectType(PostEffectType::ArchiveAtmosphere);
    sceneManager->SetArchiveApproach(0.0f);
    sceneManager->SetPostEffectKickStrength(1.0f);
    sceneManager->SetPaintIntensity(1.0f);
    UpdatePostEffectPreviewParameters();
    DebugRenderer::GetInstance()->SetVisible(true);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt({ 0.0f, 3.8f, -35.0f }, { 0.0f, -1.0f, 1.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    debugCameraController_.SetTargetCamera(camera_.get());
    debugCameraController_.SetDebugMode(true);

    EffectManager::GetInstance()->SetCamera(camera_.get());
    smokeEffectHandle_ = kInvalidEffectHandle;
    isSmokeEnabled_ = false;
    flameEffectHandles_.fill(kInvalidEffectHandle);
    isFlameEnabled_ = false;

    InitializePostEffectList();

    backdrop_ = std::make_unique<Object3d>();
    backdrop_->Initialize(Object3dManager::GetInstance());
    backdrop_->SetModel(ModelManager::GetInstance()->Load(kArchiveRoomModel));
    backdrop_->SetEnableLighting(false);
    backdrop_->SetColor({ 0.58f, 0.61f, 0.65f, 1.0f });
    backdrop_->Update();

    // 死亡ぽよぽよスライムシャワーの初期化
    deathSlimeShower_ = std::make_unique<DeathSlimeShower>();
    deathSlimeShower_->Initialize(120);

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("STAGE 03  GAMELAB");
    titleText_->SetPosition({ 640.0f, 68.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(48.0f);
    titleText_->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f });
    titleText_->SetOutlineColor({ 0.02f, 0.03f, 0.06f, 1.0f });
    titleText_->SetOutlineWidth(2.0f);

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText(
        "WASD : MOVE   Q : DOWN   E : UP   ARROWS / LEFT DRAG : LOOK   "
        "BACKSPACE : STAGE SELECT");
    instructionText_->SetPosition({ 640.0f, 660.0f });
    instructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    instructionText_->SetFontSize(16.0f);
    instructionText_->SetColor({ 0.72f, 0.80f, 0.88f, 1.0f });
    instructionText_->SetOutlineColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    instructionText_->SetOutlineWidth(2.0f);

    pageReveal_.InitializeIfRequested();
}

void GameLabScene::Finalize()
{
    if (EffectManager::GetInstance()->IsEffectAlive(smokeEffectHandle_)) {
        EffectManager::GetInstance()->StopEffect(smokeEffectHandle_);
    }
    smokeEffectHandle_ = kInvalidEffectHandle;
    // Retire this scene's particles and lights, including extinguishing tails.
    EffectManager::GetInstance()->StopAllEffects();
    flameEffectHandles_.fill(kInvalidEffectHandle);
    EffectManager::GetInstance()->SetCamera(nullptr);

    TimeManager::GetInstance()->SetTimeScale(previousTimeScale_);
    SceneManager* sceneManager = SceneManager::GetInstance();
    sceneManager->SetArchiveApproach(previousArchiveApproach_);
    sceneManager->SetVignetteStrength(previousVignetteStrength_);
    sceneManager->SetPaintProgress(previousPaintProgress_);
    sceneManager->SetPaintIntensity(previousPaintIntensity_);
    sceneManager->SetSonicBoomProgress(previousSonicBoomProgress_);
    sceneManager->ClearPostEffects();
    debugCameraController_.SetTargetCamera(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    DebugRenderer::GetInstance()->SetVisible(false);
}

void GameLabScene::Update()
{
    if (Input::GetInstance()->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<ArchiveScene>());
        return;
    }

    DebugRenderer::GetInstance()->SetVisible(true);
    debugCameraController_.SetDebugMode(true);

    UpdatePostEffectPreviewParameters();
    UpdateSmokePreview();
    UpdateFlamePreview();
    EffectManager::GetInstance()->Update();
    debugCameraController_.Update();
    debugCameraController_.SetDebugMode(true);

    camera_->Update();
    EffectManager::GetInstance()->SetCamera(camera_.get());
    EffectManager::GetInstance()->UpdatePerView();
    backdrop_->Update();

    if (deathSlimeShower_) {
        deathSlimeShower_->Update(TimeManager::GetInstance()->GetDeltaTime());
    }
    titleText_->Update();
    instructionText_->Update();
    pageReveal_.Update(TimeManager::GetInstance()->GetDeltaTime());
}

void GameLabScene::Draw2D()
{
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    instructionText_->Draw();
    pageReveal_.Draw();
}

void GameLabScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    backdrop_->Draw();
    if (deathSlimeShower_) {
        deathSlimeShower_->Draw();
    }
}

void GameLabScene::DrawParticle()
{
    EffectManager* effectManager = EffectManager::GetInstance();
    effectManager->PreDraw();
    effectManager->Draw();
}

void GameLabScene::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(410.0f, 620.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("GAMELAB - Engine Post Effects");

    if (ImGui::Button("START TIME")) {
        TimeManager::GetInstance()->SetTimeScale(1.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("STOP TIME")) {
        TimeManager::GetInstance()->SetTimeScale(0.0f);
    }

    ImGui::Separator();
    ImGui::Checkbox("Smoke", &isSmokeEnabled_);

    ImGui::Separator();
    ImGui::Checkbox("Flame (white.png)", &isFlameEnabled_);
    ImGui::DragFloat3("Flame Position", &flamePosition_.x, 0.05f, -20.0f, 20.0f);
    if (ImGui::Button("Focus Flame")) {
        camera_->LookAt(
            { flamePosition_.x + 3.0f, flamePosition_.y + 2.5f, flamePosition_.z - 6.0f },
            { flamePosition_.x, flamePosition_.y + 1.2f, flamePosition_.z });
        camera_->Update();
        debugCameraController_.SetTargetCamera(camera_.get());
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "--- FIREWORK EFFECTS ---");
    ImGui::DragFloat3("Launch Pos", &fireworkLaunchPosition_.x, 0.1f, -20.0f, 20.0f);

    if (ImGui::Button("LAUNCH BLUE FIREWORK")) {
        EffectManager::GetInstance()->PlayEffect("BlueFireworkSparks", fireworkLaunchPosition_);
    }
    ImGui::SameLine();
    if (ImGui::Button("LAUNCH 6-DIR TRAIL")) {
        EffectManager::GetInstance()->PlayEffect("SixDirectionFireworkTrails", fireworkLaunchPosition_);
    }

    if (ImGui::Button("LAUNCH COMBO FIREWORK")) {
        EffectManager::GetInstance()->PlayEffect("BlueFireworkSparks", fireworkLaunchPosition_);
        EffectManager::GetInstance()->PlayEffect("SixDirectionFireworkTrails", fireworkLaunchPosition_);
    }

    if (deathSlimeShower_) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), "--- POYO POYO SLIME SHOWER ---");
        ImGui::Text("Active Slimes: %u", deathSlimeShower_->GetActiveCount());

        if (ImGui::Button("DROP SLIME RAIN (30)")) {
            deathSlimeShower_->SpawnRain(30, { 0.0f, 10.0f, 0.0f }, 5.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("DROP SHOWER (80)")) {
            deathSlimeShower_->SpawnRain(80, { 0.0f, 12.0f, 0.0f }, 6.0f);
        }
        if (ImGui::Button("CLEAR SLIMES")) {
            deathSlimeShower_->Clear();
        }

        ImGui::SliderFloat("Slime Gravity", &deathSlimeShower_->gravity, -40.0f, -5.0f);
        ImGui::SliderFloat("Bounce Restitution", &deathSlimeShower_->restitution, 0.1f, 0.95f);
        ImGui::SliderFloat("Poyo Stiffness (Spring)", &deathSlimeShower_->springStiffness, 30.0f, 400.0f);
        ImGui::SliderFloat("Poyo Damping", &deathSlimeShower_->springDamping, 1.0f, 30.0f);
    }

    ImGui::Separator();
    for (PostEffectToggle& toggle : postEffectToggles_) {
        bool enabled = toggle.enabled;
        const char* effectName = GetPostEffectTypeName(toggle.type);
        if (ImGui::Checkbox(effectName, &enabled)) {
            toggle.enabled = enabled;
            ApplyPostEffectToggle(toggle);
        }
    }

    ImGui::End();
#endif
}

void GameLabScene::InitializePostEffectList()
{
    postEffectToggles_.clear();
    for (PostEffectType type : kPostEffectTypes) {
        PostEffectToggle toggle;
        toggle.type = type;
        if (type == PostEffectType::Copy) {
            toggle.enabled = true;
        }
        postEffectToggles_.push_back(toggle);
    }
}

void GameLabScene::UpdatePostEffectPreviewParameters()
{
    postEffectPreviewProgress_ +=
        TimeManager::GetInstance()->GetDeltaTime() * 0.45f;
    if (postEffectPreviewProgress_ >= 1.0f) {
        postEffectPreviewProgress_ -= 1.0f;
    }

    SceneManager* sceneManager = SceneManager::GetInstance();
    sceneManager->SetArchiveApproach(postEffectPreviewProgress_);
    sceneManager->SetVignetteStrength(postEffectPreviewProgress_);
    sceneManager->SetPaintProgress(postEffectPreviewProgress_);
    sceneManager->SetSonicBoomProgress(postEffectPreviewProgress_);
}

void GameLabScene::UpdateSmokePreview()
{
    EffectManager* effectManager = EffectManager::GetInstance();
    if (isSmokeEnabled_) {
        if (!effectManager->IsEffectAlive(smokeEffectHandle_)) {
            smokeEffectHandle_ = effectManager->PlayLoopEffect(
                "Smoke",
                kSmokePreviewPosition);
        }
        effectManager->SetEffectPosition(
            smokeEffectHandle_,
            kSmokePreviewPosition);
        return;
    }

    if (effectManager->IsEffectAlive(smokeEffectHandle_)) {
        effectManager->StopEffect(smokeEffectHandle_);
    }
    smokeEffectHandle_ = kInvalidEffectHandle;
}

void GameLabScene::UpdateFlamePreview()
{
    EffectManager* effects = EffectManager::GetInstance();
    for (size_t index = 0; index < flameEffectHandles_.size(); ++index) {
        EffectHandle& handle = flameEffectHandles_[index];
        if (isFlameEnabled_) {
            if (!effects->IsEffectAlive(handle)) {
                handle = effects->PlayLoopEffect(kFlameEffects[index], flamePosition_);
            }
            effects->SetEffectPosition(handle, flamePosition_);
        } else if (handle != kInvalidEffectHandle) {
            effects->StopEffect(handle);
            handle = kInvalidEffectHandle;
        }
    }
}

void GameLabScene::ApplyPostEffectToggle(PostEffectToggle& toggle)
{
    SceneManager* sceneManager = SceneManager::GetInstance();
    if (toggle.enabled) {
        sceneManager->AddPostEffect(
            toggle.type,
            PostEffectStage::AfterParticle);
        return;
    }
    sceneManager->RemovePostEffect(toggle.type);
}

void GameLabScene::DrawDebugGrid()
{
    DebugRenderer* debugRenderer = DebugRenderer::GetInstance();
    const Vector4 gridColor = { 0.20f, 0.72f, 0.58f, 1.0f };
    const Vector4 axisXColor = { 1.0f, 0.25f, 0.25f, 1.0f };
    const Vector4 axisZColor = { 0.25f, 0.55f, 1.0f, 1.0f };
    const float extent = static_cast<float>(kGridHalfExtent) * kGridSpacing;

    for (int32_t index = -kGridHalfExtent; index <= kGridHalfExtent; ++index) {
        const float coordinate = static_cast<float>(index) * kGridSpacing;
        Vector4 xLineColor = gridColor;
        Vector4 zLineColor = gridColor;
        float xLineThickness = 1.0f;
        float zLineThickness = 1.0f;

        if (index == 0) {
            xLineColor = axisZColor;
            zLineColor = axisXColor;
            xLineThickness = 2.5f;
            zLineThickness = 2.5f;
        }

        debugRenderer->AddLine(
            { coordinate, 0.01f, -extent },
            { coordinate, 0.01f, extent },
            xLineColor,
            xLineThickness);
        debugRenderer->AddLine(
            { -extent, 0.01f, coordinate },
            { extent, 0.01f, coordinate },
            zLineColor,
            zLineThickness);
    }

    debugRenderer->AddWireOBB(
        { 0.0f, -0.25f, 0.0f },
        { 20.0f, 0.5f, 20.0f },
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
        { 1.0f, 0.82f, 0.20f, 1.0f },
        2.0f);
}
