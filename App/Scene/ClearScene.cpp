#include "ClearScene.h"

#include "ArchiveScene.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/math/MatrixMath.h"
#include "SceneManager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <numbers>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kSkyBoxTexture = "resources/Textures/skybox.dds";
constexpr const char* kArchiveRoomModel = "StageSelectBook/ArchiveRoom.obj";
constexpr const char* kMeadowTreeTrunkModel = "ClearMeadow/MeadowTreeTrunk.obj";
constexpr const char* kMeadowTreeCanopyModel = "ClearMeadow/MeadowTreeCanopy.obj";
constexpr const char* kMeadowMountainModel = "ClearMeadow/MeadowMountain.obj";
constexpr const char* kBookLeather = "resources/Models/StageSelectBook/BookLeather.png";
constexpr const char* kPrintedPage = "resources/Models/StageSelectBook/Pages/page_001.png";
constexpr const char* kPrintedPageDirectory = "resources/Models/StageSelectBook/Pages";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
constexpr uint32_t kOpeningPageCount = 24;
constexpr uint32_t kOpeningPageStripCount = 16;
constexpr float kBookPageWidth = 4.45f;
constexpr float kBookPageHeight = 5.05f;

enum class ArchiveMaterialMode : int32_t { Paper = 3, Leather = 4, Brass = 5 };

void SetArchiveMaterial(Object3d* object, ArchiveMaterialMode mode,
                        float u = 0.0f, float width = 1.0f)
{
    Material* material = object->GetMaterial();
    material->enableLighting = static_cast<int32_t>(mode);
    material->enableEnvironmentMap = 0;
    material->environmentCoefficient = 0.0f;
    material->uvTransform = MatrixMath::MakeAffineMatrix(
        { width, 1.0f, 1.0f }, Vector3 {}, { u, 0.0f, 0.0f });
}

float SmoothStep(float value)
{
    value = (std::clamp)(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}
}

void ClearScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt({ 0.0f, 4.0f, -32.0f }, { 0.0f, -2.0f, 1.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    EffectManager::GetInstance()->SetCamera(camera_.get());

    TextureManager::GetInstance()->LoadTexture(kSkyBoxTexture);
    skyBox_ = std::make_unique<SkyBox>();
    skyBox_->Initialize(DirectXCommon::GetInstance());
    skyBox_->SetTexture(kSkyBoxTexture);
    skyBox_->Update(camera_.get());

    Object3dManager* objects = Object3dManager::GetInstance();
    ModelManager* models = ModelManager::GetInstance();
    archiveRoom_ = std::make_unique<Object3d>();
    archiveRoom_->Initialize(objects);
    archiveRoom_->SetModel(models->Load(kArchiveRoomModel));
    archiveRoom_->SetEnableLighting(false);
    archiveRoom_->SetColor({ 0.58f, 0.61f, 0.65f, 1.0f });
    archiveRoom_->Update();

    Model* whiteCube = models->CreateCube(kWhiteTexture);
    grassGround_ = std::make_unique<Object3d>();
    grassGround_->Initialize(objects);
    grassGround_->SetModel(whiteCube);
    grassGround_->SetScale({ 60.0f, 0.35f, 70.0f });
    grassGround_->SetTranslate({ 0.0f, -7.15f, 18.0f });
    grassGround_->SetColor({ 0.12f, 0.48f, 0.16f, 1.0f });
    grassGround_->SetEnableLighting(false);
    grassGround_->Update();

    Model* treeTrunkModel = models->Load(kMeadowTreeTrunkModel);
    Model* treeCanopyModel = models->Load(kMeadowTreeCanopyModel);
    Model* mountainModel = models->Load(kMeadowMountainModel);
    const Vector3 treePositions[] = {
        { -12.0f, -6.8f, 3.0f }, { 11.0f, -6.8f, 5.0f },
        { -16.0f, -6.8f, 11.0f }, { 15.5f, -6.8f, 13.0f },
        { -10.0f, -6.8f, 19.0f }, { 9.0f, -6.8f, 22.0f },
        { -20.0f, -6.8f, 26.0f }, { 19.0f, -6.8f, 29.0f },
    };
    for (uint32_t index = 0; index < std::size(treePositions); ++index) {
        auto tree = std::make_unique<Object3d>();
        tree->Initialize(objects);
        tree->SetModel(treeTrunkModel);
        const float scale = 0.85f + static_cast<float>(index % 3) * 0.18f;
        tree->SetScale({ scale, scale, scale });
        tree->SetTranslate(treePositions[index]);
        tree->SetRotate({ 0.0f, 0.35f * static_cast<float>(index), 0.0f });
        tree->SetColor({ 0.46f, 0.20f + 0.025f * (index % 2), 0.12f, 1.0f });
        tree->SetEnableLighting(false);
        tree->Update();
        meadowTrees_.push_back(std::move(tree));

        auto canopy = std::make_unique<Object3d>();
        canopy->Initialize(objects);
        canopy->SetModel(treeCanopyModel);
        canopy->SetScale({ scale, scale, scale });
        canopy->SetTranslate(treePositions[index]);
        canopy->SetRotate({ 0.0f, 0.35f * static_cast<float>(index), 0.0f });
        canopy->SetColor({ 0.28f + 0.025f * (index % 2), 0.72f, 0.12f, 1.0f });
        canopy->SetEnableLighting(false);
        canopy->Update();
        meadowTreeCanopies_.push_back(std::move(canopy));
    }
    const Vector3 mountainPositions[] = {
        { -18.0f, -7.0f, 43.0f }, { 0.0f, -7.0f, 49.0f },
        { 19.0f, -7.0f, 44.0f },
    };
    for (uint32_t index = 0; index < std::size(mountainPositions); ++index) {
        auto mountain = std::make_unique<Object3d>();
        mountain->Initialize(objects);
        mountain->SetModel(mountainModel);
        const float scale = 1.7f + static_cast<float>(index) * 0.18f;
        mountain->SetScale({ scale, scale, scale });
        mountain->SetTranslate(mountainPositions[index]);
        mountain->SetColor({ 0.20f, 0.34f + index * 0.025f, 0.19f, 1.0f });
        mountain->SetEnableLighting(false);
        mountain->Update();
        meadowMountains_.push_back(std::move(mountain));
    }

    InitializeArchiveBook();

    flashSprite_ = std::make_unique<Sprite>();
    flashSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    flashSprite_->SetSize({ 1280.0f, 720.0f });
    flashSprite_->SetColor({ 1.0f, 0.94f, 0.70f, 0.0f });

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("STAGE CLEAR");
    titleText_->SetPosition({ 640.0f, 190.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(64.0f);
    titleText_->SetColor({ 1.0f, 0.88f, 0.28f, 0.0f });
    titleText_->SetOutlineColor({ 0.10f, 0.20f, 0.04f, 1.0f });
    titleText_->SetOutlineWidth(3.0f);

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText("ENTER / SPACE : TITLE");
    instructionText_->SetPosition({ 640.0f, 610.0f });
    instructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    instructionText_->SetFontSize(26.0f);
    instructionText_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });

    sceneTime_ = 0.0f;
    fireworkTimer_ = 0.0f;
    fireworkIndex_ = 0;
    meadowRevealed_ = false;
}

void ClearScene::Finalize()
{
    EffectManager::GetInstance()->StopAllEffects();
    EffectManager::GetInstance()->SetCamera(nullptr);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void ClearScene::Update()
{
    const float dt = TimeManager::GetInstance()->GetDeltaTime();
    sceneTime_ += dt;

    const float approach = SmoothStep(sceneTime_ / 2.2f);
    if (!meadowRevealed_) {
        camera_->LookAt(
            { 0.0f, 4.0f - approach * 1.8f, -32.0f + approach * 15.5f },
            { 0.0f, -2.0f + approach * 1.1f, 1.0f });
    } else {
        const float meadowMove = SmoothStep((sceneTime_ - 3.7f) / 1.5f);
        camera_->LookAt(
            { 0.0f, 5.2f, -30.0f + meadowMove * 3.0f },
            { 0.0f, 1.0f + meadowMove * 0.6f, 16.0f });
    }

    UpdateArchiveBook((sceneTime_ - 1.75f) / 1.55f);

    float lightAlpha = 0.0f;
    if (sceneTime_ >= 2.85f && sceneTime_ < 3.75f) {
        lightAlpha = SmoothStep((sceneTime_ - 2.85f) / 0.90f);
    } else if (sceneTime_ >= 3.75f) {
        lightAlpha = 1.0f - SmoothStep((sceneTime_ - 3.75f) / 0.95f);
    }
    if (!meadowRevealed_ && sceneTime_ >= 3.70f) {
        meadowRevealed_ = true;
        fireworkTimer_ = 0.3f;
    }
    flashSprite_->SetColor({ 1.0f, 0.94f, 0.70f, lightAlpha });

    if (meadowRevealed_) {
        fireworkTimer_ += dt;
        if (fireworkTimer_ >= 0.30f && fireworkIndex_ < 18) {
            fireworkTimer_ = 0.0f;
            LaunchFirework();
        }
    }

    if (sceneTime_ >= 5.0f) {
        Input* input = Input::GetInstance();
        if (input->IsKeyTrigger(DIK_RETURN) || input->IsKeyTrigger(DIK_SPACE)) {
            SceneManager::GetInstance()->SetNextScene(std::make_unique<ArchiveScene>());
            return;
        }
    }

    const float titleAlpha = SmoothStep((sceneTime_ - 4.35f) / 0.75f);
    titleText_->SetColor({ 1.0f, 0.88f, 0.28f, titleAlpha });
    instructionText_->SetColor({ 1.0f, 1.0f, 1.0f,
        SmoothStep((sceneTime_ - 5.0f) / 0.65f) });
    camera_->Update();
    skyBox_->Update(camera_.get());
    archiveRoom_->Update();
    grassGround_->Update();
    for (auto& tree : meadowTrees_) tree->Update();
    for (auto& canopy : meadowTreeCanopies_) canopy->Update();
    for (auto& mountain : meadowMountains_) mountain->Update();
    EffectManager::GetInstance()->Update();
    EffectManager::GetInstance()->SetCamera(camera_.get());
    EffectManager::GetInstance()->UpdatePerView();
    flashSprite_->Update();
    titleText_->Update();
    instructionText_->Update();
}

void ClearScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    flashSprite_->Draw();
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    instructionText_->Draw();
}

void ClearScene::Draw3D()
{
    if (meadowRevealed_) {
        SkyBoxManager::GetInstance()->PreDraw();
        skyBox_->Draw(DirectXCommon::GetInstance()->GetCommandList());
    }
    Object3dManager::GetInstance()->PreDraw();
    if (meadowRevealed_) {
        grassGround_->Draw();
        for (const auto& mountain : meadowMountains_) mountain->Draw();
        for (const auto& tree : meadowTrees_) tree->Draw();
        for (const auto& canopy : meadowTreeCanopies_) canopy->Draw();
    } else {
        archiveRoom_->Draw();
        leftBookCover_->Draw();
        rightBookCover_->Draw();
        for (const auto& fitting : bookFittings_) fitting->Draw();
        leftPageBlock_->Draw();
        rightPageBlock_->Draw();
        bookSpine_->Draw();
        for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
            if (!openingPageVisible_[page]) continue;
            for (uint32_t strip = 0; strip < kOpeningPageStripCount; ++strip) {
                openingPageStrips_[page * kOpeningPageStripCount + strip]->Draw();
            }
        }
    }
}

