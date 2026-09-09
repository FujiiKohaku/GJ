#include "LobbyPanel.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/Input/Input.h"
#include "Engine/Network/EosMultiplayer.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Winapp/WinApp.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr const char* Font = "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr size_t MaxLobbyNameCharacters = 12;
constexpr size_t MaxLobbyNameBytes = 48;
std::unique_ptr<Text> Label(float x, float y, float size) {
    auto text = std::make_unique<Text>(); text->Initialize(Font);
    text->SetPosition({x, y}); text->SetFontSize(size);
    text->SetColor({0.94f, 0.90f, 0.80f, 1}); return text;
}
size_t Utf8Characters(const std::string& text) {
    return static_cast<size_t>(std::count_if(text.begin(), text.end(),
        [](unsigned char c) { return (c & 0xc0) != 0x80; }));
}
void EraseLastUtf8Character(std::string& text) {
    if (text.empty()) return;
    size_t offset = text.size() - 1;
    while (offset > 0 &&
           (static_cast<unsigned char>(text[offset]) & 0xc0) == 0x80)
        --offset;
    text.erase(offset);
}
void AppendUtf8Limited(std::string& destination, const std::string& input) {
    for (size_t offset = 0; offset < input.size();) {
        const unsigned char lead = static_cast<unsigned char>(input[offset]);
        const size_t bytes = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (offset + bytes > input.size() ||
            Utf8Characters(destination) >= MaxLobbyNameCharacters ||
            destination.size() + bytes > MaxLobbyNameBytes) break;
        destination.append(input, offset, bytes);
        offset += bytes;
    }
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
void LobbyPanel::PlaceButton(size_t index, float x, float y, float width, float height) {
    auto& b = buttons_[index];
    b.x = x; b.y = y; b.width = width; b.height = height;
    b.background->SetPosition({x, y}); b.background->SetSize({width, height});
    b.label->SetPosition({x + width / 2, y + height / 2});
}
void LobbyPanel::Initialize() {
    EosMultiplayer::Get().Initialize();
    panel_ = std::make_unique<Sprite>();
    panel_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    panel_->SetPosition({64, 502}); panel_->SetSize({1152, 208});
    panel_->SetColor({0.025f, 0.04f, 0.06f, 0.96f}); panel_->Update();
    title_ = Label(88, 512, 21); title_->SetText("ONLINE CO-OP  /  1～3人で冒険"); title_->Update();
    status_ = Label(88, 542, 14); status_->SetMaxWidth(1100);
    members_ = Label(355, 572, 17);
    nameField_ = std::make_unique<Sprite>();
    nameField_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    nameField_->SetPosition({88, 570}); nameField_->SetSize({242, 34});
    nameText_ = Label(100, 587, 16); nameText_->SetAnchorPoint({0, 0.5f});
    nameCaret_ = std::make_unique<Sprite>();
    nameCaret_->Initialize(SpriteManager::GetInstance(), "resources/Textures/white.png");
    nameCaret_->SetSize({2, 22});
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
    AddButton(24, 654, 260, 42); // open / close online panel
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
    title_->SetText(lobby && !online.LobbyName().empty()
        ? "ONLINE CO-OP  /  " + online.LobbyName()
        : "ONLINE CO-OP  /  1～3人で冒険");
    title_->Update();
    const bool select = canSelectStage && (!lobby || online.IsHost()) && available;
    const int slot = online.LocalSlot();
    const bool ready = slot >= 0 && online.Members()[slot].ready;
    const bool showNameField = expanded_ && !lobby;
    const auto mouse = Input::GetInstance()->GetMousePosition();
    const bool nameHovered = showNameField && mouse.x >= 88 && mouse.x < 330 &&
        mouse.y >= 570 && mouse.y < 604;
    if (Input::GetInstance()->IsMouseTrigger(0)) {
        nameFocused_ = nameHovered;
        if (nameFocused_) {
            caretTimer_ = 0.0f;
            WinApp::GetInstance()->ClearTextInput();
        }
    }
    if (!showNameField) nameFocused_ = false;
    if (nameFocused_) {
        AppendUtf8Limited(lobbyName_, WinApp::GetInstance()->ConsumeTextInput());
        if (Input::GetInstance()->IsKeyTrigger(DIK_BACK))
            EraseLastUtf8Character(lobbyName_);
    } else {
        WinApp::GetInstance()->ClearTextInput();
    }
    caretTimer_ += TimeManager::GetInstance()->GetUnscaledDeltaTime();
    nameField_->SetColor(nameFocused_ ? Vector4{0.26f, 0.31f, 0.32f, 1}
        : nameHovered ? Vector4{0.21f, 0.26f, 0.27f, 1}
                      : Vector4{0.13f, 0.17f, 0.19f, 1});
    nameText_->SetText(lobbyName_.empty() ? "ロビー名を入力" : lobbyName_);
    nameText_->SetColor(lobbyName_.empty() ? Vector4{0.52f, 0.55f, 0.56f, 1}
                                          : Vector4{0.96f, 0.91f, 0.76f, 1});
    nameText_->Update();
    const float caretX = lobbyName_.empty() ? 100.0f
        : 100.0f + (std::min)(nameText_->Measure().x, 205.0f);
    nameCaret_->SetPosition({caretX, 576});
    nameCaret_->SetColor({0.98f, 0.95f, 0.84f,
        nameFocused_ && std::fmod(caretTimer_, 1.0f) < 0.55f ? 1.0f : 0.0f});
    nameField_->Update(); nameCaret_->Update();
    const float stageButtonY = expanded_ ? 452.0f : 580.0f;
    PlaceButton(0, 160, stageButtonY, 210, 38);
    PlaceButton(1, 910, stageButtonY, 210, 38);
    PlaceButton(2, 500, stageButtonY, 280, 38);
    SetButton(0, "← 前のステージ", select);
    SetButton(1, "次のステージ →", select);
    SetButton(2, lobby ? "参加中：1人から開始可能" : "1人でプレイ", canSelectStage && !lobby && available);
    if (!lobby) {
        PlaceButton(3, 88, 612, 242, 34);
        PlaceButton(4, 88, 654, 242, 34);
    } else {
        PlaceButton(4, 88, 612, 242, 34);
        PlaceButton(5, 88, 654, 242, 34);
    }
    SetButton(3, online.Connected() ? "この名前でロビーを作る" : "オンラインに接続",
              available && !lobby && (!online.Connected() ||
                  lobbyName_.find_first_not_of(" \t") != std::string::npos),
              expanded_ && !lobby);
    SetButton(4, lobby ? (ready ? "準備を取り消す" : "準備完了") : "参加 / 一覧を更新", available && online.Connected(), expanded_);
    SetButton(5, "ロビーから退出", available && lobby, expanded_ && lobby);
    const std::string startLabel = online.IsHost()
        ? (online.Members().size() < EosMultiplayer::MinPlayers
            ? "あと1人参加すると開始できます"
            : std::to_string(online.Members().size()) + "人でこのステージを開始")
        : "ホストの開始を待っています";
    SetButton(6, startLabel, canSelectStage && online.CanStart(), expanded_ && lobby);
    status_->SetText(online.Status()); status_->Update();
    std::string names;
    for (size_t i = 0; i < online.Members().size(); ++i) {
        names += "P" + std::to_string(i + 1) + (static_cast<int>(i) == slot ? "（あなた）" : "          ");
        names += i == 0 ? " HOST  " : "       ";
        names += online.Members()[i].ready ? "準備OK\n" : "準備中\n";
    }
    for (size_t i = online.Members().size(); i < 3; ++i) names += "P" + std::to_string(i + 1) + "  参加待ち…\n";
    members_->SetText(lobby ? names : "名前を入力してロビー作成 → 全員準備完了\n1人からホストが開始できます（最大3人）\n参加にコード入力は不要です"); members_->Update();
    const size_t pageCount = (std::max)(size_t{1}, (online.Rooms().size() + 2) / 3);
    page_ = (std::min)(page_, pageCount - 1);
    for (size_t row = 0; row < 3; ++row) {
        const size_t index = page_ * 3 + row;
        const bool exists = index < online.Rooms().size();
        const auto label = exists ? "参加  /  " + online.Rooms()[index].name + "   " +
            std::to_string(online.Rooms()[index].members) + "/3人" : "参加できるロビーがありません";
        SetButton(7 + row, label, exists && available, expanded_ && !lobby && (exists || row == 0));
    }
    SetButton(10, "←", available && page_ > 0, expanded_ && !lobby && pageCount > 1);
    SetButton(11, "→", available && page_ + 1 < pageCount, expanded_ && !lobby && pageCount > 1);
    if (expanded_) PlaceButton(12, 1050, 510, 142, 32);
    else PlaceButton(12, 24, 654, 260, 42);
    SetButton(12, expanded_ ? "閉じる" : "オンラインでプレイ",
              expanded_ || canSelectStage);
    if (!Input::GetInstance()->IsMouseTrigger(0)) return Action::None;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        const auto& b = buttons_[i];
        if (!b.visible || !b.enabled || mouse.x < b.x || mouse.x >= b.x + b.width || mouse.y < b.y || mouse.y >= b.y + b.height) continue;
        switch (i) {
        case 0: return Action::PreviousStage;
        case 1: return Action::NextStage;
        case 2: return Action::Solo;
        case 3:
            if (online.Connected()) { online.Create(lobbyName_); nameFocused_ = false; }
            else online.Connect();
            break;
        case 4: if (lobby) online.SetReady(!ready); else online.Search(); break;
        case 5: online.Leave(); break;
        case 6: return Action::Start;
        case 10: --page_; break;
        case 11: ++page_; break;
        case 12: expanded_ = !expanded_; break;
        default: online.Join(page_ * 3 + i - 7); break;
        }
        break;
    }
    return Action::None;
}
void LobbyPanel::Draw() {
    SpriteManager::GetInstance()->PreDraw();
    if (expanded_) panel_->Draw();
    if (expanded_ && !EosMultiplayer::Get().InLobby()) {
        nameField_->Draw();
        nameCaret_->Draw();
    }
    for (auto& b : buttons_) if (b.visible) b.background->Draw();
    TextRenderer::GetInstance()->PreDraw();
    if (expanded_) {
        title_->Draw(); status_->Draw(); members_->Draw();
        if (!EosMultiplayer::Get().InLobby()) nameText_->Draw();
    }
    for (auto& b : buttons_) if (b.visible) b.label->Draw();
}
