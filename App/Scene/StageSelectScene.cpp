#include "StageSelectScene.h"

#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/math/MatrixMath.h"
#include "GamePlayScene.h"
#include "SceneManager.h"
#include "TestScene.h"
#include "TitleScene.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace {
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr const char* kPrintedPages = "resources/Models/StageSelectBook/PrintedPages.png";
constexpr const char* kBookLeather = "resources/Models/StageSelectBook/BookLeather.png";
constexpr const char* kStageCardModel =
    "StageSelectBook/StageCard.obj";

constexpr const char* kArchiveRoomModel = "StageSelectBook/ArchiveRoom.obj";
constexpr float kCameraApproachDuration = 2.4f;
const Vector3 kCameraStartEye = { -2.5f, 3.8f, -35.0f };
const Vector3 kCameraEndEye = { 0.75f, 1.65f, -16.5f };
const Vector3 kCameraStartTarget = { 0.0f, -1.0f, 1.0f };
const Vector3 kCameraEndTarget = { 0.0f, -0.20f, 0.0f };

constexpr float kCardOpenDuration = 0.42f;
constexpr float kCardCloseDuration = 0.22f;
constexpr float kPageTurnDuration = 0.62f;
constexpr float kCardRestY = 0.35f;
constexpr float kCardHiddenY = -2.35f;
constexpr uint32_t kTurningPageStripCount = 24;
constexpr uint32_t kOpeningPageCount = 24;
constexpr uint32_t kOpeningPageStripCount = 16;
constexpr uint32_t kPrintPageCount = 8;
constexpr int32_t kPrintSpreadCount = kPrintPageCount / 2;
constexpr float kBookPageWidth = 4.45f;
constexpr float kBookPageHeight = 5.05f;
constexpr float kTurningPageBaseZ = -0.19f;

// Archive-only shader modes: paper=3, leather=4, brass=5.
void SetArchiveMaterial(Object3d* object, int mode, float u = 0.0f, float width = 1.0f)
{
    Material* material = object->GetMaterial();
    material->enableLighting = mode;
    material->enableEnvironmentMap = 0;
    material->environmentCoefficient = 0.0f; // Contact shadow strength in paper mode.
    material->uvTransform = MatrixMath::MakeAffineMatrix(
        { width, 1.0f, 1.0f }, Vector3 { 0.0f, 0.0f, 0.0f }, { u, 0.0f, 0.0f });
}
}

void StageSelectScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt(kCameraStartEye, kCameraStartTarget);
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    InitializeStageData();
    InitializeBookObjects();
    InitializeTurningPage();
    InitializeOpeningPages();
    InitializeInterface();

    state_ = BookSelectState::CameraApproach;
    animationTime_ = 0.0f;
    pageTurnProgress_ = 0.0f;
    stageIndexChanged_ = false;
    RefreshStageText();
    UpdateCardTransform(0.0f, 0.0f);
    UpdateCameraApproach(0.0f);
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
    backdrop_->SetModel(modelManager->Load(kArchiveRoomModel));
    backdrop_->SetEnableLighting(false);
    backdrop_->SetColor({ 0.58f, 0.61f, 0.65f, 1.0f });

    for (auto* cover : { &leftBookCover_, &rightBookCover_ }) {
        *cover = std::make_unique<Object3d>();
        (*cover)->Initialize(objectManager);
        (*cover)->SetModel(modelManager->CreateCube(kBookLeather));
        SetArchiveMaterial(cover->get(), 4);
    }

    for (uint32_t index = 0; index < 8; ++index) {
        auto fitting = std::make_unique<Object3d>();
        fitting->Initialize(objectManager);
        fitting->SetModel(modelManager->CreateCube("resources/Textures/white.png"));
        fitting->SetColor({ 0.65f, 0.40f, 0.12f, 1.0f });
        SetArchiveMaterial(fitting.get(), 5);
        bookFittings_.push_back(std::move(fitting));
    }

    bookSpine_ = std::make_unique<Object3d>();
    bookSpine_->Initialize(objectManager);
    bookSpine_->SetModel(modelManager->CreateCube(kBookLeather));
    bookSpine_->SetScale({ 0.24f, 5.45f, 0.62f });
    bookSpine_->SetTranslate({ 0.0f, -0.15f, -0.02f });
    SetArchiveMaterial(bookSpine_.get(), 4);

    leftPageBlock_ = std::make_unique<Object3d>();
    leftPageBlock_->Initialize(objectManager);
    SetPrintedPage(leftPageBlock_.get(), 0);
    leftPageBlock_->SetScale({ 4.45f, 5.05f, 0.24f });
    leftPageBlock_->SetTranslate({ -2.27f, -0.08f, -0.04f });
    SetArchiveMaterial(leftPageBlock_.get(), 3);

    rightPageBlock_ = std::make_unique<Object3d>();
    rightPageBlock_->Initialize(objectManager);
    SetPrintedPage(rightPageBlock_.get(), 1);
    rightPageBlock_->SetScale({ 4.45f, 5.05f, 0.24f });
    rightPageBlock_->SetTranslate({ 2.27f, -0.08f, -0.04f });
    SetArchiveMaterial(rightPageBlock_.get(), 3);

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

    UpdateBookOpening(0.0f);
    backdrop_->Update();
    leftBookCover_->Update();
    rightBookCover_->Update();
    bookSpine_->Update();
    leftPageBlock_->Update();
    rightPageBlock_->Update();
    stageCardShadow_->Update();
    stageCard_->Update();
}

