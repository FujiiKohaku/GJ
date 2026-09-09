#pragma once
#include "BaseScene.h"
#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "App/Game/Background/RuinsBackground.h"
#include "Engine/Fluid/GpuSphFluid.h"
#include "App/Game/Gimmick/HardenedFluidSlimeCorpse.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Audio/SoundManager.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/Camera/Camera.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/2D/Sprite.h"
#include <array>
#include <vector>

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
    void InitializePlayerFluids();
    void UpdatePlayerFluids(float deltaTime);
    void UpdateDeathVisuals(float deltaTime);
    void RebuildFluidRenderList();
    Vector3 ClampCameraTarget(const Vector3& target) const;
    void UpdateFollowCamera();
    void UpdateLocalSlowMotion();
    void UpdateLivesDisplay();
    void UpdateStage1Tutorial();
    void StartClearCelebration();
    void UpdateClearCelebration(float deltaTime);
    uint64_t StateHash() const;
    std::string stageFile_, match_, error_;
    uint64_t mapHash_ = 0, tick_ = 0, inputSequence_ = 0;
    int localSlot_ = 0, playerCount_ = 0;
    bool host_ = false, loaded_ = false, cleared_ = false, failed_ = false;
    bool clearCelebrationActive_ = false;
    float accumulator_ = 0, inputTimer_ = 0, handshakeTimer_ = 0;
    float clearCelebrationTimer_ = 0;
    std::array<float, 3> silence_{};
    std::array<bool, 3> loadedPeers_{};
    std::array<uint64_t, 3> sequences_{}, acknowledged_{};
    std::array<OnlineProtocol::Input, 3> pending_{};
    OnlineProtocol::Input localInput_{};
    std::array<MapChipPlayer, 3> players_;
    std::array<std::unique_ptr<GpuSphFluid>, 3> playerFluids_;
    std::array<float, 3> eyeOffsetX_{};
    std::array<bool, 3> fluidReset_{};
    std::array<bool, 3> relayActive_{};
    std::array<uint32_t, 3> relayTicks_{};
    std::array<Vector3, 3> relayStart_{};
    std::array<Vector3, 3> relayPosition_{};
    std::array<bool, 3> relayVisualActive_{};
    std::array<float, 3> relayVisualTimer_{};
    std::array<EffectHandle, 3> relayEffect_{
        kInvalidEffectHandle, kInvalidEffectHandle, kInvalidEffectHandle};
    std::array<bool, 3> corpsePending_{};
    std::vector<std::unique_ptr<HardenedFluidSlimeCorpse>> visualCorpses_;
    std::array<EffectHandle, 3> walkingDustEffect_{
        kInvalidEffectHandle, kInvalidEffectHandle, kInvalidEffectHandle};
    bool selfDestructSlowActive_ = false;
    float timeScaleBeforeSelfDestruct_ = 1.0f;
    AudioHandle slowWaterSoundHandle_{};
    AudioHandle slimeMoveSoundHandle_{};
    std::array<Vector3, 3> spawn_{};
    std::array<int, 3> lives_{10, 10, 10};
    int maximumLives_ = 10;
    MapChipStage stage_;
    RuinsBackground background_;
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<Text> hud_, leaveText_;
    std::unique_ptr<Sprite> hudBackground_, leaveButton_;
    std::unique_ptr<Sprite> tutorialPanelSprite_;
    std::unique_ptr<Text> tutorialText_;
    std::unique_ptr<Text> livesNumberText_;
    std::vector<std::unique_ptr<Sprite>> lifeSprites_;
    int displayedLives_ = -1;
    std::size_t nextStage1TutorialIndex_ = 0;
    bool stage1ShapeTutorialShown_ = false;
    std::unique_ptr<Sprite> tutorialKeyWSprite_;
    std::unique_ptr<Sprite> tutorialKeyASprite_;
    std::unique_ptr<Sprite> tutorialKeySSprite_;
    std::unique_ptr<Sprite> tutorialKeyDSprite_;
    std::unique_ptr<Sprite> tutorialMouseSprite_;
    std::unique_ptr<Text> controlsText_;
};