void ClearScene::DrawParticle()
{
    EffectManager::GetInstance()->PreDraw();
    EffectManager::GetInstance()->Draw();
}

void ClearScene::DrawImGui()
{
}

void ClearScene::InitializeArchiveBook()
{
    for (const auto& entry : std::filesystem::directory_iterator(kPrintedPageDirectory)) {
        const std::string extension = entry.path().extension().string();
        if (entry.is_regular_file() && (extension == ".png" || extension == ".PNG")) {
            printedPagePaths_.push_back(entry.path().generic_string());
        }
    }
    std::sort(printedPagePaths_.begin(), printedPagePaths_.end());
    if (printedPagePaths_.empty()) printedPagePaths_.push_back(kWhiteTexture);

    Object3dManager* objects = Object3dManager::GetInstance();
    ModelManager* models = ModelManager::GetInstance();
    const auto create = [objects](Model* model) {
        auto object = std::make_unique<Object3d>();
        object->Initialize(objects);
        object->SetModel(model);
        return object;
    };

    leftBookCover_ = create(models->CreateCube(kBookLeather));
    rightBookCover_ = create(models->CreateCube(kBookLeather));
    SetArchiveMaterial(leftBookCover_.get(), ArchiveMaterialMode::Leather);
    SetArchiveMaterial(rightBookCover_.get(), ArchiveMaterialMode::Leather);

    for (uint32_t index = 0; index < 8; ++index) {
        auto fitting = create(models->CreateCube(kWhiteTexture));
        fitting->SetColor({ 0.65f, 0.40f, 0.12f, 1.0f });
        SetArchiveMaterial(fitting.get(), ArchiveMaterialMode::Brass);
        bookFittings_.push_back(std::move(fitting));
    }

    bookSpine_ = create(models->CreateCube(kBookLeather));
    SetArchiveMaterial(bookSpine_.get(), ArchiveMaterialMode::Leather);
    leftPageBlock_ = create(models->CreateBookLeaf(GetPrintedPagePath(0), GetPrintedPagePath(0)));
    rightPageBlock_ = create(models->CreateBookLeaf(GetPrintedPagePath(1), GetPrintedPagePath(1)));
    SetArchiveMaterial(leftPageBlock_.get(), ArchiveMaterialMode::Paper);
    SetArchiveMaterial(rightPageBlock_.get(), ArchiveMaterialMode::Paper);

    const Matrix4x4 bookWorld = MatrixMath::MakeTranslateMatrix({ 0.0f, -0.15f, 0.30f });
    const auto setWing = [&](Object3d* object, const Vector3& scale, const Vector3& center) {
        object->SetCustomWorldMatrix(MatrixMath::Multiply(
            MatrixMath::MakeAffineMatrix(scale, Vector3 {}, center), bookWorld));
        object->Update();
    };
    setWing(leftBookCover_.get(), { 4.75f, 5.7f, 0.36f }, { -2.375f, 0.0f, 0.0f });
    setWing(rightBookCover_.get(), { 4.75f, 5.7f, 0.36f }, { 2.375f, 0.0f, 0.0f });
    for (uint32_t index = 0; index < bookFittings_.size(); ++index) {
        const float side = index < 4 ? -1.0f : 1.0f;
        float x = (index % 2 == 0 ? 0.25f : 4.5f) * side;
        const float y = index % 4 < 2 ? -2.60f : 2.60f;
        setWing(bookFittings_[index].get(), { 0.34f, 0.32f, 0.055f }, { x, y, -0.20f });
    }
    setWing(leftPageBlock_.get(), { kBookPageWidth, kBookPageHeight, 0.24f },
        { -2.27f, 0.07f, -0.34f });
    setWing(rightPageBlock_.get(), { kBookPageWidth, kBookPageHeight, 0.24f },
        { 2.27f, 0.07f, -0.34f });
    bookSpine_->SetCustomWorldMatrix(MatrixMath::Multiply(
        MatrixMath::MakeAffineMatrix({ 0.24f, 5.45f, 0.62f }, Vector3 {}, { 0.0f, 0.0f, -0.32f }),
        bookWorld));
    bookSpine_->Update();

    openingPageVisible_.assign(kOpeningPageCount, false);
    openingPageStrips_.reserve(kOpeningPageCount * kOpeningPageStripCount);
    for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
        for (uint32_t strip = 0; strip < kOpeningPageStripCount; ++strip) {
            auto object = create(models->CreateBookLeaf(
                GetPrintedPagePath(page * 2 + 1), GetPrintedPagePath(page * 2 + 2),
                strip, kOpeningPageStripCount));
            SetArchiveMaterial(object.get(), ArchiveMaterialMode::Paper);
            object->Update();
            openingPageStrips_.push_back(std::move(object));
        }
    }
}

