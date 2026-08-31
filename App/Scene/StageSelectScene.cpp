#include "StageSelectScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "GamePlayScene.h"
#include "SceneManager.h"
#include "TestScene.h"
#include "TitleScene.h"

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
}

void StageSelectScene::Initialize()
{
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    backgroundSprite_->SetSize({ 1280.0f, 720.0f });
    backgroundSprite_->SetColor({ 0.025f, 0.07f, 0.09f, 1.0f });

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("STAGE SELECT");
    titleText_->SetPosition({ 640.0f, 120.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(64.0f);
    titleText_->SetColor({ 0.45f, 1.0f, 0.75f, 1.0f });

    stageText_ = std::make_unique<Text>();
    stageText_->Initialize(kDefaultFont);
    stageText_->SetText("TEST STAGE");
    stageText_->SetPosition({ 640.0f, 330.0f });
    stageText_->SetAnchorPoint({ 0.5f, 0.5f });
    stageText_->SetFontSize(48.0f);

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText(
        "ENTER / SPACE : GAME PLAY   T : TEST   BACKSPACE : TITLE");
    instructionText_->SetPosition({ 640.0f, 560.0f });
    instructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    instructionText_->SetFontSize(24.0f);
}

void StageSelectScene::Finalize()
{
}

void StageSelectScene::Update()
{
    Input* input = Input::GetInstance();
    if (input->IsKeyTrigger(DIK_RETURN) || input->IsKeyTrigger(DIK_SPACE)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<GamePlayScene>());
        return;
    }
    if (input->IsKeyTrigger(DIK_T)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TestScene>());
        return;
    }
    if (input->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<TitleScene>());
        return;
    }

    backgroundSprite_->Update();
    titleText_->Update();
    stageText_->Update();
    instructionText_->Update();
}

void StageSelectScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    stageText_->Draw();
    instructionText_->Draw();
}

void StageSelectScene::Draw3D()
{
}

void StageSelectScene::DrawParticle()
{
}

void StageSelectScene::DrawImGui()
{
}
