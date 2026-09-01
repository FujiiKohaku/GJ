#include "ClearScene.h"

#include "Engine/2D/SpriteManager.h"
#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "SceneManager.h"
#include "ArchiveScene.h"

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
}

void ClearScene::Initialize()
{
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    backgroundSprite_->SetSize({ 1280.0f, 720.0f });
    backgroundSprite_->SetColor({ 0.02f, 0.10f, 0.07f, 1.0f });

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("CLEAR");
    titleText_->SetPosition({ 640.0f, 260.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(96.0f);
    titleText_->SetColor({ 0.45f, 1.0f, 0.65f, 1.0f });

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText("ENTER / SPACE : TITLE");
    instructionText_->SetPosition({ 640.0f, 500.0f });
    instructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    instructionText_->SetFontSize(28.0f);
}

void ClearScene::Finalize()
{
}

void ClearScene::Update()
{
    Input* input = Input::GetInstance();
    if (input->IsKeyTrigger(DIK_RETURN) || input->IsKeyTrigger(DIK_SPACE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<ArchiveScene>());
        return;
    }

    backgroundSprite_->Update();
    titleText_->Update();
    instructionText_->Update();
}

void ClearScene::Draw2D()
{
    SpriteManager::GetInstance()->PreDraw();
    backgroundSprite_->Draw();
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    instructionText_->Draw();
}

void ClearScene::Draw3D()
{
}

void ClearScene::DrawParticle()
{
}

void ClearScene::DrawImGui()
{
}