void StageSelectScene::InitializeTurningPage()
{
    Object3dManager* objectManager = Object3dManager::GetInstance();
    float stripWidth = kBookPageWidth /
        static_cast<float>(kTurningPageStripCount);

    turningPageStrips_.reserve(kTurningPageStripCount);
    for (uint32_t index = 0; index < kTurningPageStripCount; ++index) {
        std::unique_ptr<Object3d> strip = std::make_unique<Object3d>();
        strip->Initialize(objectManager);
        strip->SetModel(ModelManager::GetInstance()->CreateBookLeaf(
            kPrintedPages, 1, 2, index, kTurningPageStripCount));
        strip->SetScale({ stripWidth * 1.08f, kBookPageHeight, 0.035f });
        strip->SetTranslate({ 0.0f, -8.0f, 2.0f });
        SetArchiveMaterial(strip.get(), 3);
        strip->Update();
        turningPageStrips_.push_back(std::move(strip));
    }
}

void StageSelectScene::InitializeOpeningPages()
{
    for (uint32_t page = 0; page < kPrintPageCount; ++page) {
        ModelManager::GetInstance()->CreateBookLeaf(kPrintedPages, page, page);
    }
    openingPageStrips_.reserve(kOpeningPageCount * kOpeningPageStripCount);
    openingPageVisible_.assign(kOpeningPageCount, false);
    for (uint32_t index = 0; index < kOpeningPageCount * kOpeningPageStripCount; ++index) {
        auto strip = std::make_unique<Object3d>();
        strip->Initialize(Object3dManager::GetInstance());
        const uint32_t leaf = index / kOpeningPageStripCount;
        strip->SetModel(ModelManager::GetInstance()->CreateBookLeaf(kPrintedPages,
            (leaf * 2 + 1) % kPrintPageCount, (leaf * 2 + 2) % kPrintPageCount,
            index % kOpeningPageStripCount, kOpeningPageStripCount));
        SetArchiveMaterial(strip.get(), 3);
        openingPageStrips_.push_back(std::move(strip));
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

    if (state_ == BookSelectState::CameraApproach) {
        UpdateCameraApproach(deltaTime);
    } else if (state_ == BookSelectState::CardOpening) {
        UpdateCardOpening(deltaTime);
    } else if (state_ == BookSelectState::Idle) {
        UpdateCardIdle();
    } else if (state_ == BookSelectState::CardClosing) {
        UpdateCardClosing(deltaTime);
    } else if (state_ == BookSelectState::PageTurning) {
        UpdatePageTurning(deltaTime);
    }

    camera_->Update();
    if (state_ == BookSelectState::CameraApproach) {
        for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
            if (!openingPageVisible_[page]) {
                continue;
            }
            for (uint32_t strip = 0; strip < kOpeningPageStripCount; ++strip) {
                openingPageStrips_[page * kOpeningPageStripCount + strip]->Update();
            }
        }
    }
    backdrop_->Update();
    for (const auto& fitting : bookFittings_) {
        fitting->Update();
    }
    leftBookCover_->Update();
    rightBookCover_->Update();
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

void StageSelectScene::UpdateCameraApproach(float deltaTime)
{
    animationTime_ += deltaTime;
    const float progress = Clamp01(animationTime_ / kCameraApproachDuration);
    // Quintic easing starts and ends at rest without a sudden camera stop.
    const float eased = progress * progress * progress *
        (progress * (progress * 6.0f - 15.0f) + 10.0f);
    camera_->LookAt(
        kCameraStartEye + (kCameraEndEye - kCameraStartEye) * eased,
        kCameraStartTarget + (kCameraEndTarget - kCameraStartTarget) * eased);
    // Briefly show the closed cover, then finish opening before the card appears.
    const float coverTime = Clamp01((progress - 0.08f) / 0.88f);
    const float bookProgress = coverTime * coverTime * coverTime *
        (coverTime * (coverTime * 6.0f - 15.0f) + 10.0f);
    UpdateBookOpening(bookProgress);
    UpdateOpeningPages(progress, bookProgress);
    const float titleAlpha = SmoothStep((progress - 0.55f) / 0.45f);
    titleText_->SetColor({ 0.84f, 0.72f, 0.38f, titleAlpha });
    instructionText_->SetColor({ 0.72f, 0.80f, 0.88f, titleAlpha });
    if (progress >= 1.0f) {
        animationTime_ = 0.0f;
        state_ = BookSelectState::CardOpening;
    }
}

void StageSelectScene::UpdateBookOpening(float progress)
{
    const float closed = 1.0f - Clamp01(progress);
    const float foldAngle = closed * std::numbers::pi_v<float> * 0.5f;
    // Separate the two hinges while closed so the covers enclose the page blocks.
    const float hingeOffset = 0.48f * closed;
    const Matrix4x4 bookRotation = MatrixMath::MakeRotateYMatrix(-0.85f * closed);
    const Matrix4x4 bookPlacement = MatrixMath::MakeTranslateMatrix({ 0.0f, -0.15f, 0.30f });
    const Matrix4x4 bookWorld = MatrixMath::Multiply(bookRotation, bookPlacement);

    const auto setWing = [&](Object3d* object, const Vector3& scale,
                             const Vector3& center, float side) {
        const Matrix4x4 local = MatrixMath::MakeAffineMatrix(scale, Vector3 { 0.0f, 0.0f, 0.0f }, center);
        const Matrix4x4 rotation = MatrixMath::MakeRotateYMatrix(-side * foldAngle);
        const Matrix4x4 hinge = MatrixMath::MakeTranslateMatrix({ side * hingeOffset, 0.0f, 0.0f });
        object->SetCustomWorldMatrix(MatrixMath::Multiply(
            MatrixMath::Multiply(MatrixMath::Multiply(local, rotation), hinge), bookWorld));
    };
    setWing(leftBookCover_.get(), { 4.75f, 5.7f, 0.36f }, { -2.375f, 0.0f, 0.0f }, -1.0f);
    setWing(rightBookCover_.get(), { 4.75f, 5.7f, 0.36f }, { 2.375f, 0.0f, 0.0f }, 1.0f);
    for (uint32_t index = 0; index < bookFittings_.size(); ++index) {
        const float side = index < 4 ? -1.0f : 1.0f;
        const float x = (index % 2 == 0 ? 0.25f : 4.5f) * side;
        const float y = index % 4 < 2 ? -2.60f : 2.60f;
        setWing(bookFittings_[index].get(), { 0.34f, 0.32f, 0.055f }, { x, y, -0.20f }, side);
    }
    setWing(leftPageBlock_.get(), { kBookPageWidth, kBookPageHeight, 0.24f },
        { -2.27f, 0.07f, -0.34f }, -1.0f);
    setWing(rightPageBlock_.get(), { kBookPageWidth, kBookPageHeight, 0.24f },
        { 2.27f, 0.07f, -0.34f }, 1.0f);
    const Matrix4x4 spine = MatrixMath::MakeAffineMatrix(
        { 0.24f + 0.72f * closed, 5.45f, 0.62f },
        Vector3 { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -0.32f * progress });
    bookSpine_->SetCustomWorldMatrix(MatrixMath::Multiply(spine, bookWorld));
}

void StageSelectScene::UpdateOpeningPages(float cameraProgress, float bookProgress)
{
    const float closed = 1.0f - bookProgress;
    const Matrix4x4 bookWorld = MatrixMath::Multiply(
        MatrixMath::MakeRotateYMatrix(-0.85f * closed),
        MatrixMath::MakeTranslateMatrix({ 0.0f, -0.15f, 0.30f }));
    const float stripWidth = kBookPageWidth / static_cast<float>(kOpeningPageStripCount);
    uint32_t activeLeaves = 0;
    int32_t latestStarted = -1;
    int32_t latestLanded = -1;
    for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
        const float order = static_cast<float>(page) / static_cast<float>(kOpeningPageCount - 1);
        const float start = 0.24f + (1.65f * order - 0.65f * SmoothStep(order)) * 0.48f;
        const float duration = 0.14f + 0.12f * order * order;
        activeLeaves += cameraProgress > start && cameraProgress < start + duration ? 1u : 0u;
        if (cameraProgress > start) latestStarted = static_cast<int32_t>(page);
        if (cameraProgress >= start + duration) latestLanded = static_cast<int32_t>(page);
    }
    SetPrintedPage(rightPageBlock_.get(), static_cast<uint32_t>(latestStarted * 2 + 3) % kPrintPageCount);
    SetPrintedPage(leftPageBlock_.get(), static_cast<uint32_t>(latestLanded * 2 + 2) % kPrintPageCount);
    const float contact = Clamp01(static_cast<float>(activeLeaves) * 0.065f);
    leftPageBlock_->GetMaterial()->environmentCoefficient = contact;
    rightPageBlock_->GetMaterial()->environmentCoefficient = contact;

    for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
        // A rapid right-to-left riffle: dense in the middle, with a slower last leaf.
        const float order = static_cast<float>(page) / static_cast<float>(kOpeningPageCount - 1);
        const float cadence = 1.65f * order - 0.65f * SmoothStep(order);
        const float start = 0.24f + cadence * 0.48f;
        const float duration = 0.14f + 0.12f * order * order;
        const float phase = Clamp01((cameraProgress - start) / duration);
        openingPageVisible_[page] = phase > 0.0f && phase < 1.0f;
        if (!openingPageVisible_[page]) {
            continue;
        }
        const float turn = SmoothStep(phase / 0.82f);
        const float foldAngle = closed * std::numbers::pi_v<float> * 0.5f;
        const float angle = foldAngle + (std::numbers::pi_v<float> - 2.0f * foldAngle) * turn;
        const float flutterEnvelope = std::sin(phase * std::numbers::pi_v<float>);
        const float landing = Clamp01((phase - 0.78f) / 0.22f);
        const float edgeRebound = std::sin(landing * std::numbers::pi_v<float> * 3.0f) *
            (1.0f - landing) * landing * 0.12f;
        // Start and end inside the moving page blocks, so leaves emerge and settle naturally.
        const float rootX = 0.48f * closed + 0.045f * std::cos(foldAngle) - 0.34f * std::sin(foldAngle);
        float edgeX = rootX * (1.0f - 2.0f * turn);
        float edgeZ = -0.045f * std::sin(foldAngle) - 0.34f * std::cos(foldAngle) -
            flutterEnvelope * (0.13f + order * 0.025f);

        for (uint32_t index = 0; index < kOpeningPageStripCount; ++index) {
            const float rate = (static_cast<float>(index) + 0.5f) /
                static_cast<float>(kOpeningPageStripCount);
            const float curl = std::sin(rate * std::numbers::pi_v<float>) * flutterEnvelope;
            const float ripple = std::sin(phase * std::numbers::pi_v<float> * 8.0f - rate * 5.0f + order);
            // The outer edge trails the spine, then flexes as it lands.
            const float localAngle = (std::clamp)(
                angle - curl * (0.48f + ripple * 0.10f) - rate * rate * std::abs(edgeRebound), foldAngle,
                std::numbers::pi_v<float> - foldAngle);
            const float dx = stripWidth * std::cos(localAngle);
            const float dz = -stripWidth * std::sin(localAngle);
            const Matrix4x4 local = MatrixMath::MakeAffineMatrix(
                { stripWidth * 1.008f, kBookPageHeight, 0.003f },
                Vector3 { 0.0f, -localAngle, 0.0f },
                { edgeX + dx * 0.5f, 0.07f, edgeZ + dz * 0.5f });
            Object3d* strip = openingPageStrips_[page * kOpeningPageStripCount + index].get();
            strip->SetCustomWorldMatrix(MatrixMath::Multiply(local, bookWorld));
            strip->GetMaterial()->environmentCoefficient = contact * flutterEnvelope;
            const float shade = 0.96f - static_cast<float>(page % 4) * 0.012f - curl * 0.10f;
            strip->SetColor({ shade, shade, shade, 1.0f });
            edgeX += dx;
            edgeZ += dz;
        }
    }
}