void ClearScene::UpdateArchiveBook(float riffleProgress)
{
    riffleProgress = (std::clamp)(riffleProgress, 0.0f, 1.0f);
    const Matrix4x4 bookWorld = MatrixMath::MakeTranslateMatrix({ 0.0f, -0.15f, 0.30f });
    const float stripWidth = kBookPageWidth / static_cast<float>(kOpeningPageStripCount);
    uint32_t activeLeaves = 0;
    int32_t latestStarted = -1;
    int32_t latestLanded = -1;
    for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
        const float order = static_cast<float>(page) / static_cast<float>(kOpeningPageCount - 1);
        const float start = 0.04f + (1.65f * order - 0.65f * SmoothStep(order)) * 0.70f;
        const float duration = 0.18f + 0.12f * order * order;
        if (riffleProgress > start && riffleProgress < start + duration) ++activeLeaves;
        if (riffleProgress > start) latestStarted = static_cast<int32_t>(page);
        if (riffleProgress >= start + duration) latestLanded = static_cast<int32_t>(page);
    }
    if (latestStarted >= 0) SetPrintedPage(rightPageBlock_.get(), latestStarted * 2 + 3);
    if (latestLanded >= 0) SetPrintedPage(leftPageBlock_.get(), latestLanded * 2 + 2);
    const float contact = (std::min)(static_cast<float>(activeLeaves) * 0.065f, 1.0f);
    leftPageBlock_->GetMaterial()->environmentCoefficient = contact;
    rightPageBlock_->GetMaterial()->environmentCoefficient = contact;

    for (uint32_t page = 0; page < kOpeningPageCount; ++page) {
        const float order = static_cast<float>(page) / static_cast<float>(kOpeningPageCount - 1);
        const float cadence = 1.65f * order - 0.65f * SmoothStep(order);
        const float start = 0.04f + cadence * 0.70f;
        const float duration = 0.18f + 0.12f * order * order;
        const float phase = (std::clamp)((riffleProgress - start) / duration, 0.0f, 1.0f);
        openingPageVisible_[page] = phase > 0.0f && phase < 1.0f;
        if (!openingPageVisible_[page]) continue;

        const float turn = SmoothStep(phase / 0.82f);
        const float angle = std::numbers::pi_v<float> * turn;
        const float flutterEnvelope = std::sin(phase * std::numbers::pi_v<float>);
        const float landing = (std::clamp)((phase - 0.78f) / 0.22f, 0.0f, 1.0f);
        const float rebound = std::sin(landing * std::numbers::pi_v<float> * 3.0f) *
            (1.0f - landing) * landing * 0.12f;
        float edgeX = 0.045f * (1.0f - 2.0f * turn);
        float edgeZ = -0.34f - flutterEnvelope * (0.13f + order * 0.025f);
        for (uint32_t strip = 0; strip < kOpeningPageStripCount; ++strip) {
            const float rate = (static_cast<float>(strip) + 0.5f) /
                static_cast<float>(kOpeningPageStripCount);
            const float curl = std::sin(rate * std::numbers::pi_v<float>) * flutterEnvelope;
            const float ripple = std::sin(phase * std::numbers::pi_v<float> * 8.0f - rate * 5.0f + order);
            const float localAngle = (std::clamp)(
                angle - curl * (0.48f + ripple * 0.10f) - rate * rate * std::abs(rebound),
                0.0f, std::numbers::pi_v<float>);
            const float dx = stripWidth * std::cos(localAngle);
            const float dz = -stripWidth * std::sin(localAngle);
            Object3d* object = openingPageStrips_[page * kOpeningPageStripCount + strip].get();
            const Matrix4x4 local = MatrixMath::MakeAffineMatrix(
                { stripWidth * 1.008f, kBookPageHeight, 0.003f },
                Vector3 { 0.0f, -localAngle, 0.0f },
                { edgeX + dx * 0.5f, 0.07f, edgeZ + dz * 0.5f });
            object->SetCustomWorldMatrix(MatrixMath::Multiply(local, bookWorld));
            object->GetMaterial()->environmentCoefficient = contact * flutterEnvelope;
            const float shade = 0.96f - static_cast<float>(page % 4) * 0.012f - curl * 0.10f;
            object->SetColor({ shade, shade, shade, 1.0f });
            object->Update();
            edgeX += dx;
            edgeZ += dz;
        }
    }
    leftPageBlock_->Update();
    rightPageBlock_->Update();
}

