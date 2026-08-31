#include "TestScene.h"

#include "ClearScene.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Fluid/FluidForceRenderer.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/Time/TimeManager.h"
#include "GameOverScene.h"
#include "SceneManager.h"
#include "StageSelectScene.h"

namespace {
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
}

void TestScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt({ 0.0f, 2.1f, -6.2f }, { 0.0f, 1.05f, 0.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    gpuSphFluid_ = std::make_unique<GpuSphFluid>();
    gpuSphFluid_->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

    fluidForceRenderer_ = std::make_unique<FluidForceRenderer>();
    fluidForceRenderer_->Initialize(DirectXCommon::GetInstance());

    SceneManager::GetInstance()->SetScreenSpaceFluid(gpuSphFluid_.get());
    

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("SLIME FLUID TEST");
    titleText_->SetPosition({ 640.0f, 56.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(36.0f);
    titleText_->SetColor({ 0.65f, 1.0f, 0.78f, 1.0f });

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText("WASD : MOVE   SPACE : BOUNCE   LEFT/J : SPIKE   RIGHT/K : HAMMER   R : RESET");
    instructionText_->SetPosition({ 640.0f, 100.0f });
    instructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    instructionText_->SetFontSize(18.0f);
    instructionText_->SetColor({ 0.76f, 0.88f, 0.92f, 1.0f });
}

void TestScene::Finalize()
{
    SceneManager::GetInstance()->SetScreenSpaceFluid(nullptr);
    if (gpuSphFluid_) {
        gpuSphFluid_->Finalize();
    }
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void TestScene::Update()
{
    Input* input = Input::GetInstance();
    if (input->IsKeyTrigger(DIK_C)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<ClearScene>());
        return;
    }
    if (input->IsKeyTrigger(DIK_G)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<GameOverScene>());
        return;
    }
    if (input->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<StageSelectScene>());
        return;
    }
    if (input->IsKeyTrigger(DIK_R)) {
        gpuSphFluid_->Reset(GpuSphFluid::Settings());
    }
    
    if (input->IsKeyTrigger(DIK_Y)) {
        showForces_ = !showForces_;
    }

    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    
    Vector3 moveInput = { 0.0f, 0.0f, 0.0f };
    if (input->IsKeyPressed(DIK_W)) moveInput.z += 1.0f;
    if (input->IsKeyPressed(DIK_S)) moveInput.z -= 1.0f;
    if (input->IsKeyPressed(DIK_A)) moveInput.x -= 1.0f;
    if (input->IsKeyPressed(DIK_D)) moveInput.x += 1.0f;
    
    Vector3 targetVelocity = { 0.0f, 0.0f, 0.0f };
    if (Vector3Length(moveInput) > 0.0f) {
        moveInput = Normalize(moveInput);
        targetVelocity = { moveInput.x * 2.0f, 0.0f, moveInput.z * 2.0f };
    }

    static Vector3 corePos = { 0.0f, 0.16f, 0.0f };
    corePos.x += targetVelocity.x * deltaTime;
    corePos.z += targetVelocity.z * deltaTime;

    gpuSphFluid_->SetLiquidated(input->IsMousePressed(1)); // 右クリック中は流体化
    gpuSphFluid_->SetControlState(corePos, targetVelocity, {0.0f, 0.0f, 1.0f});
    gpuSphFluid_->Update(deltaTime);
    
    camera_->Update();
    titleText_->Update();
    instructionText_->Update();
}

void TestScene::Draw2D()
{
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    instructionText_->Draw();
}

void TestScene::Draw3D()
{
    if (showForces_) {
        fluidForceRenderer_->Draw(*gpuSphFluid_, *camera_);
    }

    

    DebugRenderer* debugRenderer = DebugRenderer::GetInstance();
    const Vector4 floorColor = { 0.20f, 0.34f, 0.42f, 1.0f };
    const Vector4 boundsColor = { 0.22f, 0.62f, 0.48f, 1.0f };
    constexpr float minX = -2.15f;
    constexpr float maxX = 2.15f;
    constexpr float minZ = -1.85f;
    constexpr float maxZ = 1.85f;
    constexpr float gridStep = 0.25f;

    for (int index = 0; index <= 17; ++index) {
        const float x = minX + gridStep * static_cast<float>(index);
        debugRenderer->AddLine(
            { x, 0.01f, minZ },
            { x, 0.01f, maxZ },
            floorColor,
            1.0f);
    }
    for (int index = 0; index <= 15; ++index) {
        const float z = minZ + gridStep * static_cast<float>(index);
        debugRenderer->AddLine(
            { minX, 0.01f, z },
            { maxX, 0.01f, z },
            floorColor,
            1.0f);
    }

    debugRenderer->AddWireOBB(
        { 0.0f, 1.85f, 0.0f },
        { 4.3f, 2.65f, 3.7f },
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
        boundsColor,
        1.0f);

}

void TestScene::DrawParticle()
{
}

void TestScene::DrawImGui()
{
}
