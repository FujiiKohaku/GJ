#pragma once
#include "BaseScene.h"
#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "App/Game/Background/RuinsBackground.h"
#include "Engine/Fluid/SlimeCharacterRenderer.h"
#include "Engine/Camera/Camera.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/2D/Sprite.h"
#include <array>

// Host-sequenced, fixed-step co-op. All peers simulate the same ordered inputs.
// Rendering and camera follow remain local; GPU particles are never networked.
class OnlineGamePlayScene : public BaseScene {
public:
    explicit OnlineGamePlayScene(std::string stageFile);
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override {}
    bool WantsMouseCursor() const override { return true; }
private:
    void Simulate(const std::array<OnlineProtocol::Input, 3>& inputs);
    void Fail(const std::string& reason);
    void ProcessPackets();
    void SendInput();
    uint64_t StateHash() const;
    std::string stageFile_, match_, error_;
    uint64_t mapHash_ = 0, tick_ = 0, inputSequence_ = 0;
    int localSlot_ = 0, playerCount_ = 0;
    bool host_ = false, loaded_ = false, cleared_ = false, failed_ = false;
    float accumulator_ = 0, inputTimer_ = 0, handshakeTimer_ = 0;
    std::array<float, 3> silence_{};
    std::array<bool, 3> loadedPeers_{};
    std::array<uint64_t, 3> sequences_{}, acknowledged_{};
    std::array<OnlineProtocol::Input, 3> pending_{};
    OnlineProtocol::Input localInput_{};
    std::array<MapChipPlayer, 3> players_;
    std::array<Vector3, 3> spawn_{};
    std::array<int, 3> lives_{10, 10, 10};
    MapChipStage stage_;
    RuinsBackground background_;
    std::unique_ptr<Camera> camera_;
    SlimeCharacterRenderer slimes_;
    std::unique_ptr<Text> hud_, leaveText_;
    std::unique_ptr<Sprite> hudBackground_, leaveButton_;
};