const std::string& ClearScene::GetPrintedPagePath(uint32_t page) const
{
    return printedPagePaths_[page % printedPagePaths_.size()];
}

void ClearScene::SetPrintedPage(Object3d* object, uint32_t page)
{
    const std::string& path = GetPrintedPagePath(page);
    object->SetModel(ModelManager::GetInstance()->CreateBookLeaf(path, path));
}

void ClearScene::LaunchFirework()
{
    static constexpr const char* kFireworkTypes[] = {
        "BlueFireworkSparks",
        "SixDirectionFireworkTrails",
        "RainbowRingFirework",
        "GoldenWillowFirework",
        "CrownFirework",
    };
    static constexpr float kX[] = {
        -10.0f, 7.0f, -4.0f, 11.0f, 1.0f, -8.0f,
        5.0f, -12.0f, 9.0f, -1.0f, 13.0f, -6.0f
    };
    static constexpr float kY[] = {
        1.0f, 4.0f, 7.0f, 2.5f, 5.5f, 8.0f,
        1.5f, 5.0f, 7.5f, 3.5f, 6.0f, 2.0f
    };
    const int i = fireworkIndex_ % 12;
    const Vector3 position = {
        kX[i], kY[i], 13.0f + static_cast<float>(fireworkIndex_ % 3) * 4.0f
    };
    const int typeIndex = (fireworkIndex_ * 3 + fireworkIndex_ / 2) % 5;
    EffectManager::GetInstance()->PlayEffect(kFireworkTypes[typeIndex], position);
    if (fireworkIndex_ % 5 == 4) {
        EffectManager::GetInstance()->PlayEffect(
            kFireworkTypes[(typeIndex + 2) % 5],
            { position.x + 0.35f, position.y + 0.2f, position.z });
    }
    ++fireworkIndex_;
}
