#include "EosMultiplayer.h"
#include "OnlineProtocol.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>

#ifdef GJ_WITH_EOS
#include <Windows.h>
#include <eos_sdk.h>
#include <eos_connect.h>
#include <eos_lobby.h>
#include <eos_p2p.h>
#endif

namespace {
#ifdef GJ_WITH_EOS
std::filesystem::path FindEosConfig() {
    std::vector<std::filesystem::path> roots;
    std::error_code error;
    roots.push_back(std::filesystem::current_path(error));

    std::wstring executablePath(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    if (length > 0 && length < executablePath.size()) {
        executablePath.resize(length);
        roots.push_back(std::filesystem::path(executablePath).parent_path());
    }

    constexpr std::array<const char*, 2> filenames{"eos.local.json", "eos.json"};
    for (auto root : roots) {
        for (int depth = 0; depth < 8 && !root.empty(); ++depth) {
            for (const auto* filename : filenames) {
                const auto candidate = root / "resources" / "Config" / filename;
                if (std::filesystem::is_regular_file(candidate, error)) return candidate;
            }
            const auto parent = root.parent_path();
            if (parent == root) break;
            root = parent;
        }
    }
    return {};
}
#endif
}

struct EosMultiplayer::Impl {
    bool connected = false, busy = false, playing = false, endingMatch = false;
    std::string status = "オンラインに接続するとロビーを作成・検索できます";
    std::string lobbyId, localId, ownerId, stage, match;
    std::vector<Room> rooms;
    std::vector<Member> members;
    std::vector<Packet> incoming;
#ifdef GJ_WITH_EOS
    EOS_HPlatform platform = nullptr;
    EOS_HConnect connect = nullptr;
    EOS_HLobby lobby = nullptr;
    EOS_HP2P p2p = nullptr;
    EOS_ProductUserId user = nullptr;
    EOS_HLobbySearch search = nullptr;
    std::vector<EOS_HLobbyDetails> results;
    EOS_NotificationId lobbyUpdate = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId memberUpdate = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId memberStatus = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId connectionRequest = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId authExpiration = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId loginStatus = EOS_INVALID_NOTIFICATIONID;
    EOS_P2P_SocketId socket{};
    bool initialized = false;
    bool dirty = false;
    nlohmann::json config;

