#include "GamePlayScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Input/Input.h"
#include "Engine/Logger/Logger.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "SceneManager.h"
#include "ArchiveScene.h"
#include "GameOverScene.h"
#include <algorithm>
#include <format>
#include <string>
#include <vector>
#include <Windows.h>

namespace {
constexpr const char* kSkyBoxTexture = "resources/Textures/skybox.dds";
constexpr const char* kMapChipTexture = "resources/Textures/checkerboard.png";
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kStage1Json = "resources/Maps/stage1.json";
constexpr float kCameraDistance = 12.0f;
constexpr const char* kDefaultFont ="resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr float kFluidRenderZ = -0.72f;
constexpr Vector3 kSlimeRenderForward = { 0.0f, 0.0f, 1.0f };

Vector3 MakeFluidCorePosition(const MapChipPlayer& player)
{
    Vector3 corePosition = player.GetPosition();
    corePosition.z = kFluidRenderZ;
    return corePosition;
}

Vector3 MakeFluidEmitterPosition(const MapChipPlayer& player)
{
    return MakeFluidCorePosition(player);
}

Vector3 MakeFluidTargetVelocity(const Vector3& playerVelocity)
{
    return {
        playerVelocity.x,
        playerVelocity.y,
        0.0f
    };
}

GpuSphFluid::CollisionObstacle MakeFluidObstacle(
    const Vector3& center,
    const Vector3& size,
    const Vector3& velocity)
{
    GpuSphFluid::CollisionObstacle obstacle {};
    obstacle.center = { center.x, center.y, kFluidRenderZ };
    obstacle.halfSize = {
        size.x * 0.5f,
        size.y * 0.5f,
        0.65f
    };
    obstacle.velocity = { velocity.x, velocity.y, 0.0f };
    return obstacle;
}

bool IsLeftMouseButtonDown(Input* input)
{
    return input->IsMousePressed(0) ||
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

std::vector<GpuSphFluid::CollisionObstacle> BuildFluidObstacles(
    const MapChipStage& stage,
    const std::vector<BaseMapChipGimmick*>& gimmicks,
    float deltaTime)
{
    std::vector<GpuSphFluid::CollisionObstacle> obstacles;
    const MapChipField& field = stage.GetField();
    obstacles.reserve(
        static_cast<size_t>(field.GetBlockWidth()) *
            static_cast<size_t>(field.GetBlockHeight()) +
        gimmicks.size());

    for (uint32_t y = 0; y < field.GetBlockHeight(); ++y) {
        for (uint32_t x = 0; x < field.GetBlockWidth(); ++x) {
            if (field.GetMapChipTypeByIndex(x, y) != MapChipType::Block) {
                continue;
            }

            obstacles.push_back(MakeFluidObstacle(
                field.GetMapChipPositionByIndex(x, y),
                { 1.0f, 1.0f, 1.0f },
                { 0.0f, 0.0f, 0.0f }));
        }
    }

    const float safeDeltaTime = (std::max)(deltaTime, 0.0001f);
    for (BaseMapChipGimmick* gimmick : gimmicks) {
        if (!gimmick->IsSolid()) continue;
        const AABB box = gimmick->GetAABB();
        const Vector3 delta = gimmick->GetDeltaPosition();
        obstacles.push_back(MakeFluidObstacle(
            box.center,
            box.size,
            {
                delta.x / safeDeltaTime,
                delta.y / safeDeltaTime,
                delta.z / safeDeltaTime
            }));
    }

    return obstacles;
}
}

void GamePlayScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
    SceneManager::GetInstance()->SetSlimeScreenProgress(0.0f);
    isDeathTransitionActive_ = false;
    deathTransitionTime_ = 0.0f;

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    LevelDataLoader loader;
    LevelData levelData = loader.Load(kStage1Json);

    mapChipStage_.Initialize(levelData);

    Vector3 playerStartPos = { 0.0f, 0.0f, 0.0f };
    if (!levelData.playerSpawns.empty()) {
        playerStartPos = levelData.playerSpawns[0].translation;
    }

    player_ = std::make_unique<MapChipPlayer>();
    player_->Initialize(&mapChipStage_.GetField(), playerStartPos);
    mapChipStage_.SetPlayer(player_.get());
    
