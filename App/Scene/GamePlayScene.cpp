#include "GamePlayScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"
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
#include "App/Game/Gimmick/HardenedFluidSlimeCorpse.h"
#include <algorithm>
#include <cmath>
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
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr float kFluidRenderZ = 0.0f;
constexpr float kNeoWorldScale = 0.36f;
constexpr Vector3 kSlimeRenderForward = { 0.0f, 0.0f, 1.0f };

Vector3 MakeFluidCorePosition(const MapChipPlayer& player)
{
    Vector3 corePosition = player.IsShapingSelfDestruct()
        ? player.GetAABB().center
        : player.GetPosition();
    // neo_Engineの形状比率を保ち、GJのワールド寸法へ一律縮小する。
    // 最下部の休止粒子が床の衝突面へ届き、接地時に底が平らになる高さ。
    corePosition.y += 0.086f * kNeoWorldScale;
    corePosition.z = kFluidRenderZ;
    return corePosition;
}

class HardenedSlimeBody final : public BaseMapChipGimmick {
public:
    explicit HardenedSlimeBody(const AABB& bounds)
        : bounds_(bounds)
    {
    }

    bool Initialize(
        const Vector3&,
        const std::string&,
        const BaseGimmickParam*) override
    {
        // 外部モデル(slime_mesh.obj)は一切使わず、変形した自爆形状(bounds_)に100%一致するCubeモデルで生成する。
        object_ = std::make_unique<Object3d>();
        object_->Initialize(Object3dManager::GetInstance());
        Model* cubeModel = ModelManager::GetInstance()->CreateCube("resources/Textures/white.png");
        object_->SetModel(cubeModel);
        object_->SetTranslate({
            bounds_.center.x,
            bounds_.center.y,
            kFluidRenderZ });
        object_->SetScale(bounds_.size);
        object_->SetColor({ 0.20f, 0.95f, 0.65f, 1.0f });
        object_->SetEnableLighting(true);
        object_->Update();

        collisionBoxes_.clear();
        collisionBoxes_.push_back(bounds_);
        return true;
    }

    void Update() override
    {
        if (object_) {
            object_->Update();
        }
    }

    void Draw() override
    {
        if (object_) {
            object_->Draw();
        }
    }

    AABB GetAABB() const override { return bounds_; }
    std::vector<AABB> GetCollisionBoxes() const override
    {
        return collisionBoxes_;
    }
    bool IsHardenedSlime() const override { return true; }

private:
    AABB bounds_ {};
    std::vector<AABB> collisionBoxes_;
    std::unique_ptr<Object3d> object_;
};

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
            const MapChipType type = field.GetMapChipTypeByIndex(x, y);
            if (type != MapChipType::Block && type != MapChipType::Foundation) {
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
        const Vector3 delta = gimmick->GetDeltaPosition();
        for (const AABB& box : gimmick->GetCollisionBoxes()) {
            obstacles.push_back(MakeFluidObstacle(
                box.center,
                box.size,
                {
                    delta.x / safeDeltaTime,
                    delta.y / safeDeltaTime,
                    delta.z / safeDeltaTime
                }));
        }
    }

    return obstacles;
}
}

void GamePlayScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::ArchiveAtmosphere);
    SceneManager::GetInstance()->SetArchiveApproach(0.0f);
    SceneManager::GetInstance()->SetSlimeScreenProgress(0.0f);
    isDeathTransitionActive_ = false;
    deathTransitionTime_ = 0.0f;
    remainingLives_ = kInitialLives;

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    debugCameraController_.SetTargetCamera(camera_.get());
    debugCameraController_.SetDebugMode(false);

    LevelDataLoader loader;
    LevelData levelData = loader.Load(kStage1Json);

    mapChipStage_.Initialize(levelData);
    mapChipStage_.ApplyMaterialProperties();

    RuinsBackground::Settings backgroundSettings;
    backgroundSettings.mapLength =
        static_cast<float>(mapChipStage_.GetField().GetBlockWidth());
    ruinsBackground_.Initialize(backgroundSettings);

    Vector3 playerStartPos = { 0.0f, 0.0f, 0.0f };
    if (!levelData.playerSpawns.empty()) {
        playerStartPos = levelData.playerSpawns[0].translation;
    }

    player_ = std::make_unique<MapChipPlayer>();
    player_->Initialize(&mapChipStage_.GetField(), playerStartPos);
    playerStartPosition_ = playerStartPos;
    mapChipStage_.SetPlayer(player_.get());
    
    gpuSphFluid_ = std::make_unique<GpuSphFluid>();
    GpuSphFluid::Settings fluidSettings;
    fluidSettings.particleCount = 2048;
    fluidSettings.particleRadius = 0.20f * kNeoWorldScale;
    fluidSettings.smoothingRadius = 0.40f * kNeoWorldScale;
    fluidSettings.particleMass =
        kNeoWorldScale * kNeoWorldScale * kNeoWorldScale;
    fluidSettings.restDensity = 3.0f;
    fluidSettings.blobRadii = {1.2f * kNeoWorldScale,0.85f * kNeoWorldScale,1.2f * kNeoWorldScale };
    fluidSettings.stiffness = 50.0f;
    fluidSettings.shapeAttraction = 80.0f;
    fluidSettings.velocityAttraction = 0.0f;
    fluidSettings.viscosity = 15.0f;
    fluidSettings.surfaceTension = 0.0f;
    fluidSettings.gravity = { 0.0f, -20.0f * kNeoWorldScale, 0.0f };
    fluidSettings.damping = 0.985f;
    fluidSettings.horizontalFriction = 0.60f;
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
    fluidSettings.emitterRadius = 0.20f * kNeoWorldScale;
    fluidSettings.emitterSpeed = 6.4f * kNeoWorldScale;
    fluidSettings.particleLifetime = 6.0f;
    fluidSettings.collisionFriction = 0.60f;
    fluidSettings.collisionBounce = 0.30f;
    fluidSettings.simulationSubsteps = 1;
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
        "T : SLOW/SHAPE, T AGAIN : SELF-DESTRUCT   R : RESTART   "
        "F1 : FREE CAM   TAB : MENU   BACKSPACE : STAGE SELECT");
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

    livesText_ = std::make_unique<Text>();
    livesText_->Initialize(kDefaultFont);
    livesText_->SetAnchorPoint({ 1.0f, 0.0f });
    livesText_->SetFontSize(32.0f);
    livesText_->SetOutlineColor({ 0.02f, 0.10f, 0.08f, 1.0f });
    livesText_->SetOutlineWidth(3.0f);
    UpdateLivesText();

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
    if (selfDestructSlowActive_) {
        TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
        selfDestructSlowActive_ = false;
    }
    SceneManager::GetInstance()->RemovePostEffect(PostEffectType::SlimeScreen);
    SceneManager::GetInstance()->SetSlimeScreenProgress(0.0f);
    debugCameraController_.SetTargetCamera(nullptr);
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

    UpdateLivesText();
    if (isDeathTransitionActive_) {
        UpdateDeathTransition(TimeManager::GetInstance()->GetUnscaledDeltaTime());
        return;
    }

    if (input->IsKeyTrigger(DIK_R)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<GamePlayScene>());
        return;
    }

    // TABキーでメニュー開閉
    if (Input::GetInstance()->IsKeyTrigger(DIK_TAB) &&
        !player_->IsShapingSelfDestruct()) {
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

    debugCameraController_.Update();
    const bool isFreeCameraMode = debugCameraController_.GetDebugMode();

    mapChipStage_.Update(); // Playerの前にGimmickを更新して移動量を出しておくのが理想的
    ruinsBackground_.Update();
    bool hardenedThisFrame = false;
    if (player_->ConsumeDeathRequest()) {
        LoseLife();
        if (isDeathTransitionActive_) return;
        hardenedThisFrame = true;
    }

    //player_->Update(mapChipStage_.GetGimmicks());
    if (!hardenedThisFrame &&
        (!isFreeCameraMode || player_->IsShapingSelfDestruct())) {
        player_->Update(mapChipStage_.GetGimmicks());
    }

    if (player_->IsShapingSelfDestruct() && !selfDestructSlowActive_) {
        TimeManager* timeManager = TimeManager::GetInstance();
        timeScaleBeforeSelfDestruct_ = timeManager->GetTimeScale();
        timeManager->SetTimeScale(0.08f);
        selfDestructSlowActive_ = true;
    }

    AABB hardenedBody;
    if (player_->ConsumeHardenedBody(hardenedBody)) {
        LoseLife();
        if (isDeathTransitionActive_) return;
        hardenedThisFrame = true;
    }
    if (!hardenedThisFrame && (player_->IsCrushed() || player_->GetPosition().y < -10.0f)) {
        LoseLife();
        if (isDeathTransitionActive_) return;
        hardenedThisFrame = true;
    }

    if (gpuSphFluid_) {
        const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
        const std::vector<BaseMapChipGimmick*> gimmicks = mapChipStage_.GetGimmicks();

        SceneManager* sceneManager = SceneManager::GetInstance();
        sceneManager->ClearExtraScreenSpaceFluids();
        for (BaseMapChipGimmick* gimmick : gimmicks) {
            if (gimmick && gimmick->IsHardenedSlime()) {
                auto corpse = static_cast<HardenedFluidSlimeCorpse*>(gimmick);
                if (corpse->GetFluid()) {
                    sceneManager->AddExtraScreenSpaceFluid(corpse->GetFluid());
                }
            }
        }

        gpuSphFluid_->SetObstacles(
            BuildFluidObstacles(mapChipStage_, gimmicks, deltaTime));
        gpuSphFluid_->SetFloorHeight(player_->GetFluidFloorHeight());
        gpuSphFluid_->SetGrounded(player_->IsGrounded());
        const Vector3 playerScale = player_->GetVisualScale();
        const Vector3 targetRadii = {
            playerScale.x * (2.4f * kNeoWorldScale),
            playerScale.y * (1.7f * kNeoWorldScale),
            playerScale.z * (2.4f * kNeoWorldScale) };
        gpuSphFluid_->SetBlobRadii(targetRadii);
        float minX = -1000.0f, maxX = 1000.0f, maxY = 1000.0f;
        player_->GetWallBoundaries(minX, maxX, maxY, gimmicks);
        Vector3 corePos = MakeFluidCorePosition(*player_);
        const float zEnvelope = targetRadii.z * 1.2f;
        float minZ = corePos.z - zEnvelope;
        float maxZ = corePos.z + zEnvelope;
        gpuSphFluid_->SetWallBoundaries(minX, maxX, minZ, maxZ, -1000.0f, maxY);
        gpuSphFluid_->SetLiquidated(false);
        constexpr float kEyeMaximumOffset = 0.075f;
        constexpr float kEyeFollowSpeed = 0.90f;
        const float desiredEyeOffset = std::clamp(
            player_->GetVelocity().x / 5.0f,
            -1.0f,
            1.0f) * kEyeMaximumOffset;
        const float eyeDelta = std::clamp(
            desiredEyeOffset - eyeOffsetX_,
            -kEyeFollowSpeed * deltaTime,
            kEyeFollowSpeed * deltaTime);
        eyeOffsetX_ += eyeDelta;
        const Vector2 shapeEyeOffset = player_->GetEyeOffset();
        gpuSphFluid_->SetEyeOffsetX(eyeOffsetX_ + shapeEyeOffset.x);
        gpuSphFluid_->SetEyeOffsetY(shapeEyeOffset.y);
        gpuSphFluid_->SetControlState(
            MakeFluidCorePosition(*player_),
            MakeFluidTargetVelocity(player_->GetVelocity()),
            kSlimeRenderForward);
        // 変形ドラッグ中の左クリックを、液体放出として二重に扱わない。
        const bool leftMousePressed =
            !isFreeCameraMode &&
            !player_->IsShapingSelfDestruct() &&
            !hardenedThisFrame &&
            IsLeftMouseButtonDown(input);
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
    if (!isFreeCameraMode) {
        UpdateFollowCamera();
    }
    camera_->Update();
    skyBox_->Update(camera_.get());
    instructionText_->Update();
    UpdateCollisionText();
    collisionText_->Update();
}

void GamePlayScene::Draw2D()
{
    if (isDeathTransitionActive_) {
        TextRenderer::GetInstance()->PreDraw();
        livesText_->Draw();
        return;
    }
    if (showForces_) {
        fluidForceRenderer_->Draw(*gpuSphFluid_, *camera_);
    }

    TextRenderer::GetInstance()->PreDraw();
    instructionText_->Draw();
    collisionText_->Draw();
    livesText_->Draw();
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
    ruinsBackground_.Draw(true);
    mapChipStage_.Draw();
}

void GamePlayScene::DrawParticle()
{
    if (showForces_ && gpuSphFluid_) {
        fluidForceRenderer_->Draw(*gpuSphFluid_, *camera_);
    }
}

void GamePlayScene::DrawImGui()
{
}

void GamePlayScene::RespawnPlayerLeavingCorpse()
{
    if (selfDestructSlowActive_) {
        TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
        selfDestructSlowActive_ = false;
    }

    const GpuSphFluid::Settings currentSettings = gpuSphFluid_->GetSettings();
    const std::vector<GpuSphFluid::Particle> particles =
        gpuSphFluid_->GetParticlesCPU();

    auto corpse = std::make_unique<HardenedFluidSlimeCorpse>();
    if (corpse->InitializeFromParticles(
            DirectXCommon::GetInstance(),
            SrvManager::GetInstance(),
            particles,
            currentSettings)) {
        mapChipStage_.AddGimmick(std::move(corpse));
    }

    player_->Initialize(&mapChipStage_.GetField(), playerStartPosition_);
    eyeOffsetX_ = 0.0f;
    wasLeftMousePressed_ = false;

    GpuSphFluid::Settings respawnSettings = currentSettings;
    respawnSettings.corePosition = MakeFluidCorePosition(*player_);
    respawnSettings.floorHeight = player_->GetFluidFloorHeight();
    respawnSettings.targetVelocity = { 0.0f, 0.0f, 0.0f };
    const Vector3 playerScale = player_->GetVisualScale();
    respawnSettings.blobRadii = {
        playerScale.x * (2.4f * kNeoWorldScale),
        playerScale.y * (1.7f * kNeoWorldScale),
        playerScale.z * (2.4f * kNeoWorldScale) };
    gpuSphFluid_->SetLiquidated(false);
    gpuSphFluid_->SetDeathEyes(false);
    gpuSphFluid_->Reset(respawnSettings);
}

void GamePlayScene::UpdateLivesText()
{
    if (!livesText_) return;
    const float width = static_cast<float>((std::max)(WinApp::GetInstance()->GetClientWidth(), 1));
    livesText_->SetPosition({ width - 32.0f, 108.0f });
    livesText_->SetText("残機 × " + std::to_string(remainingLives_));
    livesText_->SetColor(remainingLives_ <= 2
        ? Vector4{ 1.0f, 0.40f, 0.30f, 1.0f }
        : Vector4{ 0.30f, 1.0f, 0.72f, 1.0f });
    livesText_->Update();
}

void GamePlayScene::LoseLife()
{
    if (isDeathTransitionActive_ || remainingLives_ <= 0) return;
    --remainingLives_;
    UpdateLivesText();
    if (remainingLives_ == 0) {
        StartDeathTransition();
    } else {
        RespawnPlayerLeavingCorpse();
    }
}

void GamePlayScene::StartDeathTransition()
{
    if (isDeathTransitionActive_) {
        return;
    }
    isDeathTransitionActive_ = true;
    deathTransitionTime_ = 0.0f;
    isMenuOpen_ = false;
    remainingLives_ = 0;
    UpdateLivesText();
    if (selfDestructSlowActive_) {
        TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
        selfDestructSlowActive_ = false;
    }

    // Keep the player camera still while the actual body breaks into liquid.
    debugCameraController_.SetDebugMode(false);
    UpdateFollowCamera();
    camera_->Update();
    skyBox_->Update(camera_.get());
    auto particles = gpuSphFluid_->GetParticlesCPU();
    auto settings = gpuSphFluid_->GetSettings();
    const Vector3 center = settings.corePosition;
    settings.gravity = { 0.0f, -2.0f, 0.0f };
    settings.liquidGravityScale = 1.0f;
    settings.liquidShapeAttraction = 0.0f;
    settings.liquidVelocityAttraction = 0.0f;
    settings.liquidViscosity = 0.0f;
    settings.liquidSurfaceTension = 0.0f;
    settings.liquidDamping = 0.0f;
    settings.floorHeight = center.y - 30.0f;
    settings.boundsMin = { center.x - 40.0f, center.y - 40.0f, -40.0f };
    settings.boundsMax = { center.x + 40.0f, center.y + 40.0f, 40.0f };
    gpuSphFluid_->SetLiquidated(true);
    gpuSphFluid_->SetDeathEyes(true);
    gpuSphFluid_->Reset(settings);
    gpuSphFluid_->SetWallBoundaries(center.x - 40.0f, center.x + 40.0f,
        -40.0f, 40.0f, center.y - 40.0f, center.y + 40.0f);
    gpuSphFluid_->SetGrounded(false);
    gpuSphFluid_->SetEmitter(false, center, { 0.0f, 0.0f, 0.0f });
    for (size_t i = 0; i < particles.size(); ++i) {
        auto& particle = particles[i];
        const float angle = static_cast<float>(i) * 2.39996323f;
        const float spread = 1.5f + static_cast<float>(i % 17) * 0.24f;
        // Front-facing droplets reach the lens around 0.6 seconds after rupture.
        particle.velocity = { std::cos(angle) * spread,
            std::sin(angle) * spread + 1.0f,
            i % 3 == 0 ? -18.0f - static_cast<float>(i % 7) : 2.0f * std::sin(angle) };
        particle.padding = 5.0f;
    }
    gpuSphFluid_->SetParticlesCPU(particles);
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
    constexpr float kFlightDuration = 0.55f;
    deathTransitionTime_ += deltaTime;
    gpuSphFluid_->Update(deltaTime);
    const float progress = std::clamp(
        (deathTransitionTime_ - kFlightDuration) / kCoverDuration, 0.0f, 1.0f);
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