    static std::string Id(EOS_ProductUserId id) {
        if (!id) return {};
        char buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
        int32_t length = sizeof(buffer);
        return EOS_ProductUserId_ToString(id, buffer, &length) == EOS_EResult::EOS_Success ? buffer : "";
    }
    void Error(const char* operation, EOS_EResult result) {
        busy = false;
        status = std::string(operation) + ": " + EOS_EResult_ToString(result);
    }
    void ClearSearch() {
        for (auto handle : results) EOS_LobbyDetails_Release(handle);
        results.clear(); rooms.clear();
        if (search) EOS_LobbySearch_Release(search);
        search = nullptr;
    }
    void ClearLobby() {
        if (p2p && user) {
            EOS_P2P_CloseConnectionsOptions options{};
            options.ApiVersion = EOS_P2P_CLOSECONNECTIONS_API_LATEST;
            options.LocalUserId = user; options.SocketId = &socket;
            EOS_P2P_CloseConnections(p2p, &options);
        }
        lobbyId.clear(); ownerId.clear(); members.clear(); incoming.clear();
        playing = false; endingMatch = false; stage.clear(); match.clear();
    }
    std::string Attribute(EOS_HLobbyDetails details, const char* key) {
        EOS_LobbyDetails_CopyAttributeByKeyOptions options{};
        options.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST;
        options.AttrKey = key;
        EOS_Lobby_Attribute* value = nullptr;
        std::string result;
        if (EOS_LobbyDetails_CopyAttributeByKey(details, &options, &value) == EOS_EResult::EOS_Success) {
            if (value->Data->ValueType == EOS_EAttributeType::EOS_AT_STRING && value->Data->Value.AsUtf8)
                result = value->Data->Value.AsUtf8;
            EOS_Lobby_Attribute_Release(value);
        }
        return result;
    }
    void Refresh() {
        dirty = false;
        if (lobbyId.empty()) return;
        EOS_Lobby_CopyLobbyDetailsHandleOptions options{};
        options.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
        options.LobbyId = lobbyId.c_str(); options.LocalUserId = user;
        EOS_HLobbyDetails details = nullptr;
        if (EOS_Lobby_CopyLobbyDetailsHandle(lobby, &options, &details) != EOS_EResult::EOS_Success) return;
        EOS_LobbyDetails_GetLobbyOwnerOptions owner{};
        owner.ApiVersion = EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST;
        auto newOwner = Id(EOS_LobbyDetails_GetLobbyOwner(details, &owner));
        if (playing && ownerId != newOwner) {
            ClearLobby(); status = "ホストが切断されました。ロビーに入り直してください";
            EOS_LobbyDetails_Release(details); return;
        }
        ownerId = newOwner;
        EOS_LobbyDetails_GetMemberCountOptions count{};
        count.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;
        std::vector<Member> next;
        for (uint32_t i = 0; i < EOS_LobbyDetails_GetMemberCount(details, &count); ++i) {
            EOS_LobbyDetails_GetMemberByIndexOptions byIndex{};
            byIndex.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST;
            byIndex.MemberIndex = i;
            auto member = EOS_LobbyDetails_GetMemberByIndex(details, &byIndex);
            EOS_LobbyDetails_CopyMemberAttributeByKeyOptions attr{};
            attr.ApiVersion = EOS_LOBBYDETAILS_COPYMEMBERATTRIBUTEBYKEY_API_LATEST;
            attr.TargetUserId = member; attr.AttrKey = "ready";
            EOS_Lobby_Attribute* value = nullptr;
            bool ready = false;
            if (EOS_LobbyDetails_CopyMemberAttributeByKey(details, &attr, &value) == EOS_EResult::EOS_Success) {
                ready = value->Data->ValueType == EOS_EAttributeType::EOS_AT_BOOLEAN && value->Data->Value.AsBool;
                EOS_Lobby_Attribute_Release(value);
            }
            next.push_back({ Id(member), ready });
        }
        std::sort(next.begin(), next.end(), [this](const Member& a, const Member& b) {
            if (a.id == b.id) return false;
            if (a.id == ownerId) return true;
            if (b.id == ownerId) return false;
            return a.id < b.id;
        });
        const bool rosterChanged = playing && (next.size() != members.size() ||
            !std::equal(next.begin(), next.end(), members.begin(),
                [](const Member& a, const Member& b) { return a.id == b.id; }));
        members = std::move(next);
        stage = Attribute(details, "stage");
        match = Attribute(details, "match");
        const bool lobbyPlaying = Attribute(details, "mode") == "playing";
        if (endingMatch && lobbyPlaying) {
            // Keep this client in the stage-select flow while the host's lobby
            // update is propagating. Otherwise ArchiveScene would immediately
            // open the just-finished match again.
            playing = false;
        } else {
            playing = lobbyPlaying;
            if (!lobbyPlaying) endingMatch = false;
        }
        EOS_LobbyDetails_Release(details);
        if (rosterChanged) {
            EosMultiplayer::Get().Leave();
            status = "参加者が切断されました。ロビーに入り直してください";
        }
    }
    bool AddString(EOS_HLobbyModification mod, const char* key, const std::string& text) {
        EOS_Lobby_AttributeData data{};
        data.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
        data.Key = key; data.ValueType = EOS_EAttributeType::EOS_AT_STRING; data.Value.AsUtf8 = text.c_str();
        EOS_LobbyModification_AddAttributeOptions options{};
        options.ApiVersion = EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;
        options.Attribute = &data; options.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
        return EOS_LobbyModification_AddAttribute(mod, &options) == EOS_EResult::EOS_Success;
    }
    EOS_HLobbyModification Modify() {
        EOS_Lobby_UpdateLobbyModificationOptions options{};
        options.ApiVersion = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
        options.LocalUserId = user; options.LobbyId = lobbyId.c_str();
        EOS_HLobbyModification mod = nullptr;
        auto result = EOS_Lobby_UpdateLobbyModification(lobby, &options, &mod);
        if (result != EOS_EResult::EOS_Success) Error("ロビー更新", result);
        return mod;
    }
    void Commit(EOS_HLobbyModification mod) {
        busy = true;
        EOS_Lobby_UpdateLobbyOptions options{};
        options.ApiVersion = EOS_LOBBY_UPDATELOBBY_API_LATEST;
        options.LobbyModificationHandle = mod;
        EOS_Lobby_UpdateLobby(lobby, &options, this, [](const EOS_Lobby_UpdateLobbyCallbackInfo* info) {
            auto& self = *static_cast<Impl*>(info->ClientData);
            self.busy = false;
            if (info->ResultCode != EOS_EResult::EOS_Success) self.Error("ロビー更新", info->ResultCode);
            else { self.dirty = true; self.status = "ロビーを更新しました"; }
        });
        EOS_LobbyModification_Release(mod);
    }
    void Login() {
        EOS_Connect_Credentials credentials{};
        credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
        credentials.Type = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
        EOS_Connect_UserLoginInfo userInfo{};
        userInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
        userInfo.DisplayName = "GJ Player";
        EOS_Connect_LoginOptions options{};
        options.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
        options.Credentials = &credentials; options.UserLoginInfo = &userInfo;
        EOS_Connect_Login(connect, &options, this, [](const EOS_Connect_LoginCallbackInfo* info) {
            auto& self = *static_cast<Impl*>(info->ClientData);
            if (info->ResultCode == EOS_EResult::EOS_Success) self.LoggedIn(info->LocalUserId);
            else if (info->ResultCode == EOS_EResult::EOS_InvalidUser) {
                EOS_Connect_CreateUserOptions options{};
                options.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
                options.ContinuanceToken = info->ContinuanceToken;
                EOS_Connect_CreateUser(self.connect, &options, &self, [](const EOS_Connect_CreateUserCallbackInfo* result) {
                    auto& s = *static_cast<Impl*>(result->ClientData);
                    if (result->ResultCode == EOS_EResult::EOS_Success) s.LoggedIn(result->LocalUserId);
                    else s.Error("ユーザー作成", result->ResultCode);
                });
            } else self.Error("接続", info->ResultCode);
        });
    }
    void LoggedIn(EOS_ProductUserId id) {
        user = id; localId = Id(id); connected = true; busy = false;
        status = "接続済み・ロビーを作成するか参加するロビーを検索してください";
    }
#endif
};

EosMultiplayer::EosMultiplayer() : impl_(std::make_unique<Impl>()) {}
EosMultiplayer::~EosMultiplayer() = default;
EosMultiplayer& EosMultiplayer::Get() { static EosMultiplayer instance; return instance; }
bool EosMultiplayer::Connected() const { return impl_->connected; }
bool EosMultiplayer::Busy() const { return impl_->busy; }
bool EosMultiplayer::InLobby() const { return !impl_->lobbyId.empty(); }
bool EosMultiplayer::IsHost() const { return InLobby() && impl_->localId == impl_->ownerId; }
bool EosMultiplayer::Playing() const { return impl_->playing; }
const std::string& EosMultiplayer::StageFile() const { return impl_->stage; }
const std::string& EosMultiplayer::MatchId() const { return impl_->match; }
const std::string& EosMultiplayer::Status() const { return impl_->status; }
const std::vector<EosMultiplayer::Room>& EosMultiplayer::Rooms() const { return impl_->rooms; }
const std::vector<EosMultiplayer::Member>& EosMultiplayer::Members() const { return impl_->members; }
int EosMultiplayer::LocalSlot() const {
    for (size_t i = 0; i < impl_->members.size(); ++i)
        if (impl_->members[i].id == impl_->localId) return static_cast<int>(i);
    return -1;
}
bool EosMultiplayer::CanStart() const {
    return IsHost() && !Busy() && !Playing() && impl_->members.size() >= MinPlayers &&
        impl_->members.size() <= MaxPlayers &&
        std::all_of(impl_->members.begin(), impl_->members.end(), [](const Member& m) { return m.ready; });
}

void EosMultiplayer::Initialize() {
#ifndef GJ_WITH_EOS
    impl_->status = "EOS SDK 未設定・オンラインの導入手順は docs/EOS_SETUP.md を確認してください";
#endif
}

void EosMultiplayer::Connect() {
    auto& s = *impl_;
    if (s.busy || s.connected) return;
#ifdef GJ_WITH_EOS
    if (!s.platform) {
        try {
            std::ifstream file(FindEosConfig());
            if (!file) { s.status = "EOS 接続設定がありません (eos.local.json / eos.json)"; return; }
            file >> s.config;
            for (auto key : {"productId", "sandboxId", "deploymentId", "clientId", "clientSecret"})
                if (s.config.at(key).get<std::string>().empty()) throw std::runtime_error("Missing config");
        } catch (...) { s.status = "EOS 接続設定を確認してください (eos.local.json / eos.json)"; return; }
        EOS_InitializeOptions init{};
        init.ApiVersion = EOS_INITIALIZE_API_LATEST;
        init.ProductName = "GJ"; init.ProductVersion = "1.0";
        if (!s.initialized) {
            auto result = EOS_Initialize(&init);
            if (result != EOS_EResult::EOS_Success) { s.Error("EOS 初期化", result); return; }
            s.initialized = true;
        }
        EOS_Platform_Options options{};
        options.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
        options.ProductId = s.config["productId"].get_ref<const std::string&>().c_str();
        options.SandboxId = s.config["sandboxId"].get_ref<const std::string&>().c_str();
        options.DeploymentId = s.config["deploymentId"].get_ref<const std::string&>().c_str();
        options.ClientCredentials.ClientId = s.config["clientId"].get_ref<const std::string&>().c_str();
        options.ClientCredentials.ClientSecret = s.config["clientSecret"].get_ref<const std::string&>().c_str();
        options.Flags = EOS_PF_DISABLE_OVERLAY;
        options.TickBudgetInMilliseconds = 2;
        s.platform = EOS_Platform_Create(&options);
        if (!s.platform) { s.status = "EOS Platform 作成に失敗しました。接続設定を確認してください"; return; }
        s.connect = EOS_Platform_GetConnectInterface(s.platform);
        s.lobby = EOS_Platform_GetLobbyInterface(s.platform);
        s.p2p = EOS_Platform_GetP2PInterface(s.platform);
        s.socket.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        strcpy_s(s.socket.SocketName, "GJCoopV1");
        EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions lu{};
        lu.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST;
        s.lobbyUpdate = EOS_Lobby_AddNotifyLobbyUpdateReceived(s.lobby, &lu, &s, [](const EOS_Lobby_LobbyUpdateReceivedCallbackInfo* i) {
            static_cast<Impl*>(i->ClientData)->dirty = true;
        });
        EOS_Lobby_AddNotifyLobbyMemberUpdateReceivedOptions mu{};
        mu.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERUPDATERECEIVED_API_LATEST;
        s.memberUpdate = EOS_Lobby_AddNotifyLobbyMemberUpdateReceived(s.lobby, &mu, &s, [](const EOS_Lobby_LobbyMemberUpdateReceivedCallbackInfo* i) {
            static_cast<Impl*>(i->ClientData)->dirty = true;
        });
        EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions ms{};
        ms.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;
        s.memberStatus = EOS_Lobby_AddNotifyLobbyMemberStatusReceived(s.lobby, &ms, &s, [](const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* i) {
            auto& self = *static_cast<Impl*>(i->ClientData);
            if (self.lobbyId != i->LobbyId) return;
            if (i->TargetUserId == self.user && i->CurrentStatus != EOS_ELobbyMemberStatus::EOS_LMS_JOINED && i->CurrentStatus != EOS_ELobbyMemberStatus::EOS_LMS_PROMOTED) {
                self.ClearLobby(); self.busy = false;
                self.status = "ロビーから退出しました";
            } else self.dirty = true;
        });
        EOS_Connect_AddNotifyAuthExpirationOptions ae{};
        ae.ApiVersion = EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST;
        s.authExpiration = EOS_Connect_AddNotifyAuthExpiration(s.connect, &ae, &s, [](const EOS_Connect_AuthExpirationCallbackInfo* i) {
            static_cast<Impl*>(i->ClientData)->Login();
        });
        EOS_Connect_AddNotifyLoginStatusChangedOptions ls{};
        ls.ApiVersion = EOS_CONNECT_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST;
        s.loginStatus = EOS_Connect_AddNotifyLoginStatusChanged(s.connect, &ls, &s, [](const EOS_Connect_LoginStatusChangedCallbackInfo* i) {
            if (i->CurrentStatus != EOS_ELoginStatus::EOS_LS_LoggedIn) {
                auto& self = *static_cast<Impl*>(i->ClientData);
                self.ClearLobby(); self.connected = false; self.busy = false;
                self.status = "接続が切れました。再接続してください";
            }
        });
    }
    s.busy = true; s.status = "EOS に接続しています…";
    EOS_Connect_CreateDeviceIdOptions options{};
    options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
    options.DeviceModel = "Windows PC";
    EOS_Connect_CreateDeviceId(s.connect, &options, &s, [](const EOS_Connect_CreateDeviceIdCallbackInfo* info) {
        auto& self = *static_cast<Impl*>(info->ClientData);
        if (info->ResultCode == EOS_EResult::EOS_Success || info->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed) self.Login();
        else self.Error("端末 ID 作成", info->ResultCode);
    });
#else
    Initialize();
#endif
}

void EosMultiplayer::Search() {
#ifdef GJ_WITH_EOS
    auto& s = *impl_;
    if (!Connected() || Busy() || InLobby()) return;
    s.ClearSearch();
    EOS_Lobby_CreateLobbySearchOptions create{};
    create.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST; create.MaxResults = 30;
    auto result = EOS_Lobby_CreateLobbySearch(s.lobby, &create, &s.search);
    if (result != EOS_EResult::EOS_Success) { s.Error("検索", result); return; }
    for (auto pair : { std::pair{"game", "gj-coop-v1"}, std::pair{"mode", "waiting"} }) {
        EOS_Lobby_AttributeData data{};
        data.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
        data.Key = pair.first; data.ValueType = EOS_EAttributeType::EOS_AT_STRING; data.Value.AsUtf8 = pair.second;
        EOS_LobbySearch_SetParameterOptions param{};
        param.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
        param.Parameter = &data; param.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;
        result = EOS_LobbySearch_SetParameter(s.search, &param);
        if (result != EOS_EResult::EOS_Success) { s.Error("検索条件", result); return; }
    }
    s.busy = true; s.status = "参加できるロビーを検索しています…";
    EOS_LobbySearch_FindOptions find{};
    find.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST; find.LocalUserId = s.user;
    EOS_LobbySearch_Find(s.search, &find, &s, [](const EOS_LobbySearch_FindCallbackInfo* i) {
        auto& self = *static_cast<Impl*>(i->ClientData);
        self.busy = false;
        if (i->ResultCode != EOS_EResult::EOS_Success) { self.Error("ロビー検索", i->ResultCode); return; }
        EOS_LobbySearch_GetSearchResultCountOptions count{};
        count.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
        for (uint32_t index = 0; index < EOS_LobbySearch_GetSearchResultCount(self.search, &count); ++index) {
            EOS_LobbySearch_CopySearchResultByIndexOptions copy{};
            copy.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST; copy.LobbyIndex = index;
            EOS_HLobbyDetails detail = nullptr;
            if (EOS_LobbySearch_CopySearchResultByIndex(self.search, &copy, &detail) != EOS_EResult::EOS_Success) continue;
            EOS_LobbyDetails_CopyInfoOptions options{};
            options.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;
            EOS_LobbyDetails_Info* info = nullptr;
            if (EOS_LobbyDetails_CopyInfo(detail, &options, &info) == EOS_EResult::EOS_Success) {
                if (info->AvailableSlots > 0 && info->MaxMembers == MaxPlayers) {
                    self.rooms.push_back({ info->LobbyId, static_cast<int>(info->MaxMembers - info->AvailableSlots) });
                    self.results.push_back(detail); detail = nullptr;
                }
                EOS_LobbyDetails_Info_Release(info);
            }
            if (detail) EOS_LobbyDetails_Release(detail);
        }
        self.status = self.rooms.empty() ? "ロビーがありません。「ロビーを作る」で募集できます" : "参加するロビーをクリックしてください";
    });
#endif
}

void EosMultiplayer::Create() {
#ifdef GJ_WITH_EOS
    auto& s = *impl_;
    if (!Connected() || Busy() || InLobby()) return;
    s.busy = true; s.status = "ロビーを作成しています…";
    EOS_Lobby_CreateLobbyOptions options{};
    options.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
    options.LocalUserId = s.user; options.MaxLobbyMembers = MaxPlayers;
    options.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
    options.BucketId = "gj-coop-v1";
    options.bDisableHostMigration = EOS_TRUE;
    options.bAllowInvites = EOS_FALSE;
    EOS_Lobby_CreateLobby(s.lobby, &options, &s, [](const EOS_Lobby_CreateLobbyCallbackInfo* i) {
        auto& self = *static_cast<Impl*>(i->ClientData);
        self.busy = false;
        if (i->ResultCode != EOS_EResult::EOS_Success) { self.Error("ロビー作成", i->ResultCode); return; }
        self.lobbyId = i->LobbyId; self.ownerId = self.localId;
        self.Refresh();
        if (auto mod = self.Modify()) {
            if (self.AddString(mod, "game", "gj-coop-v1") && self.AddString(mod, "mode", "waiting")) self.Commit(mod);
            else { EOS_LobbyModification_Release(mod); self.status = "ロビー公開に失敗しました。退出して作り直してください"; }
        }
    });
#endif
}

void EosMultiplayer::Join(size_t index) {
#ifdef GJ_WITH_EOS
    auto& s = *impl_;
    if (!Connected() || Busy() || InLobby() || index >= s.results.size()) return;
    s.busy = true; s.status = "ロビーに参加しています…";
    EOS_Lobby_JoinLobbyOptions options{};
    options.ApiVersion = EOS_LOBBY_JOINLOBBY_API_LATEST;
    options.LocalUserId = s.user; options.LobbyDetailsHandle = s.results[index];
    EOS_Lobby_JoinLobby(s.lobby, &options, &s, [](const EOS_Lobby_JoinLobbyCallbackInfo* i) {
        auto& self = *static_cast<Impl*>(i->ClientData);
        self.busy = false;
        if (i->ResultCode != EOS_EResult::EOS_Success) { self.Error("参加失敗・一覧を更新してください", i->ResultCode); return; }
        self.lobbyId = i->LobbyId; self.Refresh();
        self.status = "参加しました。準備完了をクリックしてください";
    });
#else
    (void)index;
#endif
}

void EosMultiplayer::Leave() {
#ifdef GJ_WITH_EOS
    auto& s = *impl_;
    if (!InLobby() || Busy()) return;
    s.busy = true;
    EOS_Lobby_LeaveLobbyOptions options{};
    options.ApiVersion = EOS_LOBBY_LEAVELOBBY_API_LATEST;
    options.LobbyId = s.lobbyId.c_str(); options.LocalUserId = s.user;
    EOS_Lobby_LeaveLobby(s.lobby, &options, &s, [](const EOS_Lobby_LeaveLobbyCallbackInfo* i) {
        auto& self = *static_cast<Impl*>(i->ClientData);
        self.busy = false;
        if (i->ResultCode != EOS_EResult::EOS_Success && i->ResultCode != EOS_EResult::EOS_NotFound) {
            self.Error("退出", i->ResultCode); return;
        }
        self.ClearLobby(); self.status = "ロビーから退出しました";
    });
    // Stop gameplay immediately; keep the ID until EOS confirms the leave.
    s.playing = false;
#endif
}

void EosMultiplayer::SetReady(bool ready) {
#ifdef GJ_WITH_EOS
    auto& s = *impl_;
    if (!InLobby() || Busy() || Playing()) return;
    auto mod = s.Modify(); if (!mod) return;
    EOS_Lobby_AttributeData data{};
    data.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    data.Key = "ready"; data.ValueType = EOS_EAttributeType::EOS_AT_BOOLEAN; data.Value.AsBool = ready ? EOS_TRUE : EOS_FALSE;
    EOS_LobbyModification_AddMemberAttributeOptions options{};
    options.ApiVersion = EOS_LOBBYMODIFICATION_ADDMEMBERATTRIBUTE_API_LATEST;
    options.Attribute = &data; options.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
    auto result = EOS_LobbyModification_AddMemberAttribute(mod, &options);
    if (result != EOS_EResult::EOS_Success) { EOS_LobbyModification_Release(mod); s.Error("準備更新", result); return; }
    s.Commit(mod);
#else
    (void)ready;
#endif
}

void EosMultiplayer::SelectStage(const std::string& stageFile) {
#ifdef GJ_WITH_EOS
    if (!IsHost() || Busy() || Playing() || impl_->stage == stageFile) return;
    auto& s = *impl_;
    if (auto mod = s.Modify()) {
        if (s.AddString(mod, "stage", stageFile)) s.Commit(mod);
        else { EOS_LobbyModification_Release(mod); s.status = "ステージ選択を同期できませんでした"; }
    }
#else
    (void)stageFile;
#endif
}
void EosMultiplayer::Start(const std::string& stageFile) {
#ifdef GJ_WITH_EOS
    if (!CanStart()) return;
    auto& s = *impl_;
    auto mod = s.Modify(); if (!mod) return;
    const auto epoch = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    EOS_LobbyModification_SetPermissionLevelOptions permission{};
    permission.ApiVersion = EOS_LOBBYMODIFICATION_SETPERMISSIONLEVEL_API_LATEST;
    permission.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_INVITEONLY;
    if (!s.AddString(mod, "stage", stageFile) || !s.AddString(mod, "match", epoch) ||
        !s.AddString(mod, "mode", "playing") || EOS_LobbyModification_SetPermissionLevel(mod, &permission) != EOS_EResult::EOS_Success) {
        EOS_LobbyModification_Release(mod); s.status = "ゲーム開始の更新に失敗しました"; return;
    }
    s.Commit(mod);
#else
    (void)stageFile;
#endif
}
void EosMultiplayer::EndMatch() {
#ifdef GJ_WITH_EOS
    auto& s = *impl_;
    if (!InLobby() || Busy() || s.endingMatch) return;

    // Every participant clears their own ready flag. The host additionally
    // reopens the same lobby and changes it back to the searchable waiting
    // state. No member leaves, so the group can select and play another stage.
    s.endingMatch = true;
    s.playing = false;
    s.incoming.clear();
    auto mod = s.Modify();
    if (!mod) {
        s.endingMatch = false;
        return;
    }

    EOS_Lobby_AttributeData ready{};
    ready.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
    ready.Key = "ready";
    ready.ValueType = EOS_EAttributeType::EOS_AT_BOOLEAN;
    ready.Value.AsBool = EOS_FALSE;
    EOS_LobbyModification_AddMemberAttributeOptions member{};
    member.ApiVersion = EOS_LOBBYMODIFICATION_ADDMEMBERATTRIBUTE_API_LATEST;
    member.Attribute = &ready;
    member.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;
    bool valid = EOS_LobbyModification_AddMemberAttribute(mod, &member) ==
                 EOS_EResult::EOS_Success;

    if (IsHost()) {
        EOS_LobbyModification_SetPermissionLevelOptions permission{};
        permission.ApiVersion =
            EOS_LOBBYMODIFICATION_SETPERMISSIONLEVEL_API_LATEST;
        permission.PermissionLevel =
            EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
        valid = valid && s.AddString(mod, "mode", "waiting") &&
                EOS_LobbyModification_SetPermissionLevel(mod, &permission) ==
                    EOS_EResult::EOS_Success;
    }

    if (!valid) {
        EOS_LobbyModification_Release(mod);
        s.endingMatch = false;
        s.status = "ロビーの待機状態への復帰に失敗しました";
        return;
    }
    s.Commit(mod);
    s.status = "同じロビーで次のステージを選べます";
#endif
}

void EosMultiplayer::Tick() {
#ifdef GJ_WITH_EOS
    auto& s = *impl_;
    if (!s.platform) return;
    EOS_Platform_Tick(s.platform);
    if (s.dirty) s.Refresh();
    if (!s.user) return;
    if (s.connectionRequest == EOS_INVALID_NOTIFICATIONID) {
        EOS_P2P_AddNotifyPeerConnectionRequestOptions options{};
        options.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
        options.LocalUserId = s.user; options.SocketId = &s.socket;
        s.connectionRequest = EOS_P2P_AddNotifyPeerConnectionRequest(s.p2p, &options, &s, [](const EOS_P2P_OnIncomingConnectionRequestInfo* i) {
            auto& self = *static_cast<Impl*>(i->ClientData);
            if (std::none_of(self.members.begin(), self.members.end(), [i](const Member& m) { return m.id == Impl::Id(i->RemoteUserId); })) return;
            EOS_P2P_AcceptConnectionOptions options{};
            options.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
            options.LocalUserId = self.user; options.RemoteUserId = i->RemoteUserId; options.SocketId = &self.socket;
            EOS_P2P_AcceptConnection(self.p2p, &options);
        });
    }
    // Bound work and memory even if a peer floods the socket.
    for (int n = 0; n < 128; ++n) {
        std::array<uint8_t, 4096> data{};
        EOS_ProductUserId sender = nullptr;
        EOS_P2P_SocketId socket{}; socket.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
        uint8_t channel = 0; uint32_t size = 0;
        EOS_P2P_ReceivePacketOptions options{};
        options.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
        options.LocalUserId = s.user; options.MaxDataSizeBytes = static_cast<uint32_t>(data.size());
        const auto result = EOS_P2P_ReceivePacket(s.p2p, &options, &sender, &socket, &channel, data.data(), &size);
        if (result != EOS_EResult::EOS_Success) break;
        if (channel != 0 || strcmp(socket.SocketName, s.socket.SocketName) != 0 || size > OnlineProtocol::MaxPacket || !Playing()) continue;
        const auto id = Impl::Id(sender);
        for (size_t i = 0; i < s.members.size(); ++i) {
            if (s.members[i].id == id && s.incoming.size() < 256)
                s.incoming.push_back({ static_cast<int>(i), {data.begin(), data.begin() + size} });
        }
    }
#endif
}
bool EosMultiplayer::Send(int member, const std::vector<uint8_t>& bytes) {
#ifdef GJ_WITH_EOS
    auto& s = *impl_;
    if (!Playing() || member < 0 || member >= static_cast<int>(s.members.size()) || member == LocalSlot() || bytes.empty() || bytes.size() > OnlineProtocol::MaxPacket) return false;
    EOS_P2P_SendPacketOptions options{};
    options.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
    options.LocalUserId = s.user;
    options.RemoteUserId = EOS_ProductUserId_FromString(s.members[member].id.c_str());
    options.SocketId = &s.socket; options.Channel = 0;
    options.Data = bytes.data(); options.DataLengthBytes = static_cast<uint32_t>(bytes.size());
    options.bAllowDelayedDelivery = EOS_TRUE;
    options.Reliability = EOS_EPacketReliability::EOS_PR_ReliableOrdered;
    // The destination has already been checked against the lobby roster.
    // Permit the initial send to open that peer connection.
    options.bDisableAutoAcceptConnection = EOS_FALSE;
    return EOS_P2P_SendPacket(s.p2p, &options) == EOS_EResult::EOS_Success;
#else
    (void)member; (void)bytes; return false;
#endif
}
std::vector<EosMultiplayer::Packet> EosMultiplayer::Receive() {
    auto result = std::move(impl_->incoming); impl_->incoming.clear(); return result;
}
void EosMultiplayer::Shutdown() {
#ifdef GJ_WITH_EOS
    auto& s = *impl_;
    if (s.platform) {
        s.ClearLobby(); s.ClearSearch();
        if (s.connectionRequest != EOS_INVALID_NOTIFICATIONID) EOS_P2P_RemoveNotifyPeerConnectionRequest(s.p2p, s.connectionRequest);
        if (s.lobbyUpdate != EOS_INVALID_NOTIFICATIONID) EOS_Lobby_RemoveNotifyLobbyUpdateReceived(s.lobby, s.lobbyUpdate);
        if (s.memberUpdate != EOS_INVALID_NOTIFICATIONID) EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived(s.lobby, s.memberUpdate);
        if (s.memberStatus != EOS_INVALID_NOTIFICATIONID) EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(s.lobby, s.memberStatus);
        if (s.authExpiration != EOS_INVALID_NOTIFICATIONID) EOS_Connect_RemoveNotifyAuthExpiration(s.connect, s.authExpiration);
        if (s.loginStatus != EOS_INVALID_NOTIFICATIONID) EOS_Connect_RemoveNotifyLoginStatusChanged(s.connect, s.loginStatus);
        EOS_Platform_Release(s.platform);
    }
    if (s.initialized) EOS_Shutdown();
#endif
    impl_ = std::make_unique<Impl>();
}
