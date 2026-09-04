#pragma once

#include "BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/2D/Sprite.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include "PageTransition.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ArchiveScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    enum class BookSelectState {
        TitleIdle,
        CameraApproach,
        CardOpening,
        Idle,
        CardClosing,
        PageTurning,
        StageConfirmed,
        ReturningToTitle
    };

    enum class PageTurnDirection : int32_t {
        Right = 1,
        Left = -1,
    };

    // 新しい遷移先（例: BossScene）を追加する手順:
    // 1. 下の列挙型へ「Boss」を追加する。
    //      enum class StageDestination { GamePlay, Test, Boss };
    // 2. ArchiveScene.cppの先頭で遷移先シーンをインクルードする。
    //      #include "BossScene.h"
    // 3. UpdateStageConfirmed()内のswitchへ遷移処理を追加する。
    //      case StageDestination::Boss:
    //          SceneManager::GetInstance()->SetNextScene(
    //              std::make_unique<BossScene>());
    //          break;
    // 4. InitializeStageData()で表示内容と遷移先を登録する。
    //      StageData bossStage;
    //      bossStage.name = "STAGE 03  BOSS";
    //      bossStage.description = "BOSS BATTLE";
    //      bossStage.destination = StageDestination::Boss;
    //      stages_.push_back(bossStage);
    // 既存のGamePlayまたはTestへ移動するステージを増やすだけなら、
    // 手順4だけを行い、既存のStageDestinationを設定すればよい。
    enum class StageDestination {
        GamePlay,
        Test,
        GameLab,
    };

    struct StageData {
        std::string name;
        std::string description;
        StageDestination destination = StageDestination::GamePlay;
    };

    struct DustMote {
        std::unique_ptr<Object3d> object;
        Vector3 basePosition;
        float phase = 0.0f;
        float speed = 0.0f;
        float drift = 0.0f;
    };

    void InitializeStageData();
    void LoadPrintedPagePaths();
    void InitializeBookObjects();
    void InitializeTurningPage();
    void InitializeOpeningPages();
    void InitializeInterface();
    void InitializeDustMotes();
    void UpdateDustMotes(float deltaTime);
    bool HandleInput();
    void UpdateCurrentState(float deltaTime);
    void UpdateTitleIdle();
    void UpdateSceneObjects();
    void EnterTitleMode();
    void StartArchiveApproach();
    void StartTitleReturn();
    void UpdateTitleReturn(float deltaTime);
    void UpdateCameraApproach(float deltaTime);
    void UpdateBookOpening(float progress);
    void UpdateOpeningPages(float cameraProgress, float bookProgress);
    void StartPageTurn(PageTurnDirection direction);
    void SetPrintedPage(Object3d* object, uint32_t page);
    const std::string& GetPrintedPagePath(uint32_t page) const;
    uint32_t GetPrintPageCount() const;
    int32_t GetPrintSpreadCount() const;
    void UpdateCardOpening(float deltaTime);
    void UpdateCardIdle();
    void UpdateCardClosing(float deltaTime);
    void UpdatePageTurning(float deltaTime);
    void UpdateTurningPage();
    void UpdateCardTransform(float progress, float alpha);
    void ChangeStageIndex();
    void RefreshStageText();
    void ConfirmStage();
    void UpdateStageConfirmed(float deltaTime);

    static float Clamp01(float value);
    static float SmoothStep(float value);
    static float EaseOutBack(float value);

private:
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> backdrop_;
    std::unique_ptr<Object3d> leftBookCover_;
    std::unique_ptr<Object3d> rightBookCover_;
    std::vector<std::unique_ptr<Object3d>> bookFittings_;
    std::unique_ptr<Object3d> bookSpine_;
    std::unique_ptr<Object3d> leftPageBlock_;
    std::unique_ptr<Object3d> rightPageBlock_;
    std::vector<std::unique_ptr<Object3d>> turningPageStrips_;
    std::vector<std::unique_ptr<Object3d>> openingPageStrips_;
    std::vector<bool> openingPageVisible_;
    std::unique_ptr<Object3d> stageCardShadow_;
    std::unique_ptr<Object3d> stageCard_;
    std::vector<DustMote> dustMotes_;

    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> stageText_;
    std::unique_ptr<Text> descriptionText_;
    std::unique_ptr<Text> pageText_;
    std::unique_ptr<Text> instructionText_;
    std::unique_ptr<Sprite> transitionPage_;
    PageTransition::RevealOverlay pageReveal_;

    std::vector<StageData> stages_;
    std::vector<std::string> printedPagePaths_;
    BookSelectState state_ = BookSelectState::CameraApproach;
    int32_t currentStageIndex_ = 0;
    PageTurnDirection pageTurnDirection_ = PageTurnDirection::Right;
    int32_t printSpreadIndex_ = 0;
    int32_t nextPrintSpreadIndex_ = 0;
    float animationTime_ = 0.0f;
    float pageTurnProgress_ = 0.0f;
    bool stageIndexChanged_ = false;
    bool openingRifflePlayed_ = false;
    bool confirmationPageSoundPlayed_ = false;
    StageDestination confirmedDestination_ = StageDestination::GamePlay;
};