    gpuSphFluid_ = std::make_unique<GpuSphFluid>();
    GpuSphFluid::Settings fluidSettings;
    fluidSettings.particleCount = 1280;
    fluidSettings.particleRadius = 0.14f;
    fluidSettings.smoothingRadius = 0.45f;
    fluidSettings.blobRadii = { 0.65f, 0.65f, 0.65f }; // 完全な球体形状
    fluidSettings.stiffness = 22.0f;        // SPH圧力反発力
    fluidSettings.shapeAttraction = 9.0f;   // Shape Matchingバネ強度
    fluidSettings.velocityAttraction = 8.5f; // 移動速度同期
    fluidSettings.viscosity = 2.8f;         // まとまりのある粘性
    fluidSettings.surfaceTension = 5.0f;    // ピンと張った滑らかな水面（表面張力）
    fluidSettings.gravity = { 0.0f, -16.0f, 0.0f }; // 重力
    fluidSettings.damping = 0.25f;          // 振動ダンピング制御
    fluidSettings.horizontalFriction = 0.95f;
    fluidSettings.liquidShapeAttraction = 0.0f;
    fluidSettings.liquidVelocityAttraction = 0.0f;
    fluidSettings.liquidViscosity = 1.15f;
    fluidSettings.liquidSurfaceTension = 1.8f;
    fluidSettings.liquidDamping = 0.04f;
    fluidSettings.liquidHorizontalFriction = 0.992f;
    fluidSettings.liquidGravityScale = 1.45f;
    fluidSettings.sloshStrength = 0.0f;
    fluidSettings.puddleSpread = 0.0f;
    fluidSettings.emitterRate = 560.0f;
    fluidSettings.emitterRadius = 0.20f;
    fluidSettings.emitterSpeed = 6.4f;
    fluidSettings.particleLifetime = 6.0f;
    fluidSettings.collisionFriction = 0.78f;
    fluidSettings.collisionBounce = 0.06f;
    fluidSettings.simulationSubsteps = 3;
    fluidSettings.corePosition = MakeFluidCorePosition(*player_);
    fluidSettings.floorHeight = player_->GetFluidFloorHeight();

    fluidSettings.boundsMin = {
        -4.0f,
        -20.0f,
        kFluidRenderZ - 2.0f
    };
    fluidSettings.boundsMax = {
        static_cast<float>(mapChipStage_.GetField().GetBlockWidth()) + 4.0f,
        static_cast<float>(mapChipStage_.GetField().GetBlockHeight()) + 8.0f,
        kFluidRenderZ + 2.0f
    };
    gpuSphFluid_->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), fluidSettings);
    gpuSphFluid_->SetLiquidated(false); // スライムプレイヤーとしてのまとまった形状を維持
    
    fluidForceRenderer_ = std::make_unique<FluidForceRenderer>();
    fluidForceRenderer_->Initialize(DirectXCommon::GetInstance());

    gpuSphFluidRenderer_ = std::make_unique<GpuSphFluidRenderer>();
    gpuSphFluidRenderer_->Initialize(DirectXCommon::GetInstance());
    
    SceneManager::GetInstance()->SetScreenSpaceFluid(gpuSphFluid_.get());
    Logger::Log(std::format(
        "[GamePlayScene] GpuSphFluid initialized. particles={} core=({:.2f},{:.2f},{:.2f}) floor={:.2f} boundsMin=({:.2f},{:.2f},{:.2f}) boundsMax=({:.2f},{:.2f},{:.2f})\n",
        fluidSettings.particleCount,
        fluidSettings.corePosition.x,
        fluidSettings.corePosition.y,
        fluidSettings.corePosition.z,
        fluidSettings.floorHeight,
        fluidSettings.boundsMin.x,
        fluidSettings.boundsMin.y,
        fluidSettings.boundsMin.z,
        fluidSettings.boundsMax.x,
        fluidSettings.boundsMax.y,
        fluidSettings.boundsMax.z));
    
    UpdateFollowCamera();
    camera_->Update();

    TextureManager::GetInstance()->LoadTexture(kSkyBoxTexture);
    skyBox_ = std::make_unique<SkyBox>();
    skyBox_->Initialize(DirectXCommon::GetInstance());
    skyBox_->SetTexture(kSkyBoxTexture);
    skyBox_->Update(camera_.get());

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText(
        "MOVE : A/D OR LEFT/RIGHT   JUMP : SPACE/W/UP   "
        "TAB : MENU   BACKSPACE : STAGE SELECT");
    instructionText_->SetPosition({ 32.0f, 32.0f });
    instructionText_->SetFontSize(24.0f);
    instructionText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    instructionText_->SetOutlineColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    instructionText_->SetOutlineWidth(2.0f);

    collisionText_ = std::make_unique<Text>();
    collisionText_->Initialize(kDefaultFont);
    collisionText_->SetPosition({ 32.0f, 68.0f });
    collisionText_->SetFontSize(22.0f);
    collisionText_->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f });
    collisionText_->SetOutlineColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    collisionText_->SetOutlineWidth(2.0f);
    UpdateCollisionText();

    // メニューUIの初期化
    TextureManager::GetInstance()->LoadTexture(kWhiteTexture);

    // 全画面半透明暗幕
    menuBackgroundSprite_ = std::make_unique<Sprite>();
    menuBackgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    menuBackgroundSprite_->SetSize({ 1280.0f, 720.0f });
    menuBackgroundSprite_->SetPosition({ 0.0f, 0.0f });
    menuBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.65f });

    // 中央パネル
    menuPanelSprite_ = std::make_unique<Sprite>();
    menuPanelSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    menuPanelSprite_->SetSize({ 640.0f, 380.0f });
    menuPanelSprite_->SetPosition({ 320.0f, 170.0f });
    menuPanelSprite_->SetColor({ 0.08f, 0.12f, 0.16f, 0.95f });

    // メニュータイトル
    menuTitleText_ = std::make_unique<Text>();
    menuTitleText_->Initialize(kDefaultFont);
    menuTitleText_->SetText("PAUSE MENU");
    menuTitleText_->SetPosition({ 640.0f, 230.0f });
    menuTitleText_->SetAnchorPoint({ 0.5f, 0.5f });
    menuTitleText_->SetFontSize(44.0f);
    menuTitleText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // メニュー説明文
    menuInstructionText_ = std::make_unique<Text>();
    menuInstructionText_->Initialize(kDefaultFont);
    menuInstructionText_->SetText("TAB : RESUME GAME\n\nG : GAME OVER\n\nBACKSPACE : STAGE SELECT");
    menuInstructionText_->SetPosition({ 640.0f, 360.0f });
    menuInstructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    menuInstructionText_->SetFontSize(24.0f);
    menuInstructionText_->SetColor({ 0.8f, 0.88f, 0.95f, 1.0f });

    pageReveal_.InitializeIfRequested();
}

