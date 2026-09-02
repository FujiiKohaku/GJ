#include "GameLabScene.h"

#include "ArchiveScene.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/Time/TimeManager.h"
#include "SceneManager.h"

namespace {
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kFloorTexture = "resources/Textures/checkerboard.png";
constexpr int32_t kGridHalfExtent = 10;
constexpr float kGridSpacing = 1.0f;
}

void GameLabScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
    DebugRenderer::GetInstance()->SetVisible(true);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt({ 0.0f, 4.0f, -12.0f }, { 0.0f, 0.0f, 0.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    debugCameraController_.SetTargetCamera(camera_.get());
    debugCameraController_.SetDebugMode(true);

    floor_ = std::make_unique<Object3d>();
    floor_->Initialize(Object3dManager::GetInstance());
    floor_->SetModel(ModelManager::GetInstance()->CreateCube(kFloorTexture));
    floor_->SetScale({ 20.0f, 0.5f, 20.0f });
    floor_->SetTranslate({ 0.0f, -0.25f, 0.0f });
    floor_->SetColor({ 0.30f, 0.36f, 0.42f, 1.0f });
    floor_->SetEnableLighting(false);
    floor_->Update();

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
    debugCameraController_.Update();
    debugCameraController_.SetDebugMode(true);

    camera_->Update();
    floor_->Update();
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
    DrawDebugGrid();
}

void GameLabScene::DrawParticle()
{
}

void GameLabScene::DrawImGui()
{
    camera_->DrawImGui();
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
