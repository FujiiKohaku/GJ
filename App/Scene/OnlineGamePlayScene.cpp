#include "OnlineGamePlayScene.h"
#include "ArchiveScene.h"
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
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>

namespace {
constexpr const char* Font = "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr std::array<Vector4, 3> Colors{{{0.25f, 0.85f, 0.55f, 1}, {0.35f, 0.65f, 1, 1}, {1, 0.6f, 0.3f, 1}}};
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
    return p.x >= 1010 && p.x < 1250 && p.y >= 20 && p.y < 62;
}
// Co-op uses the confirmed collision shape as a shared platform, independent
// of each GPU's fluid simulation. Every peer builds this same geometry.
class CoopBody : public BaseMapChipGimmick {
public:
    explicit CoopBody(AABB box) : box_(box) {}
    bool Initialize(const Vector3&, const std::string&, const BaseGimmickParam*) override {
        object_ = std::make_unique<Object3d>();
        object_->Initialize(Object3dManager::GetInstance());
        object_->SetModel(ModelManager::GetInstance()->CreateCube("resources/Textures/white.png"));
        object_->SetTranslate(box_.center); object_->SetScale(box_.size);
        object_->SetColor({0.25f, 0.46f, 0.40f, 1}); object_->EnableToonLighting(); object_->Update();
        return true;
    }
    void Update() override { object_->Update(); }
    void Draw() override { object_->Draw(); }
    AABB GetAABB() const override { return box_; }
    bool IsHardenedSlime() const override { return true; }
private:
    AABB box_;
    std::unique_ptr<Object3d> object_;
};
}

OnlineGamePlayScene::OnlineGamePlayScene(std::string stageFile) : stageFile_(std::move(stageFile)) {}
void OnlineGamePlayScene::Initialize() {
    auto& online = EosMultiplayer::Get();
    host_ = online.IsHost(); localSlot_ = online.LocalSlot(); playerCount_ = static_cast<int>(online.Members().size()); match_ = online.MatchId();
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
    TimeManager::GetInstance()->SetTimeScale(1);
    camera_ = std::make_unique<Camera>(); camera_->Initialize();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    EffectManager::GetInstance()->SetCamera(camera_.get());
    hud_ = std::make_unique<Text>(); hud_->Initialize(Font); hud_->SetFontSize(20); hud_->SetPosition({28, 24}); hud_->SetMaxWidth(940);
    leaveText_ = std::make_unique<Text>(); leaveText_->Initialize(Font); leaveText_->SetFontSize(18);
    leaveText_->SetPosition({1130, 41}); leaveText_->SetAnchorPoint({0.5f, 0.5f}); leaveText_->SetText("ステージセレクトへ戻る"); leaveText_->Update();
    hudBackground_ = std::make_unique<Sprite>(); hudBackground_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    hudBackground_->SetPosition({12, 12}); hudBackground_->SetSize({1256, 108}); hudBackground_->SetColor({0.025f, 0.04f, 0.06f, 0.92f}); hudBackground_->Update();
    leaveButton_ = std::make_unique<Sprite>(); leaveButton_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    leaveButton_->SetPosition({1010, 20}); leaveButton_->SetSize({240, 42});
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
        slimes_.Initialize(camera_.get());
        loaded_ = true; loadedPeers_[localSlot_] = true;
    } catch (...) { Fail("ステージを読み込めません。同じゲームデータを各PCに配置してください"); }
}
void OnlineGamePlayScene::Finalize() {
    if (loaded_) slimes_.Finalize();
    EffectManager::GetInstance()->StopAllEffects();
    EffectManager::GetInstance()->SetCamera(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    TimeManager::GetInstance()->SetTimeScale(1);
}
void OnlineGamePlayScene::Fail(const std::string& reason) {
    if (failed_) return;
    failed_ = true; error_ = reason;
    EosMultiplayer::Get().EndMatch();
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
void OnlineGamePlayScene::Simulate(const std::array<OnlineProtocol::Input, 3>& inputs) {
    if (cleared_ || failed_) return;
    TimeManager::SimulationStep step(OnlineProtocol::Step);
    stage_.Update();
    const auto gimmicks = stage_.GetGimmicks();
    std::vector<AABB> bodies;
    bool goal = false;
    for (int i = 0; i < playerCount_; ++i) {
        auto& player = players_[i];
        // Shaping is local to each player and does not slow other participants.
        player.SetInvincible(player.IsShapingSelfDestruct());
        player.Update(gimmicks, inputs[i]);
        goal |= player.ConsumeGoalReached();
        for (auto* gimmick : gimmicks) {
            if (gimmick->IsCheckpoint() && gimmick->TryActivateCheckpoint(player.GetAABB()))
                for (auto& spawn : spawn_) spawn = gimmick->GetAABB().center;
        }
        AABB body{};
        const bool hardened = player.ConsumeHardenedBody(body);
        if (hardened || player.ConsumeJustDied()) {
            if (hardened) bodies.push_back(body);
            --lives_[i];
            if (lives_[i] <= 0) { Fail("残機がなくなりました。ロビーを作り直して再挑戦できます"); return; }
            player.Initialize(&stage_.GetField(), spawn_[i]);
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
        online.EndMatch();
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
        const auto p = players_[localSlot_].GetPosition();
        camera_->LookAt({p.x, p.y + 3, -17}, {p.x, p.y + 1, 0}); camera_->Update();
        background_.Update(); slimes_.Update(dt); EffectManager::GetInstance()->Update();
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
    text += "\nA / D : 移動   SPACE : ジャンプ   右クリック : 形を作る / 確定   左ドラッグ : 伸ばす";
    hud_->SetText(text); hud_->Update();
}
void OnlineGamePlayScene::Draw2D() {
    SpriteManager::GetInstance()->PreDraw(); hudBackground_->Draw(); leaveButton_->Draw();
    TextRenderer::GetInstance()->PreDraw(); hud_->Draw(); leaveText_->Draw();
}
void OnlineGamePlayScene::Draw3D() {
    if (!loaded_) return;
    Object3dManager::GetInstance()->PreDraw();
    background_.Draw(stageFile_ == "stage2.json");
    stage_.Draw();
    slimes_.PreDraw();
    for (int i = 0; i < playerCount_; ++i) {
        const auto box = players_[i].GetAABB();
        slimes_.Draw(box.center, players_[i].GetVisualScale(), players_[i].GetForward(), std::abs(players_[i].GetVelocity().x), Colors[i]);
    }
}
void OnlineGamePlayScene::DrawParticle() {
    if (loaded_) {
        EffectManager::GetInstance()->PreDraw();
        EffectManager::GetInstance()->Draw();
    }
}
