#include "GamePlayScene.h"

#include "App/Game/Gimmick/HardenedFluidSlimeCorpse.h"
#include "ArchiveScene.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Input/Input.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include "Engine/Logger/Logger.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Time/TimeManager.h"
#include "GameOverScene.h"
#include "ClearScene.h"
#include "SceneManager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <vector>

namespace {
constexpr const char *kSkyBoxTexture = "resources/Textures/skybox.dds";
constexpr const char *kMapChipTexture = "resources/Textures/checkerboard.png";
constexpr const char *kWhiteTexture = "resources/Textures/white.png";
constexpr const char *kLifeSlimeTexture = "resources/Textures/Slime.png";
constexpr const char *kDeadSlimeTexture = "resources/Textures/deadSlime.png";
constexpr const char *kLifeSlimeMaterial =
    "resources/Shaders/Sprite/LifeSlime";
constexpr const char *kFantasyMenuMaterial =
    "resources/Shaders/Sprite/FantasyMenu";
constexpr float kCameraDistance = 12.0f;
constexpr const char *kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr float kFluidRenderZ = 0.0f;
constexpr float kNeoWorldScale = 0.36f;
constexpr Vector3 kSlimeRenderForward = {0.0f, 0.0f, 1.0f};
constexpr float kMenuButtonX = 440.0f;
constexpr float kMenuButtonWidth = 400.0f;
constexpr float kMenuButtonHeight = 58.0f;
constexpr float kMenuResumeY = 300.0f;
constexpr float kMenuRestartY = 380.0f;
constexpr float kMenuStageSelectY = 460.0f;
constexpr float kStageSelectFadeDuration = 0.45f;
constexpr float kFantasyMenuBlendDuration = 0.20f;
struct Stage1TutorialStep {
  float triggerX;
  const char *message;
};

constexpr std::array<Stage1TutorialStep, 10> kStage1TutorialSteps = {{
    {0.0f, "A / D で移動　SPACE でジャンプ"},
    {17.0f, "トゲは飛び越えられない。ここで死ぬと硬化スライムが足場になる"},
    {30.0f, "感圧板を踏むと扉が開く"},
    {34.0f, "感圧板の上で硬化すると、扉を開けたままにできる"},
    {52.0f, "橋の揺れを見て、タイミングよく跳ぼう"},
    {58.0f, "届かない場所は、硬化スライムで橋をつなげよう"},
    {74.0f, "硬化スライムはレーザーを防ぐ"},
    {84.0f, "感圧板の上で硬化して、ガスを出そう"},
    {87.0f, "炎がガスに引火すると、壁を壊せる"},
    {101.0f, "ゴールはもうすぐ。足場とジャンプを使い分けよう"},
}};
constexpr const char *kStage1ShapeTutorialMessage =
    "左クリック長押しで、硬化する前のスライムの形を少し変えられる";

bool IsPointInMenuButton(const Vector2 &point, float y) {
  return point.x >= kMenuButtonX &&
         point.x <= kMenuButtonX + kMenuButtonWidth && point.y >= y &&
         point.y <= y + kMenuButtonHeight;
}

bool IntersectsAABB(const AABB &left, const AABB &right) {
  const Vector3 leftHalfSize = left.size * 0.5f;
  const Vector3 rightHalfSize = right.size * 0.5f;
  return std::abs(left.center.x - right.center.x) <=
             leftHalfSize.x + rightHalfSize.x &&
         std::abs(left.center.y - right.center.y) <=
             leftHalfSize.y + rightHalfSize.y &&
         std::abs(left.center.z - right.center.z) <=
             leftHalfSize.z + rightHalfSize.z;
}

bool IsPlayerOnGasPressurePlate(const MapChipStage &stage,
                                const MapChipPlayer &player) {
  for (BaseMapChipGimmick *gimmick : stage.GetGimmicks()) {
    if (!gimmick || gimmick->GetLinkName() != "Event_2") {
      continue;
    }

    const AABB switchAABB = gimmick->GetAABB();
    // Event_2 には篝火も含まれるため、x=85 の感圧板だけを対象にする。
    if (switchAABB.center.x < 86.0f &&
        IntersectsAABB(player.GetAABB(), switchAABB)) {
      return true;
    }
  }
  return false;
}

Vector3 MakeFluidCorePosition(const MapChipPlayer &player) {
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
  explicit HardenedSlimeBody(const AABB &bounds) : bounds_(bounds) {}

  bool Initialize(const Vector3 &, const std::string &,const BaseGimmickParam *) override {
    // 外部モデル(slime_mesh.obj)は一切使わず、変形した自爆形状(bounds_)に100%一致するCubeモデルで生成する。
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    Model *cubeModel =ModelManager::GetInstance()->CreateCube("resources/Textures/white.png");
    object_->SetModel(cubeModel);
    object_->SetTranslate({bounds_.center.x, bounds_.center.y, kFluidRenderZ});
    object_->SetScale(bounds_.size);
    object_->SetColor({0.20f, 0.95f, 0.65f, 1.0f});
    object_->SetEnableLighting(true);
    object_->Update();

    collisionBoxes_.clear();
    collisionBoxes_.push_back(bounds_);
    return true;
  }

  void Update() override {
    if (object_) {
      object_->Update();
    }
  }

  void Draw() override {
    if (object_) {
      object_->Draw();
    }
  }

  AABB GetAABB() const override { return bounds_; }
  std::vector<AABB> GetCollisionBoxes() const override {
    return collisionBoxes_;
  }
  bool IsHardenedSlime() const override { return true; }

private:
  AABB bounds_{};
  std::vector<AABB> collisionBoxes_;
  std::unique_ptr<Object3d> object_;
};

Vector3 MakeFluidTargetVelocity(const Vector3 &playerVelocity) {
  return {playerVelocity.x, playerVelocity.y, 0.0f};
}

GpuSphFluid::CollisionObstacle MakeFluidObstacle(const Vector3 &center,
                                                 const Vector3 &size,
                                                 const Vector3 &velocity) {
  GpuSphFluid::CollisionObstacle obstacle{};
  obstacle.center = {center.x, center.y, kFluidRenderZ};
  obstacle.halfSize = {size.x * 0.5f, size.y * 0.5f, 0.65f};
  obstacle.velocity = {velocity.x, velocity.y, 0.0f};
  return obstacle;
}

std::vector<GpuSphFluid::CollisionObstacle>
BuildFluidObstacles(const MapChipStage &stage,
                    const std::vector<BaseMapChipGimmick *> &gimmicks,
                    float deltaTime) {
  std::vector<GpuSphFluid::CollisionObstacle> obstacles;
  const MapChipField &field = stage.GetField();
  obstacles.reserve(static_cast<size_t>(field.GetBlockWidth()) *
                        static_cast<size_t>(field.GetBlockHeight()) +
                    gimmicks.size());

  for (uint32_t y = 0; y < field.GetBlockHeight(); ++y) {
    for (uint32_t x = 0; x < field.GetBlockWidth(); ++x) {
      const MapChipType type = field.GetMapChipTypeByIndex(x, y);
      if (type != MapChipType::Block && type != MapChipType::Foundation) {
        continue;
      }

      obstacles.push_back(
          MakeFluidObstacle(field.GetMapChipPositionByIndex(x, y),
                            {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}));
    }
  }

  const float safeDeltaTime = (std::max)(deltaTime, 0.0001f);
  for (BaseMapChipGimmick *gimmick : gimmicks) {
    if (!gimmick->IsSolid())
      continue;
    const Vector3 delta = gimmick->GetDeltaPosition();
    for (const AABB &box : gimmick->GetCollisionBoxes()) {
      obstacles.push_back(
          MakeFluidObstacle(box.center, box.size,
                            {delta.x / safeDeltaTime, delta.y / safeDeltaTime,
                             delta.z / safeDeltaTime}));
    }
  }

  return obstacles;
}
} // namespace

