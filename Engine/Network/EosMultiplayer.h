#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// All methods, including EOS callbacks, run on the game's main thread.
class EosMultiplayer {
public:
    static constexpr int MinPlayers = 1;
    static constexpr int MaxPlayers = 3;
    struct Room { std::string id; std::string name; int members = 0; };
    struct Member { std::string id; bool ready = false; };
    struct Packet { int sender = -1; std::vector<uint8_t> data; };
    static EosMultiplayer& Get();
    void Initialize();
    void Shutdown();
    void Tick();
    void Connect();
    void Search();
    void Create(const std::string& name);
    void Join(size_t index);
    void Leave();
    void SetReady(bool ready);
    void SelectStage(const std::string& stageFile);
    void Start(const std::string& stageFile);
    void EndMatch();
    bool Send(int member, const std::vector<uint8_t>& bytes);
    std::vector<Packet> Receive();
    bool Connected() const;
    bool Busy() const;
    bool InLobby() const;
    bool IsHost() const;
    bool CanStart() const;
    bool Playing() const;
    int LocalSlot() const;
    const std::string& StageFile() const;
    const std::string& MatchId() const;
    const std::string& LobbyName() const;
    const std::string& Status() const;
    const std::vector<Room>& Rooms() const;
    const std::vector<Member>& Members() const;
private:
    EosMultiplayer();
    ~EosMultiplayer();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
