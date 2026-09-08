#include "LoadingScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/TextureManager/TextureManager.h"
#include "Engine/Time/TimeManager.h"
#include "GamePlayScene.h"
#include "PageTransition.h"
#include "SceneManager.h"
#include <algorithm>
#include <utility>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr float kProgressWidth = 520.0f;
constexpr float kMinimumDisplayTime = 0.25f;
const Vector4 kPaperColor = {1.0f, 0.91f, 0.68f, 1.0f};
}

LoadingScene::LoadingScene(std::string levelPath)
    : levelPath_(std::move(levelPath)) {
}

void LoadingScene::Initialize() {
    elapsedTime_ = 0.0f;
    initializationComplete_ = false;
    transitionRequested_ = false;

    TextureManager::GetInstance()->LoadTexture(kWhiteTexture);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    backgroundSprite_->SetSize({1280.0f, 720.0f});
    backgroundSprite_->SetPosition({0.0f, 0.0f});
    backgroundSprite_->SetColor(kPaperColor);
    backgroundSprite_->Update();

    progressTrackSprite_ = std::make_unique<Sprite>();
    progressTrackSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    progressTrackSprite_->SetSize({kProgressWidth, 12.0f});
    progressTrackSprite_->SetPosition({380.0f, 624.0f});
    progressTrackSprite_->SetColor({0.23f, 0.15f, 0.08f, 0.28f});
    progressTrackSprite_->Update();

    progressFillSprite_ = std::make_unique<Sprite>();
    progressFillSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    progressFillSprite_->SetSize({0.0f, 8.0f});
    progressFillSprite_->SetPosition({380.0f, 626.0f});
    progressFillSprite_->SetColor({0.23f, 0.15f, 0.08f, 0.90f});
    progressFillSprite_->Update();

    targetScene_ = std::make_unique<GamePlayScene>(levelPath_);
    targetScene_->BeginIncrementalInitialize();
}

void LoadingScene::Finalize() {
}

void LoadingScene::Update() {
    elapsedTime_ += TimeManager::GetInstance()->GetUnscaledDeltaTime();

    if (!initializationComplete_) {
        initializationComplete_ = targetScene_->InitializeNextStep();
    }

    float progress = targetScene_->GetInitializationProgress();
    progress = std::clamp(progress, 0.0f, 1.0f);
    progressFillSprite_->SetSize({kProgressWidth * progress, 8.0f});
    progressFillSprite_->Update();

    if (initializationComplete_ && elapsedTime_ >= kMinimumDisplayTime &&
        !transitionRequested_) {
        PageTransition::RequestReveal(kPaperColor, 0.40f);
        targetScene_->InitializeRevealOverlay();
        SceneManager::GetInstance()->SetNextPreparedScene(std::move(targetScene_));
        transitionRequested_ = true;
    }
}

void LoadingScene::Draw2D() {
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();
    progressTrackSprite_->Draw();
    progressFillSprite_->Draw();
}

void LoadingScene::Draw3D() {
}

void LoadingScene::DrawParticle() {
}

void LoadingScene::DrawImGui() {
}