GamePlayScene::GamePlayScene(std::string levelPath)
    : levelPath_(std::move(levelPath)) {}

void GamePlayScene::Initialize() {
  SceneManager::GetInstance()->SetPostEffectType(
      PostEffectType::ArchiveAtmosphere);
  SceneManager::GetInstance()->SetArchiveApproach(0.0f);
  SceneManager::GetInstance()->SetSlimeScreenProgress(0.0f);
  SceneManager::GetInstance()->SetFantasyMenuStrength(0.0f);
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
  LevelData levelData = loader.Load(levelPath_);
  maximumLives_ = levelPath_.find("stage2.json") != std::string::npos
      ? 20
      : (levelPath_.find("stage1.json") != std::string::npos
          ? kStage1Lives
          : kInitialLives);
  remainingLives_ = maximumLives_;

  mapChipStage_.Initialize(levelData);
  mapChipStage_.ApplyMaterialProperties();

  RuinsBackground::Settings backgroundSettings;
  backgroundSettings.mapLength =
      static_cast<float>(mapChipStage_.GetField().GetBlockWidth());
  ruinsBackground_.Initialize(backgroundSettings);

  Vector3 playerStartPos = {0.0f, 0.0f, 0.0f};
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
  fluidSettings.particleMass = kNeoWorldScale * kNeoWorldScale * kNeoWorldScale;
  fluidSettings.restDensity = 3.0f;
  fluidSettings.blobRadii = {1.2f * kNeoWorldScale, 0.85f * kNeoWorldScale,
                             1.2f * kNeoWorldScale};
  fluidSettings.stiffness = 50.0f;
  fluidSettings.shapeAttraction = 80.0f;
  fluidSettings.velocityAttraction = 0.0f;
  fluidSettings.viscosity = 15.0f;
  fluidSettings.surfaceTension = 0.0f;
  fluidSettings.gravity = {0.0f, -20.0f * kNeoWorldScale, 0.0f};
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

  fluidSettings.boundsMin = {-4.0f, -20.0f, kFluidRenderZ - 2.0f};
  fluidSettings.boundsMax = {
      static_cast<float>(mapChipStage_.GetField().GetBlockWidth()) + 4.0f,
      static_cast<float>(mapChipStage_.GetField().GetBlockHeight()) + 8.0f,
      kFluidRenderZ + 2.0f};
  gpuSphFluid_->Initialize(DirectXCommon::GetInstance(),
                           SrvManager::GetInstance(), fluidSettings);
  gpuSphFluid_->SetLiquidated(
      false); // スライムプレイヤーとしてのまとまった形状を維持
  walkingDustEffectHandle_ = kInvalidEffectHandle;
  EffectManager::GetInstance()->SetCamera(camera_.get());

  fluidForceRenderer_ = std::make_unique<FluidForceRenderer>();
  fluidForceRenderer_->Initialize(DirectXCommon::GetInstance());

  gpuSphFluidRenderer_ = std::make_unique<GpuSphFluidRenderer>();
  gpuSphFluidRenderer_->Initialize(DirectXCommon::GetInstance());

  SceneManager::GetInstance()->SetScreenSpaceFluid(gpuSphFluid_.get());
  Logger::Log(std::format(
      "[GamePlayScene] GpuSphFluid initialized. particles={} "
      "core=({:.2f},{:.2f},{:.2f}) floor={:.2f} "
      "boundsMin=({:.2f},{:.2f},{:.2f}) boundsMax=({:.2f},{:.2f},{:.2f})\n",
      fluidSettings.particleCount, fluidSettings.corePosition.x,
      fluidSettings.corePosition.y, fluidSettings.corePosition.z,
      fluidSettings.floorHeight, fluidSettings.boundsMin.x,
      fluidSettings.boundsMin.y, fluidSettings.boundsMin.z,
      fluidSettings.boundsMax.x, fluidSettings.boundsMax.y,
      fluidSettings.boundsMax.z));

  UpdateFollowCamera();
  camera_->Update();

  TextureManager::GetInstance()->LoadTexture(kSkyBoxTexture);
  skyBox_ = std::make_unique<SkyBox>();
  skyBox_->Initialize(DirectXCommon::GetInstance());
  skyBox_->SetTexture(kSkyBoxTexture);
  skyBox_->Update(camera_.get());

  TextureManager::GetInstance()->LoadTexture(kWhiteTexture);
  tutorialPanelSprite_ = std::make_unique<Sprite>();
  tutorialPanelSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
  tutorialPanelSprite_->SetPosition({60.0f, 16.0f});
  tutorialPanelSprite_->SetSize({1160.0f, 92.0f});
  tutorialPanelSprite_->SetColor({0.02f, 0.05f, 0.12f, 0.0f});
  tutorialPanelSprite_->Update();

  tutorialText_ = std::make_unique<Text>();
  tutorialText_->Initialize(kDefaultFont);
  tutorialText_->SetAnchorPoint({0.5f, 0.5f});
  tutorialText_->SetPosition({640.0f, 61.0f});
  tutorialText_->SetFontSize(42.0f);
  tutorialText_->SetColor({1.0f, 1.0f, 1.0f, 0.0f});
  tutorialText_->SetOutlineWidth(0.0f);
  tutorialText_->SetShadowColor({0.0f, 0.0f, 0.0f, 0.0f});

  // チュートリアル表示中に字形生成や頂点バッファの拡張が起きないよう、
  // stage1の全メッセージをロード中に一度だけ非表示で更新して先読みする。
  if (levelPath_.find("stage1.json") != std::string::npos) {
    std::string tutorialPreloadText;
    for (const Stage1TutorialStep& step : kStage1TutorialSteps) {
      tutorialPreloadText += step.message;
    }
    tutorialPreloadText += kStage1ShapeTutorialMessage;
    tutorialText_->SetText(tutorialPreloadText);
    tutorialText_->Update();
    tutorialText_->SetText("");
    tutorialText_->Update();
  }

  TextureManager::GetInstance()->LoadTexture(kLifeSlimeTexture);
  TextureManager::GetInstance()->LoadTexture(kDeadSlimeTexture);
  livesNumberText_ = std::make_unique<Text>();
  // 数字は96pxの高解像度字形から縮小して描画し、輪郭のぼやけを防ぐ。
  livesNumberText_->Initialize(kDefaultFont, true);
  livesNumberText_->SetAnchorPoint({1.0f, 0.5f});
  livesNumberText_->SetFontSize(48.0f);
  livesNumberText_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
  livesNumberText_->SetOutlineColor({0.0f, 0.015f, 0.04f, 1.0f});
  livesNumberText_->SetOutlineWidth(2.0f);
  livesNumberText_->SetShadowColor({0.0f, 0.0f, 0.0f, 0.0f});
  UpdateLivesText();

  // メニューUIの初期化
  TextureManager::GetInstance()->LoadTexture(kWhiteTexture);

  // 全画面半透明暗幕
  menuBackgroundSprite_ = std::make_unique<Sprite>();
  menuBackgroundSprite_->Initialize(SpriteManager::GetInstance(),
                                    kWhiteTexture);
  menuBackgroundSprite_->SetSize({1280.0f, 720.0f});
  menuBackgroundSprite_->SetPosition({0.0f, 0.0f});
  menuBackgroundSprite_->SetColor({0.0f, 0.0f, 0.0f, 0.42f});

  // 中央パネル
  menuPanelSprite_ = std::make_unique<Sprite>();
  menuPanelSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
  menuPanelSprite_->SetMaterial(kFantasyMenuMaterial);
  menuPanelSprite_->SetSize({640.0f, 380.0f});
  menuPanelSprite_->SetPosition({320.0f, 170.0f});
  menuPanelSprite_->SetColor({0.10f, 0.20f, 0.15f, 0.97f});

  const auto createMenuButton = [](float y) {
    auto button = std::make_unique<Sprite>();
    button->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    button->SetMaterial(kFantasyMenuMaterial);
    button->SetSize({kMenuButtonWidth, kMenuButtonHeight});
    button->SetPosition({kMenuButtonX, y});
    button->SetColor({0.15f, 0.25f, 0.22f, 1.0f});
    return button;
  };
  menuResumeButtonSprite_ = createMenuButton(kMenuResumeY);
  menuRestartButtonSprite_ = createMenuButton(kMenuRestartY);
  menuStageSelectButtonSprite_ = createMenuButton(kMenuStageSelectY);

  // メニュータイトル
  menuTitleText_ = std::make_unique<Text>();
  menuTitleText_->Initialize(kDefaultFont);
  menuTitleText_->SetText("PAUSE MENU");
  menuTitleText_->SetPosition({640.0f, 230.0f});
  menuTitleText_->SetAnchorPoint({0.5f, 0.5f});
  menuTitleText_->SetFontSize(44.0f);
  menuTitleText_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});

  const auto createMenuText = [](const char *label, float y) {
    auto text = std::make_unique<Text>();
    text->Initialize(kDefaultFont);
    text->SetText(label);
    text->SetPosition({640.0f, y + kMenuButtonHeight * 0.5f});
    text->SetAnchorPoint({0.5f, 0.5f});
    text->SetFontSize(25.0f);
    text->SetColor({0.90f, 0.96f, 0.93f, 1.0f});
    return text;
  };
  menuResumeText_ = createMenuText("RESUME GAME  [TAB]", kMenuResumeY);
  menuRestartText_ = createMenuText("RETRY STAGE  [R]", kMenuRestartY);
  menuStageSelectText_ =
      createMenuText("STAGE SELECT  [BACKSPACE]", kMenuStageSelectY);

  menuTransitionFadeSprite_ = std::make_unique<Sprite>();
  menuTransitionFadeSprite_->Initialize(SpriteManager::GetInstance(),
                                        kWhiteTexture);
  menuTransitionFadeSprite_->SetSize({1280.0f, 720.0f});
  menuTransitionFadeSprite_->SetPosition({0.0f, 0.0f});
  menuTransitionFadeSprite_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
  menuTransitionFadeSprite_->Update();

  pageReveal_.InitializeIfRequested();
  
  savePointHistory_.clear();
  PushSavePoint();
}