void StageSelectScene::SetPrintedPage(Object3d* object, uint32_t page)
{
    object->SetModel(ModelManager::GetInstance()->CreateBookLeaf(kPrintedPages, page, page));
}

void StageSelectScene::StartPageTurn(int32_t direction)
{
    if (state_ != BookSelectState::Idle) {
        return;
    }

    pageTurnDirection_ = direction;
    nextPrintSpreadIndex_ = (printSpreadIndex_ + direction + kPrintSpreadCount) % kPrintSpreadCount;
    // Forward: old right -> new left. Backward: old left -> previous right.
    const uint32_t front = static_cast<uint32_t>(direction > 0 ?
        printSpreadIndex_ * 2 + 1 : nextPrintSpreadIndex_ * 2 + 1);
    const uint32_t back = static_cast<uint32_t>(direction > 0 ?
        nextPrintSpreadIndex_ * 2 : printSpreadIndex_ * 2);
    for (uint32_t index = 0; index < kTurningPageStripCount; ++index) {
        turningPageStrips_[index]->SetModel(ModelManager::GetInstance()->CreateBookLeaf(
            kPrintedPages, front, back, index, kTurningPageStripCount));
    }
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
        if (pageTurnDirection_ > 0) {
            SetPrintedPage(rightPageBlock_.get(), nextPrintSpreadIndex_ * 2 + 1);
        } else {
            SetPrintedPage(leftPageBlock_.get(), nextPrintSpreadIndex_ * 2);
        }
        UpdateTurningPage();
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
        printSpreadIndex_ = nextPrintSpreadIndex_;
        SetPrintedPage(leftPageBlock_.get(), printSpreadIndex_ * 2);
        SetPrintedPage(rightPageBlock_.get(), printSpreadIndex_ * 2 + 1);
        leftPageBlock_->GetMaterial()->environmentCoefficient = 0.0f;
        rightPageBlock_->GetMaterial()->environmentCoefficient = 0.0f;
        animationTime_ = 0.0f;
        pageTurnProgress_ = 0.0f;
        state_ = BookSelectState::CardOpening;
    }
}

