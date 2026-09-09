#pragma once

#include "App/Game/Map/MapChipStage.h"
#include "App/Game/Background/RuinsBackground.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "BaseScene.h"
#ifdef USE_IMGUI
#include "Engine/LevelEditor/MapEditor.h"
#endif
#include "Engine/Fluid/GpuSphFluid.h"
#include "Engine/Fluid/GpuSphFluidRenderer.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/Camera/Camera.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "PageTransition.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "Engine/Audio/SoundManager.h"

struct GamePlaySavePoint {
    Vector3 playerStartPosition;
    StageSnapshot stageSnapshot;
};

class GamePlayScene : public BaseScene {
public:
    explicit GamePlayScene(std::string levelPath = "resources/Maps/stage1.json");
    void Initialize() override;
    void BeginIncrementalInitialize();
    bool InitializeNextStep();
    float GetInitializationProgress() const;
    void InitializeRevealOverlay();
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;
    bool WantsMouseCursor() const override { return true; }

private:
    std::string levelPath_;
    static constexpr int kInitializationStepCount = 7;
    int initializationStep_ = 0;
    LevelData initializationLevelData_;
    void UpdateFollowCamera();
    MapChipStage& GetActiveMapChipStage();
    const MapChipStage& GetActiveMapChipStage() const;
    void StartCannonTravel(const Vector3& cannonPosition);
    void UpdateCannonTravel(float deltaTime);

    /**
     * @brief カメラの注視点（ターゲット）座標が、マップ境界外を映さないように制限（クランプ）する
     * @param targetPosition 本来カメラが追従したい理想の座標
     * @return 画面内にマップ外の未配置領域が映らないように補正された安全な座標
     */
    Vector3 ClampCameraTarget(const Vector3& targetPosition) const;

    void PushSavePoint();
    void PopSavePoint();
    void RestoreSavePoint();

    std::vector<GamePlaySavePoint> savePointHistory_;

    void UpdateLivesText();
    void StopPlayerLoopSounds();
    void LoseLife(bool leaveCorpse = true);
    void StartDeathTransition();
    void UpdateDeathTransition(float deltaTime);
    void RespawnPlayerLeavingCorpse();

    void StartLifeRelay(bool leaveCorpse = true);
    void FinishLifeRelay();
    void ResetToLastRespawnPoint();
    void StartHardResetTransition();
    void UpdateHardResetTransition(float deltaTime);
    void ExecuteHardReset();
    void StartClearCelebration();
    void UpdateClearCelebration(float unscaledDeltaTime);

    void StartStageSelectTransition();
    void UpdateStageSelectTransition(float deltaTime);
    void UpdateFantasyMenuEffect(float deltaTime);
    void UpdateStage1Tutorial();

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<Sprite> tutorialPanelSprite_;
    std::unique_ptr<Text> tutorialText_;
    std::unique_ptr<Text> livesNumberText_;
    std::vector<std::unique_ptr<Sprite>> lifeSprites_;
    int displayedLives_ = -1;
    static constexpr int kInitialLives = 5;
    static constexpr int kStage1Lives = 10;
    int remainingLives_ = kInitialLives;
    int maximumLives_ = kInitialLives;
    MapChipStage mapChipStage_;
    std::unique_ptr<MapChipStage> backMapChipStage_;
    bool hasBackMap_ = false;
    int activeMapIndex_ = 0;
    bool isCannonTravelActive_ = false;
    float cannonTravelTime_ = 0.0f;
    Vector3 cannonTravelStart_ = {0.0f, 0.0f, 0.0f};
    Vector3 cannonTravelEnd_ = {0.0f, 0.0f, 0.0f};
    float cameraLaneDepth_ = 0.0f;
    float cameraTravelStartDepth_ = 0.0f;
    float cameraTravelEndDepth_ = 0.0f;
    float cameraTargetYOffset_ = 0.0f;
    float cameraTravelStartYOffset_ = 0.0f;
    float cameraTravelEndYOffset_ = 0.0f;
    RuinsBackground ruinsBackground_;
    std::unique_ptr<MapChipPlayer> player_;
    std::unique_ptr<GpuSphFluid> gpuSphFluid_;
    std::unique_ptr<GpuSphFluidRenderer> gpuSphFluidRenderer_;
    PageTransition::RevealOverlay pageReveal_;

    // メニュー関連
    bool isMenuOpen_ = false;
    std::unique_ptr<Sprite> menuBackgroundSprite_;
    std::unique_ptr<Sprite> menuPanelSprite_;
    std::unique_ptr<Sprite> menuResumeButtonSprite_;
    std::unique_ptr<Sprite> menuRestartButtonSprite_;
    std::unique_ptr<Sprite> menuStageSelectButtonSprite_;
    std::unique_ptr<Text> menuTitleText_;
    std::unique_ptr<Text> menuResumeText_;
    std::unique_ptr<Text> menuRestartText_;
    std::unique_ptr<Text> menuStageSelectText_;
    std::unique_ptr<Sprite> loopArrowSprite_;
    std::unique_ptr<Sprite> menuTransitionFadeSprite_;
    bool isStageSelectTransitionActive_ = false;
    float stageSelectTransitionTime_ = 0.0f;
    float fantasyMenuEffectStrength_ = 0.0f;
    bool isHardResetTransitionActive_ = false;
    float hardResetTransitionTime_ = 0.0f;
    bool isDeathTransitionActive_ = false;
    float deathTransitionTime_ = 0.0f;
    EffectHandle walkingDustEffectHandle_ = kInvalidEffectHandle;
    float eyeOffsetX_ = 0.0f;
    Vector3 playerStartPosition_ = { 0.0f, 0.0f, 0.0f };
    bool selfDestructSlowActive_ = false;
    float timeScaleBeforeSelfDestruct_ = 1.0f;

    std::size_t nextStage1TutorialIndex_ = 0;
    bool stage1ShapeTutorialShown_ = false;

    std::unique_ptr<Sprite> tutorialKeyWSprite_;
    std::unique_ptr<Sprite> tutorialKeyASprite_;
    std::unique_ptr<Sprite> tutorialKeySSprite_;
    std::unique_ptr<Sprite> tutorialKeyDSprite_;
    std::unique_ptr<Sprite> tutorialMouseLeftSprite_;
    std::unique_ptr<Sprite> tutorialMouseRightSprite_;
    std::unique_ptr<Text> controlsText_;

    bool isLifeRelayActive_ = false;
    float lifeRelayTimer_ = 0.0f;
    float lifeRelayDuration_ = 1.5f;
    Vector3 lifeRelayOrbStartPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 lifeRelayOrbCurrentPosition_ = { 0.0f, 0.0f, 0.0f };
    EffectHandle lifeRelayOrbEffectHandle_ = kInvalidEffectHandle;
    bool isClearCelebrationActive_ = false;

    AudioHandle slowWaterSoundHandle_{};
    AudioHandle slimeMoveSoundHandle_{};
    float clearCelebrationTimer_ = 0.0f;
};
