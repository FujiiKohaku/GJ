#include "LobbyPanel.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/Input/Input.h"
#include "Engine/Network/EosMultiplayer.h"
#include <algorithm>

namespace {
constexpr const char* Font = "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
std::unique_ptr<Text> Label(float x, float y, float size) {
    auto text = std::make_unique<Text>(); text->Initialize(Font);
    text->SetPosition({x, y}); text->SetFontSize(size);
    text->SetColor({0.94f, 0.90f, 0.80f, 1}); return text;
}
}
void LobbyPanel::AddButton(float x, float y, float width, float height) {
    Button b{x, y, width, height};
    b.background = std::make_unique<Sprite>();
    b.background->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    b.background->SetPosition({x, y}); b.background->SetSize({width, height});
    b.label = Label(x + width / 2, y + height / 2, 16);
    b.label->SetAnchorPoint({0.5f, 0.5f});
    buttons_.push_back(std::move(b));
}
void LobbyPanel::Initialize() {
    EosMultiplayer::Get().Initialize();
    panel_ = std::make_unique<Sprite>();
    panel_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    panel_->SetPosition({64, 502}); panel_->SetSize({1152, 208});
    panel_->SetColor({0.025f, 0.04f, 0.06f, 0.96f}); panel_->Update();
    title_ = Label(88, 512, 21); title_->SetText("ONLINE CO-OP  /  2～3人で冒険"); title_->Update();
    status_ = Label(88, 542, 14); status_->SetMaxWidth(1100);
    members_ = Label(355, 572, 17);
    AddButton(160, 452, 210, 38); // previous stage
    AddButton(910, 452, 210, 38); // next stage
    AddButton(500, 452, 280, 38); // solo
    AddButton(88, 570, 242, 34); // connect / create
    AddButton(88, 612, 242, 34); // search / ready
    AddButton(88, 654, 242, 34); // leave
    AddButton(672, 654, 350, 34); // start
    for (int row = 0; row < 3; ++row) AddButton(672, 570.0f + row * 38, 512, 32);
    AddButton(1034, 686, 70, 20); // previous list page
    AddButton(1114, 686, 70, 20); // next list page
    Update(false);
}
void LobbyPanel::SetButton(size_t index, const std::string& text, bool enabled, bool visible) {
    auto& b = buttons_[index]; b.enabled = enabled; b.visible = visible;
    b.label->SetText(text);
    const auto mouse = Input::GetInstance()->GetMousePosition();
    const bool hover = mouse.x >= b.x && mouse.x < b.x + b.width && mouse.y >= b.y && mouse.y < b.y + b.height;
    b.background->SetColor(!enabled ? Vector4{0.10f, 0.12f, 0.14f, 1} :
        hover ? Vector4{0.30f, 0.38f, 0.35f, 1} : Vector4{0.15f, 0.21f, 0.24f, 1});
    b.label->SetColor(enabled ? Vector4{0.96f, 0.91f, 0.76f, 1} : Vector4{0.44f, 0.46f, 0.48f, 1});
    b.background->Update(); b.label->Update();
}
LobbyPanel::Action LobbyPanel::Update(bool canSelectStage) {
    auto& online = EosMultiplayer::Get();
    const bool lobby = online.InLobby(), available = !online.Busy() && !online.Playing();
    const bool select = canSelectStage && (!lobby || online.IsHost()) && available;
    const int slot = online.LocalSlot();
    const bool ready = slot >= 0 && online.Members()[slot].ready;
    SetButton(0, "← 前のステージ", select);
    SetButton(1, "次のステージ →", select);
    SetButton(2, lobby ? "参加中：2人以上で開始可能" : "1人でプレイ", canSelectStage && !lobby && available);
    SetButton(3, online.Connected() ? "ロビーを作る" : "オンラインに接続", available && !lobby);
    SetButton(4, lobby ? (ready ? "準備を取り消す" : "準備完了") : "参加 / 一覧を更新", available && online.Connected());
    SetButton(5, "ロビーから退出", available && lobby, lobby);
    const std::string startLabel = online.IsHost()
        ? (online.Members().size() < EosMultiplayer::MinPlayers
            ? "あと1人参加すると開始できます"
            : std::to_string(online.Members().size()) + "人でこのステージを開始")
        : "ホストの開始を待っています";
    SetButton(6, startLabel, canSelectStage && online.CanStart(), lobby);
    status_->SetText(online.Status()); status_->Update();
    std::string names;
    for (size_t i = 0; i < online.Members().size(); ++i) {
        names += "P" + std::to_string(i + 1) + (static_cast<int>(i) == slot ? "（あなた）" : "          ");
        names += i == 0 ? " HOST  " : "       ";
        names += online.Members()[i].ready ? "準備OK\n" : "準備中\n";
    }
    for (size_t i = online.Members().size(); i < 3; ++i) names += "P" + std::to_string(i + 1) + "  参加待ち…\n";
    members_->SetText(lobby ? names : "ロビー作成 → 参加 → 全員準備完了\n2人以上でホストが開始できます（最大3人）\n参加にコード入力は不要です"); members_->Update();
    const size_t pageCount = (std::max)(size_t{1}, (online.Rooms().size() + 2) / 3);
    page_ = (std::min)(page_, pageCount - 1);
    for (size_t row = 0; row < 3; ++row) {
        const size_t index = page_ * 3 + row;
        const bool exists = index < online.Rooms().size();
        const auto label = exists ? "参加  /  ROOM " + online.Rooms()[index].id.substr(0, 8) + "   " + std::to_string(online.Rooms()[index].members) + "/3人" : "参加できるロビーがありません";
        SetButton(7 + row, label, exists && available, !lobby && (exists || row == 0));
    }
    SetButton(10, "←", available && page_ > 0, !lobby && pageCount > 1);
    SetButton(11, "→", available && page_ + 1 < pageCount, !lobby && pageCount > 1);
    if (!Input::GetInstance()->IsMouseTrigger(0)) return Action::None;
    const auto mouse = Input::GetInstance()->GetMousePosition();
    for (size_t i = 0; i < buttons_.size(); ++i) {
        const auto& b = buttons_[i];
        if (!b.visible || !b.enabled || mouse.x < b.x || mouse.x >= b.x + b.width || mouse.y < b.y || mouse.y >= b.y + b.height) continue;
        switch (i) {
        case 0: return Action::PreviousStage;
        case 1: return Action::NextStage;
        case 2: return Action::Solo;
        case 3: if (online.Connected()) online.Create(); else online.Connect(); break;
        case 4: if (lobby) online.SetReady(!ready); else online.Search(); break;
        case 5: online.Leave(); break;
        case 6: return Action::Start;
        case 10: --page_; break;
        case 11: ++page_; break;
        default: online.Join(page_ * 3 + i - 7); break;
        }
        break;
    }
    return Action::None;
}
void LobbyPanel::Draw() {
    SpriteManager::GetInstance()->PreDraw(); panel_->Draw();
    for (auto& b : buttons_) if (b.visible) b.background->Draw();
    TextRenderer::GetInstance()->PreDraw(); title_->Draw(); status_->Draw(); members_->Draw();
    for (auto& b : buttons_) if (b.visible) b.label->Draw();
}