void StageSelectScene::UpdateTurningPage()
{
    float baseAngle = (pageTurnDirection_ > 0 ? pageTurnProgress_ : 1.0f - pageTurnProgress_) * std::numbers::pi_v<float>;
    float pageArch = std::sin(baseAngle);
    leftPageBlock_->GetMaterial()->environmentCoefficient = pageArch * 0.65f;
    rightPageBlock_->GetMaterial()->environmentCoefficient = pageArch * 0.65f;
    float stripWidth = kBookPageWidth /
        static_cast<float>(kTurningPageStripCount);
    float edgeX = 0.045f * std::cos(baseAngle);
    float edgeZ = kTurningPageBaseZ;

    for (uint32_t index = 0; index < kTurningPageStripCount; ++index) {
        float pageRate =
            (static_cast<float>(index) + 0.5f) /
            static_cast<float>(kTurningPageStripCount);
        float curlEnvelope = std::sin(pageRate * std::numbers::pi_v<float>);
        float curlAngle = curlEnvelope * pageArch * 0.58f;
        float localAngle = baseAngle - static_cast<float>(pageTurnDirection_) * curlAngle;

        float deltaX = stripWidth * std::cos(localAngle);
        float deltaZ = -stripWidth * std::sin(localAngle);
        float centerX = edgeX + deltaX * 0.5f;
        float centerZ = edgeZ + deltaZ * 0.5f;
        float lift = curlEnvelope * pageArch * 0.14f;

        float rotateY = -localAngle;

        float flutter =
            std::sin(pageRate * std::numbers::pi_v<float> * 3.0f) *
            pageArch * 0.018f;
        float shade = 1.0f - curlEnvelope * pageArch * 0.08f;

        Object3d* strip = turningPageStrips_[index].get();
        strip->SetTranslate({ centerX, -0.08f + lift, centerZ });
        strip->SetScale({ stripWidth, kBookPageHeight, 0.006f });
        strip->SetRotate({ 0.0f, rotateY, flutter });
        strip->SetColor({ shade, shade, shade, 1.0f });
        strip->GetMaterial()->environmentCoefficient = pageArch * 0.20f;
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
    leftBookCover_->Draw();
    rightBookCover_->Draw();
    for (const auto& fitting : bookFittings_) {
        fitting->Draw();
    }
    bookSpine_->Draw();
    leftPageBlock_->Draw();
    rightPageBlock_->Draw();
    if (state_ == BookSelectState::CameraApproach) {
        for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
            if (!openingPageVisible_[page]) {
                continue;
            }
            for (uint32_t strip = 0; strip < kOpeningPageStripCount; ++strip) {
                openingPageStrips_[page * kOpeningPageStripCount + strip]->Draw();
            }
        }
    }
    if (state_ == BookSelectState::PageTurning) {
        for (const std::unique_ptr<Object3d>& strip : turningPageStrips_) {
            strip->Draw();
        }
    }
    if (state_ != BookSelectState::CameraApproach) {
        stageCardShadow_->Draw();
        stageCard_->Draw();
    }
}

void StageSelectScene::DrawParticle()
{
}

void StageSelectScene::DrawImGui()
{
}