void GamePlayScene::Finalize() {
  if (selfDestructSlowActive_) {
    TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
    selfDestructSlowActive_ = false;
  }
  SceneManager::GetInstance()->RemovePostEffect(PostEffectType::SlimeScreen);
  SceneManager::GetInstance()->RemovePostEffect(PostEffectType::ClearSlimeRise);
  SceneManager::GetInstance()->RemovePostEffect(PostEffectType::FantasyMenu);
  SceneManager::GetInstance()->SetSlimeScreenProgress(0.0f);
  SceneManager::GetInstance()->SetFantasyMenuStrength(0.0f);
  debugCameraController_.SetTargetCamera(nullptr);
  SceneManager::GetInstance()->SetScreenSpaceFluid(nullptr);
  SceneManager::GetInstance()->ClearExtraScreenSpaceFluids();
  EffectManager::GetInstance()->StopAllEffects();
  EffectManager::GetInstance()->SetCamera(nullptr);
  walkingDustEffectHandle_ = kInvalidEffectHandle;
  if (gpuSphFluid_) {
    gpuSphFluid_->Finalize();
  }
  Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
  SkinningObject3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void GamePlayScene::Update() {
  Input *input = Input::GetInstance();
  const float unscaledDeltaTime =
      TimeManager::GetInstance()->GetUnscaledDeltaTime();
  pageReveal_.Update(TimeManager::GetInstance()->GetDeltaTime());
  UpdateFantasyMenuEffect(unscaledDeltaTime);

  UpdateLivesText();
  if (isStageSelectTransitionActive_) {
    UpdateStageSelectTransition(
        TimeManager::GetInstance()->GetUnscaledDeltaTime());
    return;
  }
  if (isDeathTransitionActive_) {
    UpdateDeathTransition(TimeManager::GetInstance()->GetUnscaledDeltaTime());
    return;
  }

  if (isClearCelebrationActive_) {
    UpdateClearCelebration(unscaledDeltaTime);
    if (!isClearCelebrationActive_) return;
  }

  if (!isClearCelebrationActive_ && !isMenuOpen_ && input->IsKeyTrigger(DIK_R)) {
    ResetToLastRespawnPoint();
    return;
  }

  // TABキーでメニュー開閉
  if (!isClearCelebrationActive_ && Input::GetInstance()->IsKeyTrigger(DIK_TAB) &&
      !player_->IsShapingSelfDestruct()) {
    isMenuOpen_ = !isMenuOpen_;
    if (isMenuOpen_) {
      SceneManager::GetInstance()->AddPostEffect(
          PostEffectType::FantasyMenu, PostEffectStage::AfterParticle);
    }
  }

  // メニューが開いているときはゲーム内処理を行わずに早期リターン
  if (isMenuOpen_) {
    const Vector2 mousePosition = input->GetMousePosition();
    const bool resumeHovered = IsPointInMenuButton(mousePosition, kMenuResumeY);
    const bool restartHovered = IsPointInMenuButton(mousePosition, kMenuRestartY);
    const bool stageSelectHovered =
        IsPointInMenuButton(mousePosition, kMenuStageSelectY);
    const bool clicked = input->IsMouseTrigger(0);

    menuResumeButtonSprite_->SetColor(resumeHovered
                                          ? Vector4{0.25f, 0.48f, 0.34f, 1.0f}
                                          : Vector4{0.15f, 0.25f, 0.22f, 1.0f});
    menuRestartButtonSprite_->SetColor(
        restartHovered ? Vector4{0.30f, 0.38f, 0.54f, 1.0f}
                       : Vector4{0.17f, 0.21f, 0.30f, 1.0f});
    menuStageSelectButtonSprite_->SetColor(
        stageSelectHovered ? Vector4{0.30f, 0.38f, 0.54f, 1.0f}
                           : Vector4{0.17f, 0.21f, 0.30f, 1.0f});

    if (clicked && resumeHovered) {
      isMenuOpen_ = false;
      return;
    }
    if (input->IsKeyTrigger(DIK_R) || (clicked && restartHovered)) {
      SceneManager::GetInstance()->SetNextScene(
          std::make_unique<GamePlayScene>(levelPath_));
      return;
    }
    if (input->IsKeyTrigger(DIK_BACKSPACE) || (clicked && stageSelectHovered)) {
      StartStageSelectTransition();
      return;
    }

    menuBackgroundSprite_->Update();
    menuPanelSprite_->Update();
    menuResumeButtonSprite_->Update();
    menuRestartButtonSprite_->Update();
    menuStageSelectButtonSprite_->Update();
    menuTitleText_->Update();
    menuResumeText_->Update();
    menuRestartText_->Update();
    menuStageSelectText_->Update();
    return;
  }

  if (input->IsKeyTrigger(DIK_Y)) {
    showForces_ = !showForces_;
  }

  debugCameraController_.Update();
  const bool isFreeCameraMode = debugCameraController_.GetDebugMode();

  // 形状作成中のスローは、レーザーなどのトラップ判定より先に無敵を
  // 設定する。これにより、形状を作っている最中にトラップ死から
  // リスポーン処理へ入ることを防ぐ。
  const bool isSlowMotion = TimeManager::GetInstance()->GetTimeScale() < 0.999f;
  player_->SetInvincible(isSlowMotion);

  mapChipStage_
      .Update(); // Playerの前にGimmickを更新して移動量を出しておくのが理想的
  ruinsBackground_.Update();
  bool hardenedThisFrame = false;

  // player_->Update(mapChipStage_.GetGimmicks());
    if (!isClearCelebrationActive_ && !hardenedThisFrame &&
      (!isFreeCameraMode || player_->IsShapingSelfDestruct()) &&
      !isLifeRelayActive_) {
        player_->Update(mapChipStage_.GetGimmicks());

        if (player_->ConsumeGoalReached()) {
          StartClearCelebration();
        }

    // 中間地点を通過したら、以降の命のリレー先をここへ更新する。
    for (BaseMapChipGimmick *gimmick : mapChipStage_.GetGimmicks()) {
      if (gimmick && gimmick->IsCheckpoint() &&
          gimmick->TryActivateCheckpoint(player_->GetAABB())) {
        playerStartPosition_ = gimmick->GetAABB().center;
        PushSavePoint();
        EffectManager::GetInstance()->PlayEffect("BlueFireworkSparks",
                                                 playerStartPosition_);
      }
    }
  }

  if (isLifeRelayActive_) {
    lifeRelayTimer_ += TimeManager::GetInstance()->GetDeltaTime();
    float t = lifeRelayTimer_ / lifeRelayDuration_;
    if (t > 1.0f) {
      t = 1.0f;
    }

    float smoothT = t * t * (3.0f - 2.0f * t);
    lifeRelayOrbCurrentPosition_ =
        Lerp(lifeRelayOrbStartPosition_, playerStartPosition_, smoothT);

    if (EffectManager::GetInstance()->IsEffectAlive(
            lifeRelayOrbEffectHandle_)) {
      EffectManager::GetInstance()->SetEffectPosition(
          lifeRelayOrbEffectHandle_, lifeRelayOrbCurrentPosition_);
    } else {
      lifeRelayOrbEffectHandle_ = EffectManager::GetInstance()->PlayLoopEffect(
          "FlameCore", lifeRelayOrbCurrentPosition_);
    }

    if (t >= 1.0f) {
      FinishLifeRelay();
      PushSavePoint();
    }
  }

  // 落下死はスローに入れず、通常速度のまま残機を消費してリスポーンする。
  if (!isClearCelebrationActive_ && !isLifeRelayActive_ &&
      player_->ConsumeJustDied()) {
    LoseLife(false);
    if (isDeathTransitionActive_)
      return;
    hardenedThisFrame = true;
  }

    if (!isClearCelebrationActive_ && player_->IsShapingSelfDestruct() && !selfDestructSlowActive_) {
    TimeManager *timeManager = TimeManager::GetInstance();
    timeScaleBeforeSelfDestruct_ = timeManager->GetTimeScale();
    timeManager->SetTimeScale(0.08f);
    selfDestructSlowActive_ = true;
  }

  AABB hardenedBody;
    if (!isClearCelebrationActive_ && player_->ConsumeHardenedBody(hardenedBody)) {
    // 右クリックによる確定自爆はスロー中でも有効にする。
    // スロー中に無効化するのはトラップ・落下などの意図しない死亡だけ。
    LoseLife();
    if (isDeathTransitionActive_)
      return;
    hardenedThisFrame = true;
  }
  if (gpuSphFluid_) {
    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    const std::vector<BaseMapChipGimmick *> gimmicks =
        mapChipStage_.GetGimmicks();

    SceneManager *sceneManager = SceneManager::GetInstance();
    sceneManager->ClearExtraScreenSpaceFluids();
    for (BaseMapChipGimmick *gimmick : gimmicks) {
      if (gimmick && gimmick->IsHardenedSlime()) {
        auto corpse = static_cast<HardenedFluidSlimeCorpse *>(gimmick);
        if (corpse->GetFluid()) {
          sceneManager->AddExtraScreenSpaceFluid(corpse->GetFluid());
        }
      }
    }

    if (!isLifeRelayActive_) {
      gpuSphFluid_->SetObstacles(
          BuildFluidObstacles(mapChipStage_, gimmicks, deltaTime));
      gpuSphFluid_->SetFloorHeight(player_->GetFluidFloorHeight());
      gpuSphFluid_->SetGrounded(player_->IsGrounded());
      const Vector3 playerScale = player_->GetVisualScale();
      Vector3 targetRadii = {playerScale.x * (2.4f * kNeoWorldScale),
                             playerScale.y * (1.7f * kNeoWorldScale),
                             playerScale.z * (2.4f * kNeoWorldScale)};
      float minX = -1000.0f, maxX = 1000.0f, maxY = 1000.0f;
      player_->GetWallBoundaries(minX, maxX, maxY, gimmicks);
      Vector3 corePos = MakeFluidCorePosition(*player_);
      Vector3 targetVelocity = MakeFluidTargetVelocity(player_->GetVelocity());
      if (isClearCelebrationActive_) {
        // 喜びのぽよん：左右にステップしながら跳ね上がる。
        const float bounce = std::abs(std::sin(clearCelebrationTimer_ * 11.0f));
        const float sideStep = std::sin(clearCelebrationTimer_ * 7.0f);
        targetRadii.x *= 1.34f - bounce * 0.18f;
        targetRadii.y *= 1.48f + bounce * 0.52f;
        targetRadii.z *= 1.26f;
        corePos.x += sideStep * 1.10f;
        corePos.y += bounce * 0.72f;
        targetVelocity = {
          std::cos(clearCelebrationTimer_ * 7.0f) * 7.7f,
          std::cos(clearCelebrationTimer_ * 11.0f) * 2.4f,
          0.0f
        };
        gpuSphFluid_->SetGrounded(false);
      }
      gpuSphFluid_->SetBlobRadii(targetRadii);
      const float zEnvelope = targetRadii.z * 1.2f;
      float minZ = corePos.z - zEnvelope;
      float maxZ = corePos.z + zEnvelope;
      gpuSphFluid_->SetWallBoundaries(minX, maxX, minZ, maxZ, -1000.0f, maxY);
      gpuSphFluid_->SetLiquidated(false);
      constexpr float kEyeMaximumOffset = 0.075f;
      constexpr float kEyeFollowSpeed = 0.90f;
      const float desiredEyeOffset =
          std::clamp(player_->GetVelocity().x / 5.0f, -1.0f, 1.0f) *
          kEyeMaximumOffset;
      const float eyeDelta =
          std::clamp(desiredEyeOffset - eyeOffsetX_,
                     -kEyeFollowSpeed * deltaTime, kEyeFollowSpeed * deltaTime);
      eyeOffsetX_ += eyeDelta;
      const Vector2 shapeEyeOffset = player_->GetEyeOffset();
      gpuSphFluid_->SetEyeOffsetX(eyeOffsetX_ + shapeEyeOffset.x);
      gpuSphFluid_->SetEyeOffsetY(shapeEyeOffset.y);
      gpuSphFluid_->SetControlState(
          corePos, targetVelocity, kSlimeRenderForward);
      const float horizontalSpeed = isClearCelebrationActive_
          ? 0.0f : std::abs(player_->GetVelocity().x);
      const bool emitWalkingTrail =
          player_->IsGrounded() && horizontalSpeed > 0.25f;
      const float movementDirection =
          player_->GetVelocity().x >= 0.0f ? 1.0f : -1.0f;
      Vector3 trailPosition = MakeFluidCorePosition(*player_);
      trailPosition.x -= movementDirection * targetRadii.x * 0.92f;
      trailPosition.y = player_->GetFluidFloorHeight() + 0.07f;
      const Vector3 trailVelocity = {
          -movementDirection * (0.70f + horizontalSpeed * 0.12f), 0.10f, 0.0f};
      gpuSphFluid_->SetEmitter(false, corePos, {0.0f, 0.0f, 0.0f});
      gpuSphFluid_->Update(deltaTime);

      // 流体とは無関係な土埃エフェクト。低い位置から後方へ短く舞い上がる。
      EffectManager *effects = EffectManager::GetInstance();
      const bool emitWalkingDust =
          player_->IsGrounded() && horizontalSpeed > 0.45f;
      if (emitWalkingDust) {
        Vector3 dustPosition = trailPosition;
        // 地面の内部に埋まらない高さから、足元で土煙を見せる。
        dustPosition.y = player_->GetFluidFloorHeight() + 0.14f;
        if (!effects->IsEffectAlive(walkingDustEffectHandle_)) {
          walkingDustEffectHandle_ =
              effects->PlayLoopEffect("WalkDust", dustPosition);
        }
        effects->SetEffectPosition(walkingDustEffectHandle_, dustPosition);
        effects->SetEffectVelocity(
            walkingDustEffectHandle_,
            {
                -movementDirection * (0.30f + horizontalSpeed * 0.08f),
                0.16f,
                0.0f,
            });
      } else if (effects->IsEffectAlive(walkingDustEffectHandle_)) {
        effects->StopEffect(walkingDustEffectHandle_);
        walkingDustEffectHandle_ = kInvalidEffectHandle;
      }
    } else {
      EffectManager *effects = EffectManager::GetInstance();
      if (effects->IsEffectAlive(walkingDustEffectHandle_)) {
        effects->StopEffect(walkingDustEffectHandle_);
        walkingDustEffectHandle_ = kInvalidEffectHandle;
      }
    }
  }
  if (!isFreeCameraMode) {
    UpdateFollowCamera();
  }
  camera_->Update();
  EffectManager::GetInstance()->SetCamera(camera_.get());
  EffectManager::GetInstance()->Update();
  skyBox_->Update(camera_.get());
  UpdateStage1Tutorial();
  tutorialPanelSprite_->Update();
  tutorialText_->Update();
}

void GamePlayScene::UpdateStage1Tutorial() {
  if (!tutorialText_ || !tutorialPanelSprite_ || !player_ ||
      levelPath_.find("stage1.json") == std::string::npos) {
    return;
  }

  // 最初に右クリックで形作りを始めた瞬間だけ、変形操作を案内する。
  if (!stage1ShapeTutorialShown_ && player_->IsShapingSelfDestruct()) {
    tutorialText_->SetText(kStage1ShapeTutorialMessage);
    tutorialPanelSprite_->SetColor({0.02f, 0.05f, 0.12f, 0.82f});
    tutorialText_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    stage1ShapeTutorialShown_ = true;
    return;
  }

  if (nextStage1TutorialIndex_ >= kStage1TutorialSteps.size()) {
    return;
  }

  const Stage1TutorialStep &step =
      kStage1TutorialSteps[nextStage1TutorialIndex_];
  if (player_->GetPosition().x < step.triggerX) {
    return;
  }
  if (nextStage1TutorialIndex_ == 7 &&
      !IsPlayerOnGasPressurePlate(mapChipStage_, *player_)) {
    return;
  }

  tutorialText_->SetText(step.message);
  tutorialPanelSprite_->SetColor({0.02f, 0.05f, 0.12f, 0.82f});
  tutorialText_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
  ++nextStage1TutorialIndex_;
}

void GamePlayScene::Draw2D() {
  if (isDeathTransitionActive_) {
    SpriteManager::GetInstance()->PreDraw();
    for (const auto& lifeSprite : lifeSprites_) {
      lifeSprite->Draw();
    }
    TextRenderer::GetInstance()->PreDraw();
    livesNumberText_->Draw();
    return;
  }
  if (showForces_) {
    fluidForceRenderer_->Draw(*gpuSphFluid_, *camera_);
  }

  SpriteManager::GetInstance()->PreDraw();
  tutorialPanelSprite_->Draw();
  for (const auto& lifeSprite : lifeSprites_) {
    lifeSprite->Draw();
  }

  TextRenderer::GetInstance()->PreDraw();
  tutorialText_->Draw();
  livesNumberText_->Draw();
  pageReveal_.Draw();

  // メニュー表示中は最前面に暗幕とメニューパネルを描画
  if (isMenuOpen_) {
    SpriteManager::GetInstance()->PreDraw();
    menuBackgroundSprite_->Draw();
    menuPanelSprite_->Draw();
    menuResumeButtonSprite_->Draw();
    menuRestartButtonSprite_->Draw();
    menuStageSelectButtonSprite_->Draw();

    TextRenderer::GetInstance()->PreDraw();
    menuTitleText_->Draw();
    menuResumeText_->Draw();
    menuRestartText_->Draw();
    menuStageSelectText_->Draw();
  }
  if (isStageSelectTransitionActive_) {
    SpriteManager::GetInstance()->PreDraw();
    menuTransitionFadeSprite_->Draw();
  }
}

void GamePlayScene::Draw3D() {
  SkyBoxManager::GetInstance()->PreDraw();
  skyBox_->Draw(DirectXCommon::GetInstance()->GetCommandList());

  Object3dManager::GetInstance()->PreDraw();
  ruinsBackground_.Draw(false);
  mapChipStage_.Draw();
}

void GamePlayScene::PushSavePoint() {
  GamePlaySavePoint sp;
  sp.playerStartPosition = playerStartPosition_;
  sp.stageSnapshot = mapChipStage_.CreateStageSnapshot();
  savePointHistory_.push_back(std::move(sp));
}

void GamePlayScene::PopSavePoint() {
  if (savePointHistory_.size() > 1) {
    savePointHistory_.pop_back();
  }
}

void GamePlayScene::RestoreSavePoint() {
  if (!savePointHistory_.empty()) {
    const GamePlaySavePoint& sp = savePointHistory_.back();
    playerStartPosition_ = sp.playerStartPosition;
    mapChipStage_.RestoreStageSnapshot(sp.stageSnapshot);
  }
}

void GamePlayScene::DrawParticle() {
  EffectManager::GetInstance()->PreDraw();
  EffectManager::GetInstance()->Draw();
  if (showForces_ && gpuSphFluid_) {
    fluidForceRenderer_->Draw(*gpuSphFluid_, *camera_);
  }
}

void GamePlayScene::DrawImGui() {}

void GamePlayScene::StartStageSelectTransition() {
  if (isStageSelectTransitionActive_) {
    return;
  }
  isStageSelectTransitionActive_ = true;
  stageSelectTransitionTime_ = 0.0f;
  menuTransitionFadeSprite_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
  menuTransitionFadeSprite_->Update();
}

void GamePlayScene::UpdateFantasyMenuEffect(float deltaTime) {
  const float targetStrength = isMenuOpen_ ? 1.0f : 0.0f;
  const float step = deltaTime / kFantasyMenuBlendDuration;
  if (fantasyMenuEffectStrength_ < targetStrength) {
    fantasyMenuEffectStrength_ =
        (std::min)(fantasyMenuEffectStrength_ + step, targetStrength);
  } else if (fantasyMenuEffectStrength_ > targetStrength) {
    fantasyMenuEffectStrength_ =
        (std::max)(fantasyMenuEffectStrength_ - step, targetStrength);
  }

  SceneManager *sceneManager = SceneManager::GetInstance();
  sceneManager->SetFantasyMenuStrength(fantasyMenuEffectStrength_);
  if (!isMenuOpen_ && fantasyMenuEffectStrength_ <= 0.0f) {
    sceneManager->RemovePostEffect(PostEffectType::FantasyMenu);
  }
}

void GamePlayScene::UpdateStageSelectTransition(float deltaTime) {
  stageSelectTransitionTime_ += deltaTime;
  const float progress = std::clamp(
      stageSelectTransitionTime_ / kStageSelectFadeDuration, 0.0f, 1.0f);
  const float alpha = progress * progress * (3.0f - 2.0f * progress);
  menuTransitionFadeSprite_->SetColor({0.0f, 0.0f, 0.0f, alpha});
  menuTransitionFadeSprite_->Update();

  if (progress >= 1.0f) {
    PageTransition::RequestReveal({0.0f, 0.0f, 0.0f, 1.0f},
                                  kStageSelectFadeDuration);
    SceneManager::GetInstance()->SetNextScene(std::make_unique<ArchiveScene>());
  }
}

void GamePlayScene::RespawnPlayerLeavingCorpse() { StartLifeRelay(); }

void GamePlayScene::StartLifeRelay(bool leaveCorpse) {
  if (selfDestructSlowActive_) {
    TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
    selfDestructSlowActive_ = false;
  }

  if (leaveCorpse) {
    const GpuSphFluid::Settings currentSettings = gpuSphFluid_->GetSettings();
    const std::vector<GpuSphFluid::Particle> particles =
        gpuSphFluid_->GetParticlesCPU();

    auto corpse = std::make_unique<HardenedFluidSlimeCorpse>();
    if (corpse->InitializeFromParticles(DirectXCommon::GetInstance(),
                                        SrvManager::GetInstance(), particles,
                                        currentSettings)) {
      mapChipStage_.AddGimmick(std::move(corpse));
      mapChipStage_.LimitHardenedSlimeCount(10);
    }
  }

  GpuSphFluid::Settings hiddenSettings = gpuSphFluid_->GetSettings();
  hiddenSettings.corePosition = {0.0f, 10000.0f, 0.0f};
  gpuSphFluid_->Reset(hiddenSettings);
  SceneManager::GetInstance()->SetScreenSpaceFluid(nullptr);

  isLifeRelayActive_ = true;
  lifeRelayTimer_ = 0.0f;
  lifeRelayOrbStartPosition_ = MakeFluidCorePosition(*player_);
  lifeRelayOrbCurrentPosition_ = lifeRelayOrbStartPosition_;

  lifeRelayOrbEffectHandle_ = EffectManager::GetInstance()->PlayLoopEffect(
      "FlameCore", lifeRelayOrbCurrentPosition_);
}

void GamePlayScene::FinishLifeRelay() {
  isLifeRelayActive_ = false;

  if (EffectManager::GetInstance()->IsEffectAlive(lifeRelayOrbEffectHandle_)) {
    EffectManager::GetInstance()->StopEffect(lifeRelayOrbEffectHandle_);
    lifeRelayOrbEffectHandle_ = kInvalidEffectHandle;
  }

  EffectManager::GetInstance()->PlayEffect("BlueFireworkSparks",
                                           playerStartPosition_);

  player_->Initialize(&mapChipStage_.GetField(), playerStartPosition_);
  eyeOffsetX_ = 0.0f;

  GpuSphFluid::Settings respawnSettings = gpuSphFluid_->GetSettings();
  respawnSettings.corePosition = MakeFluidCorePosition(*player_);
  respawnSettings.floorHeight = player_->GetFluidFloorHeight();
  respawnSettings.targetVelocity = {0.0f, 0.0f, 0.0f};
  const Vector3 playerScale = player_->GetVisualScale();
  respawnSettings.blobRadii = {playerScale.x * (2.4f * kNeoWorldScale),
                               playerScale.y * (1.7f * kNeoWorldScale),
                               playerScale.z * (2.4f * kNeoWorldScale)};
  gpuSphFluid_->SetLiquidated(false);
  gpuSphFluid_->SetDeathEyes(false);
  gpuSphFluid_->Reset(respawnSettings);
  SceneManager::GetInstance()->SetScreenSpaceFluid(gpuSphFluid_.get());
}

void GamePlayScene::ResetToLastRespawnPoint() {
  // ステージは再読み込みせず、最後に有効化した中間地点（なければ開始地点）へ戻す。
  // すでに置かれた死体や、開閉済みのドアなどの進行状況は保持する。
  if (selfDestructSlowActive_) {
    TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
    selfDestructSlowActive_ = false;
  }
  // 直近の死亡で置いた死体も取り消し、詰まりから脱出できるようにする。
  if (mapChipStage_.RemoveLatestHardenedSlime()) {
    remainingLives_ = (std::min)(remainingLives_ + 1, maximumLives_);
    UpdateLivesText();
    PopSavePoint();
  }
  
  RestoreSavePoint();
  
  isLifeRelayActive_ = false;
  lifeRelayTimer_ = 0.0f;
  FinishLifeRelay();
}

void GamePlayScene::StartClearCelebration() {
  if (isClearCelebrationActive_) return;

  // 入力を止め、流体だけを弾ませて「喜び」を見せてからクリア画面へ遷移する。
  isClearCelebrationActive_ = true;
  clearCelebrationTimer_ = 0.0f;
  SceneManager* sceneManager = SceneManager::GetInstance();
  sceneManager->SetSlimeScreenProgress(0.0f);
  sceneManager->AddPostEffect(
      PostEffectType::ClearSlimeRise, PostEffectStage::AfterParticle);
  if (selfDestructSlowActive_) {
    TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
    selfDestructSlowActive_ = false;
  }
  EffectManager::GetInstance()->PlayEffect("BlueFireworkSparks", player_->GetPosition());
}

void GamePlayScene::UpdateClearCelebration(float unscaledDeltaTime) {
  constexpr float kDanceDuration = 2.0f;
  constexpr float kSlimeRiseDuration = 1.1f;
  clearCelebrationTimer_ += unscaledDeltaTime;
  const float riseProgress = std::clamp(
      (clearCelebrationTimer_ - kDanceDuration) / kSlimeRiseDuration,
      0.0f, 1.0f);
  const float easedProgress =
      riseProgress * riseProgress * (3.0f - 2.0f * riseProgress);
  SceneManager::GetInstance()->SetSlimeScreenProgress(easedProgress);

  if (riseProgress >= 1.0f) {
    isClearCelebrationActive_ = false;
    SceneManager::GetInstance()->SetNextScene(std::make_unique<ClearScene>());
  }
}

void GamePlayScene::UpdateLivesText() {
  if (displayedLives_ == remainingLives_)
    return;

  displayedLives_ = remainingLives_;
  lifeSprites_.clear();

  const float width = static_cast<float>(WinApp::GetInstance()->GetRenderWidth());
  const float height = static_cast<float>(WinApp::GetInstance()->GetRenderHeight());
  constexpr float kLifeIconSize = 46.0f;
  constexpr float kLifeIconGap = 8.0f;
  constexpr float kLifeIconBottomMargin = 18.0f;
  const float rowWidth =
      kLifeIconSize * static_cast<float>(maximumLives_) +
      kLifeIconGap * static_cast<float>((std::max)(maximumLives_ - 1, 0));
  const float rowLeft = (width - rowWidth) * 0.5f;
  const float rowTop = height - kLifeIconSize - kLifeIconBottomMargin;

  livesNumberText_->SetPosition({rowLeft - 18.0f, rowTop + kLifeIconSize * 0.5f});
  livesNumberText_->SetText(std::to_string(remainingLives_));
  livesNumberText_->SetColor(remainingLives_ <= 2
      ? Vector4{1.0f, 0.35f, 0.25f, 1.0f}
      : Vector4{1.0f, 1.0f, 1.0f, 1.0f});
  livesNumberText_->Update();

  for (int lifeIndex = 0; lifeIndex < maximumLives_; ++lifeIndex) {
    const bool isRemainingLife = lifeIndex < remainingLives_;
    auto lifeSprite = std::make_unique<Sprite>();
    lifeSprite->Initialize(SpriteManager::GetInstance(),
                           isRemainingLife ? kLifeSlimeTexture
                                           : kDeadSlimeTexture);
    lifeSprite->SetSize({kLifeIconSize, kLifeIconSize});
    lifeSprite->SetMaterial(kLifeSlimeMaterial);
    // 生きている残機だけを少しずつ位相をずらしてぷにぷに動かす。
    // 使用済みアイコンは静止させ、状態を見分けやすくする。
    lifeSprite->SetEffectAmplitude(isRemainingLife ? 0.10f : 0.0f);
    lifeSprite->SetEffectPhase(static_cast<float>(lifeIndex) * 0.62f);
    lifeSprite->SetPosition({
        rowLeft + (kLifeIconSize + kLifeIconGap) *
                      static_cast<float>(lifeIndex),
        rowTop});
    lifeSprite->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    lifeSprite->Update();
    lifeSprites_.push_back(std::move(lifeSprite));
  }
}

void GamePlayScene::LoseLife(bool leaveCorpse) {
  if (isDeathTransitionActive_ || isLifeRelayActive_ || remainingLives_ <= 0)
    return;
  --remainingLives_;
  UpdateLivesText();
  if (remainingLives_ == 0) {
    StartDeathTransition();
  } else {
    StartLifeRelay(leaveCorpse);
  }
}

void GamePlayScene::StartDeathTransition() {
  if (isDeathTransitionActive_) {
    return;
  }
  isDeathTransitionActive_ = true;
  deathTransitionTime_ = 0.0f;
  isMenuOpen_ = false;
  remainingLives_ = 0;
  UpdateLivesText();
  EffectManager::GetInstance()->StopAllEffects();
  walkingDustEffectHandle_ = kInvalidEffectHandle;
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
  settings.gravity = {0.0f, -2.0f, 0.0f};
  settings.liquidGravityScale = 1.0f;
  settings.liquidShapeAttraction = 0.0f;
  settings.liquidVelocityAttraction = 0.0f;
  settings.liquidViscosity = 0.0f;
  settings.liquidSurfaceTension = 0.0f;
  settings.liquidDamping = 0.0f;
  settings.floorHeight = center.y - 30.0f;
  settings.boundsMin = {center.x - 40.0f, center.y - 40.0f, -40.0f};
  settings.boundsMax = {center.x + 40.0f, center.y + 40.0f, 40.0f};
  gpuSphFluid_->SetLiquidated(true);
  gpuSphFluid_->SetDeathEyes(true);
  gpuSphFluid_->Reset(settings);
  gpuSphFluid_->SetWallBoundaries(center.x - 40.0f, center.x + 40.0f, -40.0f,
                                  40.0f, center.y - 40.0f, center.y + 40.0f);
  gpuSphFluid_->SetGrounded(false);
  gpuSphFluid_->SetEmitter(false, center, {0.0f, 0.0f, 0.0f});
  for (size_t i = 0; i < particles.size(); ++i) {
    auto &particle = particles[i];
    const float angle = static_cast<float>(i) * 2.39996323f;
    const float spread = 1.5f + static_cast<float>(i % 17) * 0.24f;
    // Front-facing droplets reach the lens around 0.6 seconds after rupture.
    particle.velocity = {std::cos(angle) * spread,
                         std::sin(angle) * spread + 1.0f,
                         i % 3 == 0 ? -18.0f - static_cast<float>(i % 7)
                                    : 2.0f * std::sin(angle)};
    particle.padding = 5.0f;
  }
  gpuSphFluid_->SetParticlesCPU(particles);
  SceneManager *sceneManager = SceneManager::GetInstance();
  const Vector3 position = player_->GetPosition();
  sceneManager->SetPaintSeed(position.x * 17.31f + position.y * 7.13f);
  sceneManager->SetSlimeScreenProgress(0.0f);
  sceneManager->AddPostEffect(PostEffectType::SlimeScreen,
                              PostEffectStage::AfterParticle);
}

void GamePlayScene::UpdateDeathTransition(float deltaTime) {
  constexpr float kCoverDuration = 2.1f;
  constexpr float kFlightDuration = 0.55f;
  deathTransitionTime_ += deltaTime;
  gpuSphFluid_->Update(deltaTime);
  const float progress = std::clamp(
      (deathTransitionTime_ - kFlightDuration) / kCoverDuration, 0.0f, 1.0f);
  SceneManager::GetInstance()->SetSlimeScreenProgress(progress);
  if (progress >= 1.0f) {
    PageTransition::RequestSlimeReveal();
    SceneManager::GetInstance()->SetNextScene(
        std::make_unique<GameOverScene>());
  }
}

/**
 * @brief カメラの注視点（ターゲット）座標が、マップ境界外を映さないように制限（クランプ）する
 * @param targetPosition 本来カメラが追従したい理想の座標
 * @return 画面内にマップ外の未配置領域が映らないように補正された安全な座標
 */
Vector3 GamePlayScene::ClampCameraTarget(const Vector3& targetPosition) const {
  if (!camera_) return targetPosition;

  // カメラの視錐台から、現在の距離（kCameraDistance）における画面半分のサイズを算出
  float fovY = camera_->GetFovY();
  float aspectRatio = camera_->GetAspectRatio();
  float halfHeight = std::tan(fovY * 0.5f) * kCameraDistance;
  float halfWidth = halfHeight * aspectRatio;

  // マップの物理的な境界（ブロック数 × ブロックサイズ）を取得
  float mapWidth = static_cast<float>(mapChipStage_.GetField().GetBlockWidth());
  float mapHeight = static_cast<float>(mapChipStage_.GetField().GetBlockHeight());

  // 万が一マップが1画面に収まりきらないほど小さい場合のフェールセーフ（中央固定）
  float minX = (std::min)(halfWidth, mapWidth * 0.5f);
  float maxX = (std::max)(halfWidth, mapWidth - halfWidth);
  float minY = (std::min)(halfHeight, mapHeight * 0.5f);
  float maxY = (std::max)(halfHeight, mapHeight - halfHeight);

  Vector3 clampedPosition = targetPosition;
  clampedPosition.x = std::clamp(clampedPosition.x, minX, maxX);
  clampedPosition.y = std::clamp(clampedPosition.y, minY, maxY);

  return clampedPosition;
}

void GamePlayScene::UpdateFollowCamera() {
  if (!player_) {
    return;
  }
  Vector3 targetPosition = player_->GetPosition();
  if (isLifeRelayActive_) {
    targetPosition = lifeRelayOrbCurrentPosition_;
  }

  // マップ境界はみ出し防止のクランプ処理を適用
  targetPosition = ClampCameraTarget(targetPosition);

  camera_->LookAt({targetPosition.x, targetPosition.y, -kCameraDistance},
                  {targetPosition.x, targetPosition.y, 0.0f});
}
