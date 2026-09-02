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
constexpr const char* kFloorTexture = "resources/Textures/checkerboard.png";
constexpr const char* kShipModelPath = "sailing_ship.obj";
constexpr int32_t kGridHalfExtent = 10;
constexpr float kGridSpacing = 1.0f;
constexpr Vector3 kSmokePreviewPosition = { 3.0f, 0.05f, 2.0f };
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
    sceneManager->AddPostEffect(
        PostEffectType::Copy,
        PostEffectStage::AfterParticle);
    sceneManager->SetPostEffectKickStrength(1.0f);
    sceneManager->SetPaintIntensity(1.0f);
    UpdatePostEffectPreviewParameters();
    DebugRenderer::GetInstance()->SetVisible(true);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt({ 0.0f, 4.0f, -12.0f }, { 0.0f, 0.0f, 0.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    debugCameraController_.SetTargetCamera(camera_.get());
    debugCameraController_.SetDebugMode(true);

    EffectManager::GetInstance()->SetCamera(camera_.get());
    smokeEffectHandle_ = kInvalidEffectHandle;
    isSmokeEnabled_ = false;

    InitializePostEffectList();

    floor_ = std::make_unique<Object3d>();
    floor_->Initialize(Object3dManager::GetInstance());
    floor_->SetModel(ModelManager::GetInstance()->CreateCube(kFloorTexture));
    floor_->SetScale({ 20.0f, 0.5f, 20.0f });
    floor_->SetTranslate({ 0.0f, -0.25f, 0.0f });
    floor_->SetColor({ 0.30f, 0.36f, 0.42f, 1.0f });
    floor_->SetEnableLighting(false);
    floor_->Update();

    // 帆船モデルの初期化
    ModelManager::GetInstance()->Load(kShipModelPath);
    shipObject_ = std::make_unique<Object3d>();
    shipObject_->Initialize(Object3dManager::GetInstance());
    shipObject_->SetModel(kShipModelPath);
    shipObject_->SetTranslate({ 0.0f, 1.2f, 0.0f });
    shipObject_->SetScale({ 0.6f, 0.6f, 0.6f });
    shipObject_->SetEnableLighting(true);
    shipObject_->Update();

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("STAGE 03  GAMELAB");
    titleText_->SetPosition({ 32.0f, 32.0f });
    titleText_->SetFontSize(30.0f);
    titleText_->SetColor({ 0.45f, 1.0f, 0.72f, 1.0f });
    titleText_->SetOutlineColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    titleText_->SetOutlineWidth(2.0f);

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText(
        "WASD : MOVE   Q : DOWN   E : UP   ARROWS / LEFT DRAG : LOOK   "
        "BACKSPACE : STAGE SELECT");
    instructionText_->SetPosition({ 32.0f, 72.0f });
    instructionText_->SetFontSize(18.0f);
    instructionText_->SetColor({ 0.82f, 0.90f, 1.0f, 1.0f });
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
    EffectManager::GetInstance()->Update();
    debugCameraController_.Update();
    debugCameraController_.SetDebugMode(true);

    camera_->Update();
    EffectManager::GetInstance()->SetCamera(camera_.get());
    EffectManager::GetInstance()->UpdatePerView();
    floor_->Update();

    // 帆船のアニメーション（ゆっくり回転＋波揺れ）
    if (shipObject_) {
        float dt = TimeManager::GetInstance()->GetDeltaTime();
        shipRotationY_ += 0.3f * dt;
        float waveY = 1.2f + std::sin(shipRotationY_ * 2.5f) * 0.08f;
        float tiltZ = std::sin(shipRotationY_ * 1.8f) * 0.06f;
        float pitchX = std::cos(shipRotationY_ * 1.8f) * 0.04f;
        shipObject_->SetTranslate({ 0.0f, waveY, 0.0f });
        shipObject_->SetRotate({ pitchX, shipRotationY_, tiltZ });
        shipObject_->Update();
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
    floor_->Draw();
    if (shipObject_) {
        shipObject_->Draw();
    }
    DrawDebugGrid();
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