void GamePlayScene::Finalize()
{
    SceneManager::GetInstance()->RemovePostEffect(PostEffectType::SlimeScreen);
    SceneManager::GetInstance()->SetSlimeScreenProgress(0.0f);
    SceneManager::GetInstance()->SetScreenSpaceFluid(nullptr);
    if (gpuSphFluid_) {
        gpuSphFluid_->Finalize();
    }
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void GamePlayScene::Update()
{
    Input* input = Input::GetInstance();
    pageReveal_.Update(TimeManager::GetInstance()->GetDeltaTime());

    if (isDeathTransitionActive_) {
        UpdateDeathTransition(TimeManager::GetInstance()->GetDeltaTime());
        return;
    }

    // TABキーでメニュー開閉
    if (Input::GetInstance()->IsKeyTrigger(DIK_TAB)) {
        isMenuOpen_ = !isMenuOpen_;
    }

    // メニューが開いているときはゲーム内処理を行わずに早期リターン
    if (isMenuOpen_) {
        if (Input::GetInstance()->IsKeyTrigger(DIK_G)) {
            StartDeathTransition();
            return;
        }
        if (Input::GetInstance()->IsKeyTrigger(DIK_BACKSPACE)) {
            SceneManager::GetInstance()->SetNextScene(
                std::make_unique<ArchiveScene>());
            return;
        }

        menuBackgroundSprite_->Update();
        menuPanelSprite_->Update();
        menuTitleText_->Update();
        menuInstructionText_->Update();
        return;
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<ArchiveScene>());
        return;
    }
    
    if (input->IsKeyTrigger(DIK_Y)) {
        showForces_ = !showForces_;
    }

    mapChipStage_.Update(); // Playerの前にGimmickを更新して移動量を出しておくのが理想的
    player_->Update(mapChipStage_.GetGimmicks());
    if (player_->IsCrushed()) {
        StartDeathTransition();
        return;
    }
    if (gpuSphFluid_) {
        const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
        const std::vector<BaseMapChipGimmick*> gimmicks = mapChipStage_.GetGimmicks();
        gpuSphFluid_->SetObstacles(
            BuildFluidObstacles(mapChipStage_, gimmicks, deltaTime));
        gpuSphFluid_->SetFloorHeight(player_->GetFluidFloorHeight());
        Vector3 targetRadii = player_->IsGrounded()
            ? Vector3{ 0.76f, 0.48f, 0.76f }
            : Vector3{ 0.65f, 0.65f, 0.65f };
        gpuSphFluid_->SetBlobRadii(targetRadii);
        float minX = -1000.0f, maxX = 1000.0f, maxY = 1000.0f;
        player_->GetWallBoundaries(minX, maxX, maxY, gimmicks);
        Vector3 corePos = MakeFluidCorePosition(*player_);
        float minZ = corePos.z - 0.22f;
        float maxZ = corePos.z + 0.22f;
        gpuSphFluid_->SetWallBoundaries(minX, maxX, minZ, maxZ, -1000.0f, maxY);
        gpuSphFluid_->SetLiquidated(false);
        gpuSphFluid_->SetControlState(
            MakeFluidCorePosition(*player_),
            MakeFluidTargetVelocity(player_->GetVelocity()),
            kSlimeRenderForward);
        const bool leftMousePressed = IsLeftMouseButtonDown(input);
        const bool leftMouseTriggered =
            leftMousePressed && !wasLeftMousePressed_;
        const Vector3 emitterPosition = MakeFluidEmitterPosition(*player_);
        if (leftMouseTriggered) {
            constexpr uint32_t kClickBurstParticleCount = 260;
            gpuSphFluid_->TriggerEmitBurst(kClickBurstParticleCount);
            emittedParticleTotal_ += kClickBurstParticleCount;
            Logger::Log(std::format(
                "[GamePlayScene] Liquid emit burst. source=({:.2f},{:.2f},{:.2f}) total={}\n",
                emitterPosition.x,
                emitterPosition.y,
                emitterPosition.z,
                emittedParticleTotal_));
        }
        gpuSphFluid_->SetEmitter(
            leftMousePressed,
            emitterPosition,
            MakeFluidTargetVelocity(player_->GetVelocity()));
        wasLeftMousePressed_ = leftMousePressed;
        gpuSphFluid_->Update(deltaTime);
    }
    UpdateFollowCamera();
    camera_->Update();
    skyBox_->Update(camera_.get());
    instructionText_->Update();
    UpdateCollisionText();
    collisionText_->Update();
}

