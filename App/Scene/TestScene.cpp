#include "TestScene.h"

#include "ClearScene.h"
#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "GameOverScene.h"
#include "SceneManager.h"
#include "StageSelectScene.h"

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
}

void TestScene::Initialize()
{
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    backgroundSprite_->SetSize({ 1280.0f, 720.0f });
    backgroundSprite_->SetColor({ 0.08f, 0.035f, 0.075f, 1.0f });

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("TEST SCENE");
    titleText_->SetPosition({ 640.0f, 220.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(72.0f);
    titleText_->SetColor({ 1.0f, 0.55f, 0.85f, 1.0f });

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText("C : CLEAR   G : GAME OVER   BACKSPACE : STAGE SELECT");
    instructionText_->SetPosition({ 640.0f, 500.0f });
    instructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    instructionText_->SetFontSize(24.0f);
}

void TestScene::Finalize()
{
}

void TestScene::Update()
{
    Input* input = Input::GetInstance();
    if (input->IsKeyTrigger(DIK_C)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<ClearScene>());
        return;
    }
    if (input->IsKeyTrigger(DIK_G)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<GameOverScene>());
        return;
    }
    if (input->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<StageSelectScene>());
        return;
    }

    backgroundSprite_->Update();
    titleText_->Update();
    instructionText_->Update();
}

void TestScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    instructionText_->Draw();
}

void TestScene::Draw3D()
{
}

void TestScene::DrawParticle()
{
}

void TestScene::DrawImGui()
{
}
