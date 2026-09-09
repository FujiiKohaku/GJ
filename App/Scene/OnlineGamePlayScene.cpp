#include "OnlineGamePlayScene.h"
#include "ArchiveScene.h"
#include "ClearScene.h"
#include "GameOverScene.h"
#include "SceneManager.h"
#include "Engine/Network/EosMultiplayer.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Input/Input.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <regex>

namespace {
constexpr const char* Font = "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* SkyBoxTexture = "resources/Textures/skybox.dds";
constexpr const char* LifeSlimeTexture = "resources/Textures/Slime.png";
constexpr const char* DeadSlimeTexture = "resources/Textures/deadSlime.png";
constexpr const char* LifeSlimeMaterial = "resources/Shaders/Sprite/LifeSlime";
constexpr const char* KeyWTexture = "resources/Textures/W.png";
constexpr const char* KeyATexture = "resources/Textures/A.png";
constexpr const char* KeySTexture = "resources/Textures/S.png";
constexpr const char* KeyDTexture = "resources/Textures/D.png";
constexpr const char* MouseTexture = "resources/Textures/mouse.png";
constexpr float NeoWorldScale = 0.36f;
constexpr float CameraDistance = 12.0f;
constexpr Vector3 SlimeRenderForward = {0, 0, 1};
constexpr const char* SlowWaterSoundName = "SlowWater";
constexpr const char* SlowWaterSoundPath = "resources/Audio/Scene/player/水中.mp3";
constexpr const char* SlimeMoveSoundName = "SlimeMove";
constexpr const char* SlimeMoveSoundPath = "resources/Audio/Scene/player/ゾンビの食事.mp3";
constexpr const char* ConfirmSoundName = "StageSelect.Confirm";
constexpr const char* ConfirmSoundPath = "resources/Audio/StageSelect/confirm.wav";
constexpr const char *kClearPourSoundName = "Scene.Transition.ClearPour";
constexpr const char *kClearPourSoundPath = "resources/Audio/Scene/clear_transition_pour.wav";
constexpr const char *kDeathSplatSoundName = "Scene.Transition.DeathSplat";
constexpr const char *kDeathSplatSoundPath = "resources/Audio/Scene/game_over_transition_splat.wav";
struct TutorialStep { float triggerX; const char* message; };
constexpr std::array<TutorialStep, 10> TutorialSteps{{
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
constexpr const char* ShapeTutorialMessage =
    "左クリック長押しで、硬化する前のスライムの形を少し変えられる";

bool Intersects(const AABB& a, const AABB& b) {
    const Vector3 ah = a.size * 0.5f, bh = b.size * 0.5f;
    return std::abs(a.center.x - b.center.x) <= ah.x + bh.x &&
           std::abs(a.center.y - b.center.y) <= ah.y + bh.y &&
           std::abs(a.center.z - b.center.z) <= ah.z + bh.z;
}
bool IsOnGasPressurePlate(const MapChipStage& stage,
                          const MapChipPlayer& player) {
    for (auto* gimmick : stage.GetGimmicks()) {
        if (gimmick && gimmick->GetLinkName() == "Event_2" &&
            gimmick->GetAABB().center.x < 86.0f &&
            Intersects(player.GetAABB(), gimmick->GetAABB())) return true;
    }
    return false;
}
Vector3 FluidCorePosition(const MapChipPlayer& player) {
    Vector3 result = player.IsShapingSelfDestruct()
        ? player.GetAABB().center : player.GetPosition();
    result.y += 0.086f * NeoWorldScale;
    return result;
}
GpuSphFluid::CollisionObstacle FluidObstacle(
    const Vector3& center, const Vector3& size, const Vector3& velocity) {
    GpuSphFluid::CollisionObstacle result{};
    result.center = center;
    result.halfSize = {size.x * 0.5f, size.y * 0.5f, 0.65f};
    result.velocity = {velocity.x, velocity.y, 0};
    return result;
}
std::vector<GpuSphFluid::CollisionObstacle> BuildFluidObstacles(
    const MapChipStage& stage,
    const std::vector<BaseMapChipGimmick*>& gimmicks, float deltaTime) {
    std::vector<GpuSphFluid::CollisionObstacle> result;
    const auto& field = stage.GetField();
    result.reserve(static_cast<size_t>(field.GetBlockWidth()) *
                   field.GetBlockHeight() + gimmicks.size());
    for (uint32_t y = 0; y < field.GetBlockHeight(); ++y) {
        for (uint32_t x = 0; x < field.GetBlockWidth(); ++x) {
            const auto type = field.GetMapChipTypeByIndex(x, y);
            if (type == MapChipType::Block || type == MapChipType::Foundation) {
                result.push_back(FluidObstacle(
                    field.GetMapChipPositionByIndex(x, y) + stage.GetWorldOffset(),
                    {1, 1, 1}, {}));
            }
        }
    }
    const float safeDeltaTime = (std::max)(deltaTime, 0.0001f);
    for (auto* gimmick : gimmicks) {
        if (!gimmick || !gimmick->IsSolid()) continue;
        const auto delta = gimmick->GetDeltaPosition();
        const Vector3 velocity{delta.x / safeDeltaTime, delta.y / safeDeltaTime,
                               delta.z / safeDeltaTime};
        for (const auto& box : gimmick->GetCollisionBoxes())
            result.push_back(FluidObstacle(box.center, box.size, velocity));
    }
    return result;
}
void HashByte(uint64_t& hash, uint8_t byte) { hash ^= byte; hash *= 1099511628211ull; }
uint64_t HashFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Map missing");
    uint64_t hash = 14695981039346656037ull;
    char ch; while (file.get(ch)) HashByte(hash, static_cast<uint8_t>(ch));
    return hash;
}
bool LeaveHovered() {
    const auto p = Input::GetInstance()->GetMousePosition();
    return p.x >= 1010 && p.x < 1250 && p.y >= 124 && p.y < 166;
}
// Co-op uses the confirmed collision shape as a shared platform, independent
// of each GPU's fluid simulation. Every peer builds this same geometry.
class CoopBody : public BaseMapChipGimmick {
public:
    explicit CoopBody(AABB box) : box_(box) {}
    bool Initialize(const Vector3&, const std::string&, const BaseGimmickParam*) override {
        return true;
    }
    void Update() override {}
    void Draw() override {}
    AABB GetAABB() const override { return box_; }
    bool IsHardenedSlime() const override { return true; }
private:
    AABB box_;
};
}

OnlineGamePlayScene::OnlineGamePlayScene(std::string stageFile) : stageFile_(std::move(stageFile)) {}
void OnlineGamePlayScene::Initialize() {
    auto& online = EosMultiplayer::Get();
    host_ = online.IsHost(); localSlot_ = online.LocalSlot(); playerCount_ = static_cast<int>(online.Members().size()); match_ = online.MatchId();
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::ArchiveAtmosphere);
    SceneManager::GetInstance()->SetArchiveApproach(0.0f);
    SceneManager::GetInstance()->SetSlimeScreenProgress(0.0f);
    TimeManager::GetInstance()->SetTimeScale(1);
    camera_ = std::make_unique<Camera>(); camera_->Initialize();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    EffectManager::GetInstance()->SetCamera(camera_.get());
    SoundManager::GetInstance()->Load(SlowWaterSoundName, SlowWaterSoundPath,
                                      AudioCategory::SE);
    SoundManager::GetInstance()->Load(SlimeMoveSoundName, SlimeMoveSoundPath,
                                      AudioCategory::SE);
    SoundManager::GetInstance()->Load(ConfirmSoundName, ConfirmSoundPath,
                                      AudioCategory::SE);
    SoundManager::GetInstance()->Load(kClearPourSoundName, kClearPourSoundPath, AudioCategory::SE);
    SoundManager::GetInstance()->Load(kDeathSplatSoundName, kDeathSplatSoundPath, AudioCategory::SE);
    hud_ = std::make_unique<Text>(); hud_->Initialize(Font); hud_->SetFontSize(18); hud_->SetPosition({28, 132}); hud_->SetMaxWidth(950);
    leaveText_ = std::make_unique<Text>(); leaveText_->Initialize(Font); leaveText_->SetFontSize(18);
    leaveText_->SetPosition({1130, 145}); leaveText_->SetAnchorPoint({0.5f, 0.5f}); leaveText_->SetText("ロビーへ戻る"); leaveText_->Update();
    hudBackground_ = std::make_unique<Sprite>(); hudBackground_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    hudBackground_->SetPosition({12, 124}); hudBackground_->SetSize({986, 42}); hudBackground_->SetColor({0.025f, 0.04f, 0.06f, 0.76f}); hudBackground_->Update();
    leaveButton_ = std::make_unique<Sprite>(); leaveButton_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    leaveButton_->SetPosition({1010, 124}); leaveButton_->SetSize({240, 42});
    try {
        // Only allow bundled stage filenames; never load a path supplied by a peer.
        if (!std::regex_match(stageFile_, std::regex("stage([0-9]+|_test)\\.json")) ||
            playerCount_ < EosMultiplayer::MinPlayers || playerCount_ > EosMultiplayer::MaxPlayers ||
            localSlot_ < 0 || localSlot_ >= playerCount_)
            throw std::runtime_error("Invalid stage or roster");
        const auto path = std::filesystem::path("resources/Maps") / stageFile_;
        mapHash_ = HashFile(path);
        LevelDataLoader loader;
        const auto level = loader.Load(path.string());
        maximumLives_ = level.maximumLives;
        lives_.fill(maximumLives_);
        stage_.Initialize(level); stage_.ApplyMaterialProperties();
        std::vector<MapChipPlayer*> activePlayers;
        for (int i = 0; i < playerCount_; ++i) {
            spawn_[i] = level.playerSpawns.empty() ? Vector3{1, 2, 0} : level.playerSpawns[static_cast<size_t>(i) < level.playerSpawns.size() ? i : 0].translation;
            players_[i].Initialize(&stage_.GetField(), spawn_[i]);
            activePlayers.push_back(&players_[i]);
        }
        stage_.SetPlayers(activePlayers);
        RuinsBackground::Settings settings;
        settings.mapLength = static_cast<float>(stage_.GetField().GetBlockWidth()); background_.Initialize(settings);

        TextureManager::GetInstance()->LoadTexture(SkyBoxTexture);
        skyBox_ = std::make_unique<SkyBox>();
        skyBox_->Initialize(DirectXCommon::GetInstance());
        skyBox_->SetTexture(SkyBoxTexture);
        skyBox_->Update(camera_.get());
        InitializePlayerFluids();

        tutorialPanelSprite_ = std::make_unique<Sprite>();
        tutorialPanelSprite_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
        tutorialPanelSprite_->SetPosition({60, 16});
        tutorialPanelSprite_->SetSize({1160, 92});
        tutorialPanelSprite_->SetColor({0.02f, 0.05f, 0.12f, 0});
        tutorialPanelSprite_->Update();
        tutorialText_ = std::make_unique<Text>();
        tutorialText_->Initialize(Font); tutorialText_->SetAnchorPoint({0.5f, 0.5f});
        tutorialText_->SetPosition({640, 61}); tutorialText_->SetFontSize(42);
        tutorialText_->SetColor({1, 1, 1, 0}); tutorialText_->SetOutlineWidth(0);
        tutorialText_->SetShadowColor({0, 0, 0, 0});
        if (stageFile_ == "stage1.json") {
            std::string preload;
            for (const auto& step : TutorialSteps) preload += step.message;
            preload += ShapeTutorialMessage;
            tutorialText_->SetText(preload); tutorialText_->Update();
            tutorialText_->SetText(""); tutorialText_->Update();
        }

        TextureManager::GetInstance()->LoadTexture(LifeSlimeTexture);
        TextureManager::GetInstance()->LoadTexture(DeadSlimeTexture);
        livesNumberText_ = std::make_unique<Text>();
        livesNumberText_->Initialize(Font, true);
        livesNumberText_->SetAnchorPoint({1, 0.5f});
        livesNumberText_->SetFontSize(48); livesNumberText_->SetOutlineWidth(2);
        livesNumberText_->SetOutlineColor({0, 0.015f, 0.04f, 1});
        livesNumberText_->SetShadowColor({0, 0, 0, 0});
        UpdateLivesDisplay();

        if (stageFile_ == "stage1.json") {
            for (const char* texture : {KeyWTexture, KeyATexture, KeySTexture, KeyDTexture, MouseTexture})
                TextureManager::GetInstance()->LoadTexture(texture);
            const auto makeKey = [](const char* texture, Vector2 size) {
                auto sprite = std::make_unique<Sprite>();
                sprite->Initialize(SpriteManager::GetInstance(), texture);
                sprite->SetSize(size); return sprite;
            };
            tutorialKeyWSprite_ = makeKey(KeyWTexture, {48, 48});
            tutorialKeyASprite_ = makeKey(KeyATexture, {48, 48});
            tutorialKeySSprite_ = makeKey(KeySTexture, {48, 48});
            tutorialKeyDSprite_ = makeKey(KeyDTexture, {48, 48});
            tutorialMouseSprite_ = makeKey(MouseTexture, {72, 96});
            const float width = static_cast<float>(WinApp::GetInstance()->GetRenderWidth());
            const float height = static_cast<float>(WinApp::GetInstance()->GetRenderHeight());
            const float centerX = width * 0.5f, baseY = height - 154.0f;
            tutorialKeyWSprite_->SetPosition({centerX - 50, baseY - 50});
            tutorialKeyASprite_->SetPosition({centerX - 100, baseY});
            tutorialKeySSprite_->SetPosition({centerX - 50, baseY});
            tutorialKeyDSprite_->SetPosition({centerX, baseY});
            tutorialMouseSprite_->SetPosition({centerX + 90, baseY - 32});
            controlsText_ = std::make_unique<Text>(); controlsText_->Initialize(Font);
            controlsText_->SetAnchorPoint({0, 1}); controlsText_->SetPosition({20, height - 20});
            controlsText_->SetFontSize(24); controlsText_->SetColor({1, 1, 1, 1});
            controlsText_->SetOutlineColor({0.02f, 0.05f, 0.12f, 1});
            controlsText_->SetOutlineWidth(2);
            controlsText_->SetText("WASD：移動　SPACE：ジャンプ\n右クリック：自滅スロー\n右クリック(スロー中)：自滅確定\n左クリック長押し＋移動(スロー中)：変形");
            controlsText_->Update();
        }
        loaded_ = true; loadedPeers_[localSlot_] = true;
    } catch (...) { Fail("ステージを読み込めません。同じゲームデータを各PCに配置してください"); }
}
void OnlineGamePlayScene::Finalize() {
    if (selfDestructSlowActive_)
        TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
    auto* audio = SoundManager::GetInstance();
    if (slowWaterSoundHandle_.IsValid()) audio->Stop(slowWaterSoundHandle_);
    if (slimeMoveSoundHandle_.IsValid()) audio->Stop(slimeMoveSoundHandle_);
    auto* sceneManager = SceneManager::GetInstance();
    sceneManager->SetScreenSpaceFluid(nullptr);
    sceneManager->ClearExtraScreenSpaceFluids();
    sceneManager->RemovePostEffect(PostEffectType::ClearSlimeRise);
    sceneManager->SetSlimeScreenProgress(0);
    for (auto& fluid : playerFluids_) if (fluid) fluid->Finalize();
    for (auto& corpse : visualCorpses_)
        if (corpse && corpse->GetFluid()) corpse->GetFluid()->Finalize();
    EffectManager::GetInstance()->StopAllEffects();
    EffectManager::GetInstance()->SetCamera(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    TimeManager::GetInstance()->SetTimeScale(1);
}
void OnlineGamePlayScene::Fail(const std::string& reason) {
    if (failed_) return;
    failed_ = true; error_ = reason;
    EosMultiplayer::Get().Leave();
}
void OnlineGamePlayScene::SendInput() {
    auto& online = EosMultiplayer::Get();
    if (host_) {
        OnlineProtocol::Accumulate(pending_[0], localInput_);
    } else {
        auto message = OnlineProtocol::Message("input", match_);
        message["seq"] = ++inputSequence_; message["ack"] = tick_;
        message["input"] = OnlineProtocol::EncodeInput(localInput_);
        if (!online.Send(0, OnlineProtocol::Encode(message))) { Fail("入力の送信に失敗しました"); return; }
    }
    OnlineProtocol::ConsumeTransient(localInput_);
}
void OnlineGamePlayScene::ProcessPackets() {
    auto& online = EosMultiplayer::Get();
    for (const auto& packet : online.Receive()) {
        try {
            const auto message = OnlineProtocol::Decode(packet.data, match_);
            const auto type = message.at("type").get<std::string>();
            const int sender = packet.sender;
            if (sender < 0 || sender >= playerCount_ || sender == localSlot_) continue;
            if (host_ && sender != 0 && type == "loaded") {
                if (message.at("map").get<uint64_t>() != mapHash_) { Fail("参加者のステージデータが一致しません"); return; }
                loadedPeers_[sender] = true; silence_[sender] = 0;
            } else if (host_ && sender != 0 && type == "input") {
                auto input = OnlineProtocol::DecodeInput(message.at("input"));
                const auto seq = message.at("seq").get<uint64_t>();
                const auto ack = message.at("ack").get<uint64_t>();
                if (seq <= sequences_[sender] || ack > tick_) continue;
                sequences_[sender] = seq; acknowledged_[sender] = ack;
                OnlineProtocol::Accumulate(pending_[sender], input);
                silence_[sender] = 0;
            } else if (!host_ && sender == 0 && type == "frame") {
                if (message.at("tick").get<uint64_t>() != tick_ + 1) { Fail("通信フレームの順序が一致しません"); return; }
                const auto& values = message.at("inputs");
                if (!values.is_array() || values.size() != static_cast<size_t>(playerCount_)) throw std::runtime_error("Invalid roster");
                std::array<OnlineProtocol::Input, 3> inputs;
                for (int i = 0; i < playerCount_; ++i) inputs[i] = OnlineProtocol::DecodeInput(values[i]);
                const auto expectedState = message.at("state").get<uint64_t>();
                Simulate(inputs); ++tick_; silence_[0] = 0;
                if (expectedState != StateHash()) { Fail("ゲーム状態が一致しません。同じビルドで再接続してください"); return; }
            } else if (!host_ && sender == 0 && type == "waiting") {
                silence_[0] = 0;
            }
        } catch (...) {
            // Invalid or stale packets cannot become movement commands.
            continue;
        }
        if (failed_) return;
    }
}
void OnlineGamePlayScene::InitializePlayerFluids() {
    auto* sceneManager = SceneManager::GetInstance();
    sceneManager->SetScreenSpaceFluid(nullptr);
    for (int i = 0; i < playerCount_; ++i) {
        GpuSphFluid::Settings settings;
        settings.particleCount = 2048;
        settings.particleRadius = 0.20f * NeoWorldScale;
        settings.smoothingRadius = 0.40f * NeoWorldScale;
        settings.particleMass = NeoWorldScale * NeoWorldScale * NeoWorldScale;
        settings.restDensity = 3.0f;
        settings.blobRadii = {1.2f * NeoWorldScale, 0.85f * NeoWorldScale,
                              1.2f * NeoWorldScale};
        settings.stiffness = 50.0f;
        settings.shapeAttraction = 80.0f;
        settings.velocityAttraction = 0.0f;
        settings.viscosity = 15.0f;
        settings.surfaceTension = 0.0f;
        settings.gravity = {0, -20.0f * NeoWorldScale, 0};
        settings.damping = 0.985f;
        settings.horizontalFriction = 0.60f;
        settings.liquidShapeAttraction = 0.0f;
        settings.liquidVelocityAttraction = 0.0f;
        settings.liquidViscosity = 1.15f;
        settings.liquidSurfaceTension = 1.8f;
        settings.liquidDamping = 0.04f;
        settings.liquidHorizontalFriction = 0.992f;
        settings.liquidGravityScale = 1.45f;
        settings.sloshStrength = 0.0f;
        settings.puddleSpread = 0.0f;
        settings.emitterRate = 560.0f;
        settings.emitterRadius = 0.20f * NeoWorldScale;
        settings.emitterSpeed = 6.4f * NeoWorldScale;
        settings.particleLifetime = 6.0f;
        settings.collisionFriction = 0.60f;
        settings.collisionBounce = 0.30f;
        settings.simulationSubsteps = 1;
        settings.corePosition = FluidCorePosition(players_[i]);
        settings.floorHeight = players_[i].GetFluidFloorHeight();
        settings.boundsMin = {-4, -20, -2};
        settings.boundsMax = {
            static_cast<float>(stage_.GetField().GetBlockWidth()) + 4,
            static_cast<float>(stage_.GetField().GetBlockHeight()) + 8, 6};
        playerFluids_[i] = std::make_unique<GpuSphFluid>();
        playerFluids_[i]->Initialize(DirectXCommon::GetInstance(),
                                     SrvManager::GetInstance(), settings);
        playerFluids_[i]->SetLiquidated(false);
        
        float hueShift = 0.0f;
        if (i == 1) hueShift = 2.0944f;
        else if (i == 2) hueShift = -2.0944f;
        playerFluids_[i]->SetHueShift(hueShift);

        if (i == 0) sceneManager->SetScreenSpaceFluid(playerFluids_[i].get());
        else sceneManager->AddExtraScreenSpaceFluid(playerFluids_[i].get());
    }
}
void OnlineGamePlayScene::UpdatePlayerFluids(float deltaTime) {
    if (!loaded_) return;
    const auto gimmicks = stage_.GetGimmicks();
    const auto obstacles = BuildFluidObstacles(
        stage_, gimmicks, TimeManager::GetInstance()->GetUnscaledDeltaTime());
    for (int i = 0; i < playerCount_; ++i) {
        auto& fluid = playerFluids_[i];
        auto& player = players_[i];
        if (!fluid) continue;
        if (isDeathTransitionActive_ && i == (localSlot_ >= 0 ? localSlot_ : 0)) {
            continue; // Handled by UpdateDeathTransition
        }
        if (relayActive_[i]) {
            if (EffectManager::GetInstance()->IsEffectAlive(walkingDustEffect_[i])) {
                EffectManager::GetInstance()->StopEffect(walkingDustEffect_[i]);
                walkingDustEffect_[i] = kInvalidEffectHandle;
            }
            continue;
        }
        Vector3 core = FluidCorePosition(player);
        Vector3 velocity{player.GetVelocity().x, player.GetVelocity().y, 0};
        const Vector3 scale = player.GetVisualScale();
        Vector3 radii{scale.x * (2.4f * NeoWorldScale),
                      scale.y * (1.7f * NeoWorldScale),
                      scale.z * (2.4f * NeoWorldScale)};
        if (clearCelebrationActive_) {
            const float phase = clearCelebrationTimer_ + i * 0.18f;
            const float bounce = std::abs(std::sin(phase * 11.0f));
            radii.x *= 1.34f - bounce * 0.18f;
            radii.y *= 1.48f + bounce * 0.52f;
            radii.z *= 1.26f;
            core.x += std::sin(phase * 7.0f) * 1.10f;
            core.y += bounce * 0.72f;
            velocity = {std::cos(phase * 7.0f) * 7.7f,
                        std::cos(phase * 11.0f) * 2.4f, 0};
            fluid->SetGrounded(false);
        } else {
            fluid->SetGrounded(player.IsGrounded());
        }
        if (fluidReset_[i]) {
            auto settings = fluid->GetSettings();
            settings.corePosition = core;
            settings.floorHeight = player.GetFluidFloorHeight();
            settings.targetVelocity = {};
            settings.blobRadii = radii;
            fluid->Reset(settings);
            fluidReset_[i] = false;
        }
        fluid->SetObstacles(obstacles);
        fluid->SetFloorHeight(player.GetFluidFloorHeight());
        fluid->SetBlobRadii(radii);
        float minX = -1000, maxX = 1000, maxY = 1000;
        player.GetWallBoundaries(minX, maxX, maxY, gimmicks);
        const float zEnvelope = radii.z * 1.2f;
        fluid->SetWallBoundaries(minX, maxX, core.z - zEnvelope,
                                 core.z + zEnvelope, -1000, maxY);
        fluid->SetLiquidated(false);
        fluid->SetDeathEyes(player.IsShapingSelfDestruct());
        const float desiredEye = std::clamp(player.GetVelocity().x / 5.0f,
                                            -1.0f, 1.0f) * 0.075f;
        eyeOffsetX_[i] += std::clamp(desiredEye - eyeOffsetX_[i],
                                    -0.90f * deltaTime, 0.90f * deltaTime);
        const auto shapeEye = player.GetEyeOffset();
        fluid->SetEyeOffsetX(eyeOffsetX_[i] + shapeEye.x);
        fluid->SetEyeOffsetY(shapeEye.y);
        fluid->SetControlState(core, velocity, SlimeRenderForward);
        fluid->SetEmitter(false, core, {});
        fluid->Update(deltaTime);

        const float horizontalSpeed = clearCelebrationActive_
            ? 0.0f : std::abs(player.GetVelocity().x);
        const bool emitDust = player.IsGrounded() && horizontalSpeed > 0.45f;
        auto* effects = EffectManager::GetInstance();
        if (emitDust) {
            const float direction = player.GetVelocity().x >= 0 ? 1.0f : -1.0f;
            Vector3 dust = FluidCorePosition(player);
            dust.x -= direction * radii.x * 0.92f;
            dust.y = player.GetFluidFloorHeight() + 0.14f;
            if (!effects->IsEffectAlive(walkingDustEffect_[i]))
                walkingDustEffect_[i] = effects->PlayLoopEffect("WalkDust", dust);
            effects->SetEffectPosition(walkingDustEffect_[i], dust);
            effects->SetEffectVelocity(walkingDustEffect_[i],
                {-direction * (0.30f + horizontalSpeed * 0.08f), 0.16f, 0});
        } else if (effects->IsEffectAlive(walkingDustEffect_[i])) {
            effects->StopEffect(walkingDustEffect_[i]);
            walkingDustEffect_[i] = kInvalidEffectHandle;
        }
    }
    for (auto& corpse : visualCorpses_) corpse->Update();
    RebuildFluidRenderList();
}
void OnlineGamePlayScene::UpdateDeathVisuals(float deltaTime) {
    constexpr float relayDuration = 1.5f;
    for (int i = 0; i < playerCount_; ++i) {
        if (corpsePending_[i] && playerFluids_[i]) {
            auto corpse = std::make_unique<HardenedFluidSlimeCorpse>();
            if (corpse->InitializeFromParticles(
                    DirectXCommon::GetInstance(), SrvManager::GetInstance(),
                    playerFluids_[i]->GetParticlesCPU(),
                    playerFluids_[i]->GetSettings())) {
                visualCorpses_.push_back(std::move(corpse));
                if (visualCorpses_.size() > 10) visualCorpses_.erase(visualCorpses_.begin());
            }
            corpsePending_[i] = false;
        }

        if (relayActive_[i] && !relayVisualActive_[i]) {
            relayVisualActive_[i] = true;
            relayVisualTimer_[i] = 0;
            relayPosition_[i] = relayStart_[i];
            relayEffect_[i] = EffectManager::GetInstance()->PlayLoopEffect(
                "FlameCore", relayStart_[i]);
        }
        if (relayActive_[i]) {
            relayVisualTimer_[i] += deltaTime;
            const float t = std::clamp(relayVisualTimer_[i] / relayDuration,
                                       0.0f, 1.0f);
            const float smooth = t * t * (3.0f - 2.0f * t);
            const Vector3 position = Lerp(relayStart_[i], spawn_[i], smooth);
            relayPosition_[i] = position;
            auto* effects = EffectManager::GetInstance();
            if (effects->IsEffectAlive(relayEffect_[i]))
                effects->SetEffectPosition(relayEffect_[i], position);
            else
                relayEffect_[i] = effects->PlayLoopEffect("FlameCore", position);
        } else if (relayVisualActive_[i]) {
            auto* effects = EffectManager::GetInstance();
            if (effects->IsEffectAlive(relayEffect_[i]))
                effects->StopEffect(relayEffect_[i]);
            relayEffect_[i] = kInvalidEffectHandle;
            relayVisualActive_[i] = false;
            effects->PlayEffect("BlueFireworkSparks", spawn_[i]);
        }
    }
}
void OnlineGamePlayScene::RebuildFluidRenderList() {
    auto* sceneManager = SceneManager::GetInstance();
    sceneManager->SetScreenSpaceFluid(nullptr);
    bool first = true;
    const auto add = [sceneManager, &first](GpuSphFluid* fluid) {
        if (!fluid) return;
        if (first) {
            sceneManager->SetScreenSpaceFluid(fluid);
            first = false;
        } else {
            sceneManager->AddExtraScreenSpaceFluid(fluid);
        }
    };
    for (int i = 0; i < playerCount_; ++i)
        if (!relayActive_[i]) add(playerFluids_[i].get());
    for (const auto& corpse : visualCorpses_) add(corpse->GetFluid());
}
Vector3 OnlineGamePlayScene::ClampCameraTarget(const Vector3& target) const {
    if (!camera_) return target;
    const float halfHeight = std::tan(camera_->GetFovY() * 0.5f) * CameraDistance;
    const float halfWidth = halfHeight * camera_->GetAspectRatio();
    const float mapWidth = static_cast<float>(stage_.GetField().GetBlockWidth());
    const float mapHeight = static_cast<float>(stage_.GetField().GetBlockHeight());
    const float minX = (std::min)(halfWidth, mapWidth * 0.5f);
    const float maxX = (std::max)(halfWidth, mapWidth - halfWidth);
    const float minY = (std::min)(halfHeight, mapHeight * 0.5f);
    const float maxY = (std::max)(halfHeight, mapHeight - halfHeight);
    Vector3 result = target;
    result.x = std::clamp(result.x, minX, maxX);
    result.y = std::clamp(result.y, minY, maxY);
    return result;
}
void OnlineGamePlayScene::UpdateFollowCamera() {
    if (!loaded_ || localSlot_ < 0) return;
    Vector3 target = relayVisualActive_[localSlot_]
        ? relayPosition_[localSlot_] : players_[localSlot_].GetPosition();
    target.z = 0;
    target = ClampCameraTarget(target);
    camera_->LookAt({target.x, target.y, target.z - CameraDistance}, target);
}
void OnlineGamePlayScene::UpdateLocalSlowMotion() {
    if (!loaded_ || localSlot_ < 0) return;
    const bool shouldSlow = !cleared_ && players_[localSlot_].IsShapingSelfDestruct();
    auto* time = TimeManager::GetInstance();
    if (shouldSlow && !selfDestructSlowActive_) {
        timeScaleBeforeSelfDestruct_ = time->GetTimeScale();
        time->SetTimeScale(0.08f);
        selfDestructSlowActive_ = true;
    } else if (!shouldSlow && selfDestructSlowActive_) {
        time->SetTimeScale(timeScaleBeforeSelfDestruct_);
        selfDestructSlowActive_ = false;
    }

    auto* audio = SoundManager::GetInstance();
    if (shouldSlow) {
        if (!slowWaterSoundHandle_.IsValid())
            slowWaterSoundHandle_ = audio->Play(SlowWaterSoundName, true, 0.6f);
    } else if (slowWaterSoundHandle_.IsValid()) {
        audio->Stop(slowWaterSoundHandle_); slowWaterSoundHandle_ = {};
    }
    const bool moving = !relayActive_[localSlot_] && !cleared_ &&
        players_[localSlot_].IsGrounded() &&
        std::abs(players_[localSlot_].GetVelocity().x) > 0.25f;
    if (moving) {
        if (!slimeMoveSoundHandle_.IsValid())
            slimeMoveSoundHandle_ = audio->Play(SlimeMoveSoundName, true, 0.5f);
    } else if (slimeMoveSoundHandle_.IsValid()) {
        audio->Stop(slimeMoveSoundHandle_); slimeMoveSoundHandle_ = {};
    }
}
void OnlineGamePlayScene::UpdateLivesDisplay() {
    if (!livesNumberText_ || localSlot_ < 0 ||
        displayedLives_ == lives_[localSlot_]) return;
    displayedLives_ = lives_[localSlot_];
    lifeSprites_.clear();
    const float width = static_cast<float>(WinApp::GetInstance()->GetRenderWidth());
    const float height = static_cast<float>(WinApp::GetInstance()->GetRenderHeight());
    constexpr float icon = 46, gap = 8;
    const float rowWidth = icon * maximumLives_ +
                           gap * (std::max)(maximumLives_ - 1, 0);
    const float left = (width - rowWidth) * 0.5f;
    const float top = height - icon - 18;
    livesNumberText_->SetPosition({left - 18, top + icon * 0.5f});
    livesNumberText_->SetText(std::to_string(displayedLives_));
    livesNumberText_->SetColor(displayedLives_ <= 2
        ? Vector4{1, 0.35f, 0.25f, 1} : Vector4{1, 1, 1, 1});
    livesNumberText_->Update();
    for (int index = 0; index < maximumLives_; ++index) {
        const bool alive = index < displayedLives_;
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(SpriteManager::GetInstance(),
                           alive ? LifeSlimeTexture : DeadSlimeTexture);
        sprite->SetSize({icon, icon}); sprite->SetMaterial(LifeSlimeMaterial);
        sprite->SetEffectAmplitude(alive ? 0.10f : 0);
        sprite->SetEffectPhase(index * 0.62f);
        sprite->SetPosition({left + (icon + gap) * index, top});
        sprite->Update(); lifeSprites_.push_back(std::move(sprite));
    }
}
void OnlineGamePlayScene::UpdateStage1Tutorial() {
    if (stageFile_ != "stage1.json" || !tutorialText_ || localSlot_ < 0) return;
    auto& player = players_[localSlot_];
    if (!stage1ShapeTutorialShown_ && player.IsShapingSelfDestruct()) {
        tutorialText_->SetText(ShapeTutorialMessage);
        tutorialPanelSprite_->SetColor({0.02f, 0.05f, 0.12f, 0.82f});
        tutorialText_->SetColor({1, 1, 1, 1});
        stage1ShapeTutorialShown_ = true;
        return;
    }
    if (nextStage1TutorialIndex_ >= TutorialSteps.size()) return;
    const auto& step = TutorialSteps[nextStage1TutorialIndex_];
    if (player.GetPosition().x < step.triggerX) return;
    if (nextStage1TutorialIndex_ == 7 &&
        !IsOnGasPressurePlate(stage_, player)) return;
    tutorialText_->SetText(step.message);
    tutorialPanelSprite_->SetColor({0.02f, 0.05f, 0.12f, 0.82f});
    tutorialText_->SetColor({1, 1, 1, 1});
    ++nextStage1TutorialIndex_;
}
void OnlineGamePlayScene::StartClearCelebration() {
    if (clearCelebrationActive_) return;
    clearCelebrationActive_ = true;
    clearCelebrationTimer_ = 0;
    SoundManager::GetInstance()->PlaySE(kClearPourSoundName, 0.7f);
    if (selfDestructSlowActive_) {
        TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
        selfDestructSlowActive_ = false;
    }
    auto* sceneManager = SceneManager::GetInstance();
    sceneManager->SetSlimeScreenProgress(0);
    sceneManager->AddPostEffect(PostEffectType::ClearSlimeRise,
                                PostEffectStage::AfterParticle);
    for (int i = 0; i < playerCount_; ++i)
        EffectManager::GetInstance()->PlayEffect("BlueFireworkSparks",
                                                 players_[i].GetPosition());
}
void OnlineGamePlayScene::UpdateClearCelebration(float deltaTime) {
    clearCelebrationTimer_ += deltaTime;
    constexpr float danceDuration = 2.0f, riseDuration = 1.1f;
    const float progress = std::clamp(
        (clearCelebrationTimer_ - danceDuration) / riseDuration, 0.0f, 1.0f);
    SceneManager::GetInstance()->SetSlimeScreenProgress(
        progress * progress * (3.0f - 2.0f * progress));
    if (progress >= 1.0f)
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<ClearScene>(true));
}
void OnlineGamePlayScene::StartDeathTransition() {
    if (isDeathTransitionActive_) return;
    isDeathTransitionActive_ = true;
    deathTransitionTime_ = 0.0f;
    EffectManager::GetInstance()->StopAllEffects();
    if (selfDestructSlowActive_) {
        TimeManager::GetInstance()->SetTimeScale(timeScaleBeforeSelfDestruct_);
        selfDestructSlowActive_ = false;
    }
    UpdateFollowCamera();
    if (camera_) camera_->Update();
    
    // Animate the local player's fluid (or the one who died, but local is fine)
    int targetSlot = (localSlot_ >= 0) ? localSlot_ : 0;
    if (playerFluids_[targetSlot]) {
        auto particles = playerFluids_[targetSlot]->GetParticlesCPU();
        auto settings = playerFluids_[targetSlot]->GetSettings();
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
        playerFluids_[targetSlot]->SetLiquidated(true);
        playerFluids_[targetSlot]->SetDeathEyes(true);
        playerFluids_[targetSlot]->Reset(settings);
        playerFluids_[targetSlot]->SetWallBoundaries(center.x - 40.0f, center.x + 40.0f, -40.0f,
                                        40.0f, center.y - 40.0f, center.y + 40.0f);
        playerFluids_[targetSlot]->SetGrounded(false);
        playerFluids_[targetSlot]->SetEmitter(false, center, {0.0f, 0.0f, 0.0f});
        for (size_t i = 0; i < particles.size(); ++i) {
            auto &particle = particles[i];
            const float angle = static_cast<float>(i) * 2.39996323f;
            const float spread = 1.5f + static_cast<float>(i % 17) * 0.24f;
            particle.velocity = {std::cos(angle) * spread,
                                 std::sin(angle) * spread + 1.0f,
                                 i % 3 == 0 ? -18.0f - static_cast<float>(i % 7)
                                            : 2.0f * std::sin(angle)};
            particle.padding = 5.0f;
        }
        playerFluids_[targetSlot]->SetParticlesCPU(particles);
    }
    
    SceneManager *sceneManager = SceneManager::GetInstance();
    const Vector3 position = (localSlot_ >= 0) ? players_[localSlot_].GetPosition() : Vector3{0,0,0};
    sceneManager->SetPaintSeed(position.x * 17.31f + position.y * 7.13f);
    sceneManager->SetSlimeScreenProgress(0.0f);
    sceneManager->AddPostEffect(PostEffectType::SlimeScreen,
                                PostEffectStage::AfterParticle);
}
void OnlineGamePlayScene::UpdateDeathTransition(float deltaTime) {
    constexpr float kCoverDuration = 2.1f;
    constexpr float kFlightDuration = 0.55f;
    const float previousTime = deathTransitionTime_;
    deathTransitionTime_ += deltaTime;
    if (previousTime < kFlightDuration && deathTransitionTime_ >= kFlightDuration) {
        SoundManager::GetInstance()->PlaySE(kDeathSplatSoundName, 0.75f);
    }
    int targetSlot = (localSlot_ >= 0) ? localSlot_ : 0;
    if (playerFluids_[targetSlot]) {
        playerFluids_[targetSlot]->Update(deltaTime);
    }
    const float progress = std::clamp(
        (deathTransitionTime_ - kFlightDuration) / kCoverDuration, 0.0f, 1.0f);
    SceneManager::GetInstance()->SetSlimeScreenProgress(progress);
    if (progress >= 1.0f) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<GameOverScene>());
    }
}
void OnlineGamePlayScene::Simulate(const std::array<OnlineProtocol::Input, 3>& inputs) {
    if (cleared_ || failed_ || isDeathTransitionActive_) return;
    TimeManager::SimulationStep step(
        OnlineProtocol::Step,
        OnlineProtocol::Step);
    stage_.Update();
    const auto gimmicks = stage_.GetGimmicks();
    std::vector<AABB> bodies;
    bool goal = false;
    for (int i = 0; i < playerCount_; ++i) {
        auto& player = players_[i];
        if (relayActive_[i]) {
            ++relayTicks_[i];
            if (relayTicks_[i] >= 45) {
                relayActive_[i] = false;
                relayTicks_[i] = 0;
                player.Initialize(&stage_.GetField(), spawn_[i]);
                fluidReset_[i] = true;
            }
            continue;
        }
        // Each player controls only their own shape; the synchronized simulation
        // applies the shared slow-motion scale above while anyone is shaping.
        player.SetInvincible(player.IsShapingSelfDestruct());
        player.Update(gimmicks, inputs[i]);
        goal |= player.ConsumeGoalReached();
        for (auto* gimmick : gimmicks) {
            if (gimmick->IsCheckpoint() && gimmick->TryActivateCheckpoint(player.GetAABB())) {
                for (auto& spawn : spawn_) spawn = gimmick->GetAABB().center;
                EffectManager::GetInstance()->PlayEffect("BlueFireworkSparks",
                                                         gimmick->GetAABB().center);
            }
        }
        AABB body{};
        const bool hardened = player.ConsumeHardenedBody(body);
        if (hardened || player.ConsumeJustDied()) {
            if (hardened) bodies.push_back(body);
            --lives_[i];
            if (lives_[i] <= 0) {
                if (i == localSlot_) {
                    StartDeathTransition();
                } else {
                    // Start death transition for other player?
                    // Currently we just trigger game over for the local player to transition to game over scene.
                    // Or if anyone dies, we can just transition. But for now, we do it if localSlot_ dies or anyone.
                    // In a co-op game, if anyone's lives hit 0, the team loses. Let's start the transition.
                    StartDeathTransition();
                }
                return;
            }
            relayActive_[i] = true;
            relayTicks_[i] = 0;
            relayStart_[i] = FluidCorePosition(player);
            corpsePending_[i] = hardened;
        }
    }
    for (const auto& box : bodies) {
        auto body = std::make_unique<CoopBody>(box);
        body->Initialize({}, "", nullptr); stage_.AddGimmick(std::move(body));
    }
    // Keep all shared platforms for this match: deleting one could invalidate a
    // player's current moving-platform pointer. Lives bound the count to 27.
    if (goal) cleared_ = true;
}
uint64_t OnlineGamePlayScene::StateHash() const {
    uint64_t hash = 14695981039346656037ull;
    const auto add = [&hash](float value) {
        const auto bits = static_cast<uint32_t>(static_cast<int32_t>(std::round(value * 1000)));
        for (int shift = 0; shift < 32; shift += 8) HashByte(hash, static_cast<uint8_t>(bits >> shift));
    };
    for (int i = 0; i < playerCount_; ++i) {
        const auto& p = players_[i].GetPosition(); const auto& v = players_[i].GetVelocity();
        add(p.x); add(p.y); add(v.x); add(v.y); add(static_cast<float>(lives_[i]));
        const auto box = players_[i].GetAABB(); add(box.size.x); add(box.size.y);
        add(static_cast<float>(relayTicks_[i]));
        HashByte(hash, relayActive_[i]);
    }
    for (auto* g : stage_.GetGimmicks()) {
        const auto box = g->GetAABB(); add(box.center.x); add(box.center.y); add(box.size.x); add(box.size.y);
        HashByte(hash, g->IsActive()); HashByte(hash, g->IsSolid());
    }
    HashByte(hash, cleared_);
    return hash;
}
void OnlineGamePlayScene::Update() {
    auto& online = EosMultiplayer::Get();
    const auto dt = TimeManager::GetInstance()->GetUnscaledDeltaTime();
    if (LeaveHovered() && Input::GetInstance()->IsMouseTrigger(0)) {
        SoundManager::GetInstance()->PlaySE(ConfirmSoundName, 0.65f);
        online.Leave();
        SceneManager::GetInstance()->SetNextScene(std::make_unique<ArchiveScene>(true)); return;
    }
    leaveButton_->SetColor(LeaveHovered() ? Vector4{0.3f, 0.4f, 0.35f, 1} : Vector4{0.15f, 0.23f, 0.25f, 1}); leaveButton_->Update();
    if (!failed_ && !cleared_ && !online.Playing()) Fail("ロビーとの接続が終了しました");
    if (loaded_ && !failed_ && !cleared_) {
        for (int i = 0; i < playerCount_; ++i) if (i != localSlot_) silence_[i] += dt;
        handshakeTimer_ += dt; inputTimer_ += dt;
        auto* input = Input::GetInstance();
        localInput_.move = std::clamp(
            (input->IsKeyPressed(DIK_D) || input->IsKeyPressed(DIK_RIGHT) ? 1.0f : 0.0f) -
            (input->IsKeyPressed(DIK_A) || input->IsKeyPressed(DIK_LEFT) ? 1.0f : 0.0f) + input->GetGamepadLeftStickX(), -1.0f, 1.0f);
        localInput_.jump |= input->IsKeyTrigger(DIK_SPACE) || input->IsKeyTrigger(DIK_W) || input->IsKeyTrigger(DIK_UP) || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_A);
        localInput_.shape |= input->IsMouseTrigger(1) || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_B);
        localInput_.pullX = std::clamp(localInput_.pullX + (input->IsMousePressed(0) ? input->GetMouseDeltaX() * 0.006f : 0) + input->GetGamepadRightStickX() * 3.5f * dt, -2.5f, 2.5f);
        localInput_.pullY = std::clamp(localInput_.pullY - (input->IsMousePressed(0) ? input->GetMouseDeltaY() * 0.006f : 0) + input->GetGamepadRightStickY() * 3.5f * dt, -2.5f, 2.5f);
        ProcessPackets();
        if (!failed_ && inputTimer_ >= OnlineProtocol::Step) { inputTimer_ = 0; SendInput(); }
        if (!failed_ && tick_ == 0 && handshakeTimer_ >= 1) {
            handshakeTimer_ = 0;
            auto hello = OnlineProtocol::Message(host_ ? "waiting" : "loaded", match_); hello["map"] = mapHash_;
            if (host_) { for (int i = 1; i < playerCount_; ++i) online.Send(i, OnlineProtocol::Encode(hello)); }
            else online.Send(0, OnlineProtocol::Encode(hello));
        }
        if (!failed_ && host_ && std::all_of(loadedPeers_.begin(), loadedPeers_.begin() + playerCount_, [](bool v) { return v; })) {
            accumulator_ = (std::min)(accumulator_ + dt, OnlineProtocol::Step * 2);
            // A slow client applies its reliable backlog before the host gets
            // more than two seconds ahead; this also bounds EOS send queues.
            const bool clientsCaughtUp = std::all_of(acknowledged_.begin() + 1, acknowledged_.begin() + playerCount_,
                [this](uint64_t acknowledged) { return tick_ - acknowledged < 60; });
            if (accumulator_ >= OnlineProtocol::Step && clientsCaughtUp) {
                accumulator_ -= OnlineProtocol::Step;
                auto frame = OnlineProtocol::Message("frame", match_); frame["tick"] = tick_ + 1;
                frame["inputs"] = nlohmann::json::array();
                for (int i = 0; i < playerCount_; ++i) frame["inputs"].push_back(OnlineProtocol::EncodeInput(pending_[i]));
                Simulate(pending_); ++tick_; frame["state"] = StateHash();
                if (!failed_) {
                    const auto bytes = OnlineProtocol::Encode(frame);
                    for (int i = 1; i < playerCount_; ++i) if (!online.Send(i, bytes)) Fail("ゲーム状態の送信に失敗しました");
                }
                for (int i = 0; i < playerCount_; ++i) OnlineProtocol::ConsumeTransient(pending_[i]);
            }
        }
        if (!failed_ && !cleared_) {
            const float limit = tick_ == 0 ? 60.0f : 15.0f;
            const bool clientTimedOut = std::any_of(silence_.begin() + 1, silence_.begin() + playerCount_,
                [limit](float silence) { return silence > limit; });
            if (host_ ? clientTimedOut : silence_[0] > limit)
                Fail("通信がタイムアウトしました。接続を確認して再参加してください");
        }
    }
    if (loaded_) {
        UpdateLocalSlowMotion();
        if (cleared_ && !clearCelebrationActive_) StartClearCelebration();
        if (clearCelebrationActive_) UpdateClearCelebration(dt);
        if (isDeathTransitionActive_) UpdateDeathTransition(dt);
        background_.Update(); UpdateDeathVisuals(dt);
        UpdatePlayerFluids(TimeManager::GetInstance()->GetDeltaTime());
        if (!isDeathTransitionActive_) {
            UpdateFollowCamera(); camera_->Update();
            if (skyBox_) skyBox_->Update(camera_.get());
        }
        EffectManager::GetInstance()->Update(); skyBox_->Update(camera_.get());
        UpdateLivesDisplay(); UpdateStage1Tutorial();
        tutorialPanelSprite_->Update(); tutorialText_->Update();
        if (tutorialKeyWSprite_) {
            auto* input = Input::GetInstance();
            const Vector4 normal{1, 1, 1, 0.35f}, pressed{1, 1, 1, 1};
            tutorialKeyWSprite_->SetColor(input->IsKeyPressed(DIK_W) ? pressed : normal);
            tutorialKeyASprite_->SetColor(input->IsKeyPressed(DIK_A) ? pressed : normal);
            tutorialKeySSprite_->SetColor(input->IsKeyPressed(DIK_S) ? pressed : normal);
            tutorialKeyDSprite_->SetColor(input->IsKeyPressed(DIK_D) ? pressed : normal);
            tutorialMouseSprite_->SetColor(input->IsMousePressed(0) ? pressed : normal);
            tutorialKeyWSprite_->Update(); tutorialKeyASprite_->Update();
            tutorialKeySSprite_->Update(); tutorialKeyDSprite_->Update();
            tutorialMouseSprite_->Update();
        }
    }
    std::string text;
    if (failed_) text = error_;
    else if (cleared_) text = "STAGE CLEAR!  " + std::to_string(playerCount_) + "人の冒険が完了しました";
    else if (tick_ == 0) text = std::to_string(playerCount_) + "人のステージ読み込み・接続を待っています…";
    else {
        text = "あなたは P" + std::to_string(localSlot_ + 1);
        for (int i = 0; i < playerCount_; ++i)
            text += "    P" + std::to_string(i + 1) + ": " + std::to_string(lives_[i]) + "命";
    }
    hud_->SetText(text); hud_->Update();
}
void OnlineGamePlayScene::Draw2D() {
    SpriteManager::GetInstance()->PreDraw();
    if (loaded_) {
        tutorialPanelSprite_->Draw();
        for (const auto& life : lifeSprites_) life->Draw();
        if (tutorialKeyWSprite_) {
            tutorialKeyWSprite_->Draw(); tutorialKeyASprite_->Draw();
            tutorialKeySSprite_->Draw(); tutorialKeyDSprite_->Draw();
            tutorialMouseSprite_->Draw();
        }
    }
    hudBackground_->Draw(); leaveButton_->Draw();
    TextRenderer::GetInstance()->PreDraw();
    if (loaded_) {
        tutorialText_->Draw(); livesNumberText_->Draw();
        if (controlsText_) controlsText_->Draw();
    }
    hud_->Draw(); leaveText_->Draw();
}
void OnlineGamePlayScene::Draw3D() {
    if (!loaded_) return;
    SkyBoxManager::GetInstance()->PreDraw();
    skyBox_->Draw(DirectXCommon::GetInstance()->GetCommandList());
    Object3dManager::GetInstance()->PreDraw(); background_.Draw(true); stage_.Draw();
}
void OnlineGamePlayScene::DrawParticle() {
    if (loaded_) {
        EffectManager::GetInstance()->PreDraw();
        EffectManager::GetInstance()->Draw();
    }
}