void GamePlayScene::Draw2D()
{
    if (isDeathTransitionActive_) {
        return;
    }
    if (showForces_) {
        fluidForceRenderer_->Draw(*gpuSphFluid_, *camera_);
    }

    TextRenderer::GetInstance()->PreDraw();
    instructionText_->Draw();
    collisionText_->Draw();
    pageReveal_.Draw();

    // メニュー表示中は最前面に暗幕とメニューパネルを描画
    if (isMenuOpen_) {
        SpriteManager::GetInstance()->PreDraw();
        menuBackgroundSprite_->Draw();
        menuPanelSprite_->Draw();

        TextRenderer::GetInstance()->PreDraw();
        menuTitleText_->Draw();
        menuInstructionText_->Draw();
    }
}

void GamePlayScene::Draw3D()
{
    SkyBoxManager::GetInstance()->PreDraw();
    skyBox_->Draw(DirectXCommon::GetInstance()->GetCommandList());

    Object3dManager::GetInstance()->PreDraw();
    mapChipStage_.Draw();
}

void GamePlayScene::DrawParticle()
{
    if (showForces_ && gpuSphFluid_ && gpuSphFluidRenderer_) {
        gpuSphFluidRenderer_->Draw(*gpuSphFluid_, *camera_);
    }
}

void GamePlayScene::DrawImGui()
{
}

void GamePlayScene::StartDeathTransition()
{
    if (isDeathTransitionActive_) {
        return;
    }
    isDeathTransitionActive_ = true;
    deathTransitionTime_ = 0.0f;
    isMenuOpen_ = false;
    SceneManager* sceneManager = SceneManager::GetInstance();
    const Vector3 position = player_->GetPosition();
    sceneManager->SetPaintSeed(position.x * 17.31f + position.y * 7.13f);
    sceneManager->SetSlimeScreenProgress(0.0f);
    sceneManager->AddPostEffect(
        PostEffectType::SlimeScreen, PostEffectStage::AfterParticle);
}

void GamePlayScene::UpdateDeathTransition(float deltaTime)
{
    constexpr float kCoverDuration = 2.1f;
    deathTransitionTime_ += deltaTime;
    const float progress = std::clamp(deathTransitionTime_ / kCoverDuration, 0.0f, 1.0f);
    SceneManager::GetInstance()->SetSlimeScreenProgress(progress);
    if (progress >= 1.0f) {
        PageTransition::RequestSlimeReveal();
        SceneManager::GetInstance()->SetNextScene(std::make_unique<GameOverScene>());
    }
}

void GamePlayScene::UpdateFollowCamera()
{
    if (!player_) {
        return;
    }
    const Vector3 playerPosition = player_->GetPosition();
    camera_->LookAt(
        { playerPosition.x, playerPosition.y, -kCameraDistance },
        { playerPosition.x, playerPosition.y, 0.0f });
}

void GamePlayScene::UpdateCollisionText()
{
    if (!collisionText_ || !player_) {
        return;
    }

    std::string state = "AIR";
    if (player_->IsCrushed()) {
        state = "CRUSHED / LIQUID";
    } else if (player_->IsGrounded()) {
        state = "GROUND COLLISION";
    } else if (player_->IsColliding()) {
        state = "WALL/CEILING COLLISION";
    }
    const Vector3 position = player_->GetPosition();
    collisionText_->SetText(
        "COLLISION : " + state +
        "   PLAYER X=" + std::to_string(position.x) +
        " Y=" + std::to_string(position.y) +
        "   LIQUID BURST=" + std::to_string(emittedParticleTotal_));
}
