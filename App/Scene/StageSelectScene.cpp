#include "StageSelectScene.h"

#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/Time/TimeManager.h"
#include "GamePlayScene.h"
#include "SceneManager.h"
#include "TestScene.h"
#include "TitleScene.h"
#include "EditorScene.h"
#include <cmath>
#include <numbers>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kBookCoverModel =
    "StageSelectBook/BookCover.obj";
constexpr const char* kPageBlockModel =
    "StageSelectBook/PageBlock.obj";
constexpr const char* kStageCardModel =
    "StageSelectBook/StageCard.obj";

constexpr float kCardOpenDuration = 0.42f;
constexpr float kCardCloseDuration = 0.22f;
constexpr float kPageTurnDuration = 0.62f;
constexpr float kCardRestY = 0.35f;
constexpr float kCardHiddenY = -2.35f;
constexpr uint32_t kTurningPageStripCount = 24;
constexpr float kBookPageWidth = 4.45f;
constexpr float kBookPageHeight = 5.05f;
constexpr float kTurningPageBaseZ = -0.19f;
}

void StageSelectScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt({ 0.75f, 1.65f, -16.5f }, { 0.0f, -0.20f, 0.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    InitializeStageData();
    InitializeBookObjects();
    InitializeTurningPage();
    InitializeInterface();

    state_ = BookSelectState::CardOpening;
    animationTime_ = 0.0f;
    pageTurnProgress_ = 0.0f;
    stageIndexChanged_ = false;
    RefreshStageText();
    UpdateCardTransform(0.0f, 0.0f);
}

void StageSelectScene::Finalize()
{
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void StageSelectScene::InitializeStageData()
{
    StageData gamePlayStage;
    gamePlayStage.name = "STAGE 01  GAME PLAY";
    gamePlayStage.description = "MAP CHIP COLLISION TEST";
    gamePlayStage.opensTestScene = false;
    stages_.push_back(gamePlayStage);

    StageData testStage;
    testStage.name = "STAGE 02  TEST SCENE";
    testStage.description = "ENGINE FEATURE TEST";
    testStage.opensTestScene = true;
    stages_.push_back(testStage);
}

void StageSelectScene::InitializeBookObjects()
{
    Object3dManager* objectManager = Object3dManager::GetInstance();
    ModelManager* modelManager = ModelManager::GetInstance();

    backdrop_ = std::make_unique<Object3d>();
    backdrop_->Initialize(objectManager);
    backdrop_->SetModel(modelManager->CreatePlane(kWhiteTexture));
    backdrop_->SetScale({ 17.0f, 10.0f, 1.0f });
    backdrop_->SetTranslate({ 0.0f, 0.0f, 3.0f });
    backdrop_->SetColor({ 0.018f, 0.035f, 0.065f, 1.0f });
    backdrop_->SetEnableLighting(false);

    bookCover_ = std::make_unique<Object3d>();
    bookCover_->Initialize(objectManager);
    bookCover_->SetModel(modelManager->Load(kBookCoverModel));
    bookCover_->SetScale({ 9.5f, 5.7f, 0.36f });
    bookCover_->SetTranslate({ 0.0f, -0.15f, 0.30f });
    bookCover_->SetColor({ 0.18f, 0.055f, 0.025f, 1.0f });
    bookCover_->SetEnableLighting(false);

    bookSpine_ = std::make_unique<Object3d>();
    bookSpine_->Initialize(objectManager);
    bookSpine_->SetModel(modelManager->Load(kPageBlockModel));
    bookSpine_->SetScale({ 0.24f, 5.45f, 0.62f });
    bookSpine_->SetTranslate({ 0.0f, -0.15f, -0.02f });
    bookSpine_->SetColor({ 0.42f, 0.19f, 0.055f, 1.0f });
    bookSpine_->SetEnableLighting(false);

    leftPageBlock_ = std::make_unique<Object3d>();
    leftPageBlock_->Initialize(objectManager);
    leftPageBlock_->SetModel(modelManager->Load(kPageBlockModel));
    leftPageBlock_->SetScale({ 4.45f, 5.05f, 0.24f });
    leftPageBlock_->SetTranslate({ -2.27f, -0.08f, -0.04f });
    leftPageBlock_->SetColor({ 0.91f, 0.82f, 0.63f, 1.0f });
    leftPageBlock_->SetEnableLighting(false);

    rightPageBlock_ = std::make_unique<Object3d>();
    rightPageBlock_->Initialize(objectManager);
    rightPageBlock_->SetModel(modelManager->Load(kPageBlockModel));
    rightPageBlock_->SetScale({ 4.45f, 5.05f, 0.24f });
    rightPageBlock_->SetTranslate({ 2.27f, -0.08f, -0.04f });
    rightPageBlock_->SetColor({ 0.95f, 0.87f, 0.69f, 1.0f });
    rightPageBlock_->SetEnableLighting(false);

    stageCard_ = std::make_unique<Object3d>();
    stageCard_->Initialize(objectManager);
    stageCard_->SetModel(modelManager->Load(kStageCardModel));
    stageCard_->SetScale({ 3.85f, 2.30f, 0.18f });
    stageCard_->SetTranslate({ 0.0f, kCardHiddenY, -0.70f });
    stageCard_->SetColor({ 0.08f, 0.36f, 0.43f, 1.0f });
    stageCard_->SetEnableLighting(false);

    stageCardShadow_ = std::make_unique<Object3d>();
    stageCardShadow_->Initialize(objectManager);
    stageCardShadow_->SetModel(modelManager->Load(kStageCardModel));
    stageCardShadow_->SetScale({ 4.05f, 2.42f, 0.12f });
    stageCardShadow_->SetTranslate({ 0.12f, kCardHiddenY - 0.12f, -0.48f });
    stageCardShadow_->SetColor({ 0.01f, 0.015f, 0.025f, 0.72f });
    stageCardShadow_->SetEnableLighting(false);

    backdrop_->Update();
    bookCover_->Update();
    bookSpine_->Update();
    leftPageBlock_->Update();
    rightPageBlock_->Update();
    stageCardShadow_->Update();
    stageCard_->Update();
}

void StageSelectScene::InitializeTurningPage()
{
    Object3dManager* objectManager = Object3dManager::GetInstance();
    Model* stripModel = ModelManager::GetInstance()->Load(kPageBlockModel);
    float stripWidth = kBookPageWidth /
        static_cast<float>(kTurningPageStripCount);

    turningPageStrips_.reserve(kTurningPageStripCount);
    for (uint32_t index = 0; index < kTurningPageStripCount; ++index) {
        std::unique_ptr<Object3d> strip = std::make_unique<Object3d>();
        strip->Initialize(objectManager);
        strip->SetModel(stripModel);
        strip->SetScale({ stripWidth * 1.08f, kBookPageHeight, 0.035f });
        strip->SetTranslate({ 0.0f, -8.0f, 2.0f });
        strip->SetColor({ 0.95f, 0.86f, 0.68f, 1.0f });
        strip->SetEnableLighting(false);
        strip->Update();
        turningPageStrips_.push_back(std::move(strip));
    }
}

void StageSelectScene::InitializeInterface()
{
    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("THE STAGE ARCHIVE");
    titleText_->SetPosition({ 640.0f, 68.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(48.0f);
    titleText_->SetColor({ 0.84f, 0.72f, 0.38f, 1.0f });
    titleText_->SetOutlineColor({ 0.02f, 0.03f, 0.06f, 1.0f });
    titleText_->SetOutlineWidth(2.0f);

    stageText_ = std::make_unique<Text>();
    stageText_->Initialize(kDefaultFont);
    stageText_->SetPosition({ 640.0f, 318.0f });
    stageText_->SetAnchorPoint({ 0.5f, 0.5f });
    stageText_->SetFontSize(32.0f);
    stageText_->SetColor({ 0.93f, 0.98f, 1.0f, 0.0f });
    stageText_->SetOutlineColor({ 0.0f, 0.03f, 0.05f, 1.0f });
    stageText_->SetOutlineWidth(2.0f);

    descriptionText_ = std::make_unique<Text>();
    descriptionText_->Initialize(kDefaultFont);
    descriptionText_->SetPosition({ 640.0f, 365.0f });
    descriptionText_->SetAnchorPoint({ 0.5f, 0.5f });
    descriptionText_->SetFontSize(20.0f);
    descriptionText_->SetColor({ 0.65f, 0.92f, 0.95f, 0.0f });

    pageText_ = std::make_unique<Text>();
    pageText_->Initialize(kDefaultFont);
    pageText_->SetPosition({ 640.0f, 435.0f });
    pageText_->SetAnchorPoint({ 0.5f, 0.5f });
    pageText_->SetFontSize(18.0f);
    pageText_->SetColor({ 0.82f, 0.74f, 0.55f, 0.0f });

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText(
        "A / D OR LEFT / RIGHT : TURN PAGE    ENTER : SELECT    BACKSPACE : TITLE");
    instructionText_->SetPosition({ 640.0f, 660.0f });
    instructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    instructionText_->SetFontSize(20.0f);
    instructionText_->SetColor({ 0.72f, 0.80f, 0.88f, 1.0f });
}

void StageSelectScene::Update()
{
    float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    Input* input = Input::GetInstance();

    if (input->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
        return;
    }

    if (input->IsKeyTrigger(DIK_F12)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<EditorScene>());
        return;
    }

    if (state_ == BookSelectState::Idle) {
        if (input->IsKeyTrigger(DIK_RIGHT) || input->IsKeyTrigger(DIK_D)) {
            StartPageTurn(1);
        } else if (input->IsKeyTrigger(DIK_LEFT) || input->IsKeyTrigger(DIK_A)) {
            StartPageTurn(-1);
        } else if (input->IsKeyTrigger(DIK_RETURN) || input->IsKeyTrigger(DIK_SPACE)) {
            ConfirmStage();
            return;
        }
    }

    if (state_ == BookSelectState::CardOpening) {
        UpdateCardOpening(deltaTime);
    } else if (state_ == BookSelectState::Idle) {
        UpdateCardIdle();
    } else if (state_ == BookSelectState::CardClosing) {
        UpdateCardClosing(deltaTime);
    } else if (state_ == BookSelectState::PageTurning) {
        UpdatePageTurning(deltaTime);
    }

    camera_->Update();
    backdrop_->Update();
    bookCover_->Update();
    bookSpine_->Update();
    leftPageBlock_->Update();
    rightPageBlock_->Update();
    stageCardShadow_->Update();
    stageCard_->Update();
    titleText_->Update();
    stageText_->Update();
    descriptionText_->Update();
    pageText_->Update();
    instructionText_->Update();
}

void StageSelectScene::StartPageTurn(int32_t direction)
{
    if (state_ != BookSelectState::Idle) {
        return;
    }

    pageTurnDirection_ = direction;
    animationTime_ = 0.0f;
    pageTurnProgress_ = 0.0f;
    stageIndexChanged_ = false;
    state_ = BookSelectState::CardClosing;
}

void StageSelectScene::UpdateCardOpening(float deltaTime)
{
    animationTime_ += deltaTime;
    float progress = Clamp01(animationTime_ / kCardOpenDuration);
    float easedProgress = EaseOutBack(progress);
    float textAlpha = Clamp01((progress - 0.22f) / 0.48f);
    UpdateCardTransform(easedProgress, textAlpha);

    if (progress >= 1.0f) {
        animationTime_ = 0.0f;
        state_ = BookSelectState::Idle;
    }
}

void StageSelectScene::UpdateCardIdle()
{
    float totalTime = static_cast<float>(TimeManager::GetInstance()->GetTotalTime());
    float floating = std::sin(totalTime * 2.0f) * 0.055f;
    stageCard_->SetTranslate({ 0.0f, kCardRestY + floating, -0.70f });
    stageCard_->SetScale({ 3.85f, 2.30f, 0.18f });
    stageCard_->SetRotate({
        -0.035f,
        std::sin(totalTime * 0.85f) * 0.055f,
        std::sin(totalTime * 1.25f) * 0.012f
    });

    stageCardShadow_->SetTranslate({
        0.14f,
        kCardRestY + floating - 0.14f,
        -0.48f
    });
    stageCardShadow_->SetScale({ 4.05f, 2.42f, 0.12f });
    stageCardShadow_->SetRotate({
        -0.035f,
        std::sin(totalTime * 0.85f) * 0.055f,
        std::sin(totalTime * 1.25f) * 0.012f
    });

    float screenOffset = floating * -38.0f;
    stageText_->SetPosition({ 640.0f, 318.0f + screenOffset });
    descriptionText_->SetPosition({ 640.0f, 365.0f + screenOffset });
    pageText_->SetPosition({ 640.0f, 435.0f + screenOffset });
}

void StageSelectScene::UpdateCardClosing(float deltaTime)
{
    animationTime_ += deltaTime;
    float progress = Clamp01(animationTime_ / kCardCloseDuration);
    float easedProgress = SmoothStep(progress);
    UpdateCardTransform(1.0f - easedProgress, 1.0f - easedProgress);

    if (progress >= 1.0f) {
        animationTime_ = 0.0f;
        pageTurnProgress_ = 0.0f;
        state_ = BookSelectState::PageTurning;
    }
}

void StageSelectScene::UpdatePageTurning(float deltaTime)
{
    animationTime_ += deltaTime;
    pageTurnProgress_ = Clamp01(animationTime_ / kPageTurnDuration);
    UpdateTurningPage();

    if (!stageIndexChanged_ && pageTurnProgress_ >= 0.5f) {
        ChangeStageIndex();
        stageIndexChanged_ = true;
    }

    if (pageTurnProgress_ >= 1.0f) {
        animationTime_ = 0.0f;
        pageTurnProgress_ = 0.0f;
        state_ = BookSelectState::CardOpening;
    }
}

void StageSelectScene::UpdateTurningPage()
{
    float baseAngle = pageTurnProgress_ * std::numbers::pi_v<float>;
    float pageArch = std::sin(baseAngle);
    float stripWidth = kBookPageWidth /
        static_cast<float>(kTurningPageStripCount);
    float edgeX = 0.0f;
    float edgeZ = kTurningPageBaseZ;

    for (uint32_t index = 0; index < kTurningPageStripCount; ++index) {
        float pageRate =
            (static_cast<float>(index) + 0.5f) /
            static_cast<float>(kTurningPageStripCount);
        float curlEnvelope = std::sin(pageRate * std::numbers::pi_v<float>);
        float curlAngle = curlEnvelope * pageArch * 0.58f;
        float localAngle = baseAngle + curlAngle;

        float deltaX =
            static_cast<float>(pageTurnDirection_) *
            stripWidth * std::cos(localAngle);
        float deltaZ = -stripWidth * std::sin(localAngle);
        float centerX = edgeX + deltaX * 0.5f;
        float centerZ = edgeZ + deltaZ * 0.5f;
        float lift = curlEnvelope * pageArch * 0.14f;

        float rotateY = localAngle;
        if (pageTurnDirection_ < 0) {
            rotateY = std::numbers::pi_v<float> - localAngle;
        }

        float flutter =
            std::sin(pageRate * std::numbers::pi_v<float> * 3.0f) *
            pageArch * 0.018f;
        float shade = 0.84f + curlEnvelope * pageArch * 0.12f;

        Object3d* strip = turningPageStrips_[index].get();
        strip->SetTranslate({ centerX, -0.08f + lift, centerZ });
        strip->SetScale({ stripWidth * 1.10f, kBookPageHeight, 0.035f });
        strip->SetRotate({ 0.0f, rotateY, flutter });
        strip->SetColor({ shade, shade * 0.91f, shade * 0.72f, 1.0f });
        strip->Update();

        edgeX += deltaX;
        edgeZ += deltaZ;
    }
}

void StageSelectScene::UpdateCardTransform(float progress, float alpha)
{
    float positionY = kCardHiddenY + (kCardRestY - kCardHiddenY) * progress;
    float scaleFactor = 0.12f + 0.88f * progress;
    float rotateX = (1.0f - Clamp01(progress)) * 0.42f;

    stageCard_->SetTranslate({ 0.0f, positionY, -0.70f });
    stageCard_->SetScale({
        3.85f * scaleFactor,
        2.30f * scaleFactor,
        0.18f
    });
    stageCard_->SetRotate({ rotateX, 0.0f, 0.0f });

    stageCardShadow_->SetTranslate({ 0.14f, positionY - 0.14f, -0.48f });
    stageCardShadow_->SetScale({
        4.05f * scaleFactor,
        2.42f * scaleFactor,
        0.12f
    });
    stageCardShadow_->SetRotate({ rotateX, 0.0f, 0.0f });

    float textPositionY = 420.0f - 102.0f * Clamp01(progress);
    stageText_->SetPosition({ 640.0f, textPositionY });
    descriptionText_->SetPosition({ 640.0f, textPositionY + 47.0f });
    pageText_->SetPosition({ 640.0f, textPositionY + 117.0f });
    stageText_->SetColor({ 0.93f, 0.98f, 1.0f, alpha });
    descriptionText_->SetColor({ 0.65f, 0.92f, 0.95f, alpha });
    pageText_->SetColor({ 0.82f, 0.74f, 0.55f, alpha });
}

void StageSelectScene::ChangeStageIndex()
{
    currentStageIndex_ += pageTurnDirection_;
    int32_t stageCount = static_cast<int32_t>(stages_.size());
    if (currentStageIndex_ < 0) {
        currentStageIndex_ = stageCount - 1;
    }
    if (currentStageIndex_ >= stageCount) {
        currentStageIndex_ = 0;
    }
    RefreshStageText();
}

void StageSelectScene::RefreshStageText()
{
    const StageData& stage = stages_[currentStageIndex_];
    stageText_->SetText(stage.name);
    descriptionText_->SetText(stage.description);
    pageText_->SetText(
        "CARD " + std::to_string(currentStageIndex_ + 1) +
        " / " + std::to_string(stages_.size()));
}

void StageSelectScene::ConfirmStage()
{
    state_ = BookSelectState::StageConfirmed;
    const StageData& stage = stages_[currentStageIndex_];
    if (stage.opensTestScene) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TestScene>());
        return;
    }
    SceneManager::GetInstance()->SetNextScene(std::make_unique<GamePlayScene>());
}

float StageSelectScene::Clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

float StageSelectScene::SmoothStep(float value)
{
    float clamped = Clamp01(value);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

float StageSelectScene::EaseOutBack(float value)
{
    float clamped = Clamp01(value);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    float shifted = clamped - 1.0f;
    return 1.0f + c3 * shifted * shifted * shifted + c1 * shifted * shifted;
}

void StageSelectScene::Draw2D()
{
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    stageText_->Draw();
    descriptionText_->Draw();
    pageText_->Draw();
    instructionText_->Draw();
}

void StageSelectScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    backdrop_->Draw();
    bookCover_->Draw();
    bookSpine_->Draw();
    leftPageBlock_->Draw();
    rightPageBlock_->Draw();
    if (state_ == BookSelectState::PageTurning) {
        for (const std::unique_ptr<Object3d>& strip : turningPageStrips_) {
            strip->Draw();
        }
    }
    stageCardShadow_->Draw();
    stageCard_->Draw();
}

void StageSelectScene::DrawParticle()
{
}

void StageSelectScene::DrawImGui()
{
}
