#include "TitleScene.h"

#include "StageSelectScene.h"

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize()
{
    archiveScene_ = std::make_unique<StageSelectScene>();
    archiveScene_->Initialize();
}

void TitleScene::Finalize()
{
    if (archiveScene_) {
        archiveScene_->Finalize();
        archiveScene_.reset();
    }
}

void TitleScene::Update()
{
    archiveScene_->Update();
}

void TitleScene::Draw2D()
{
    archiveScene_->Draw2D();
}

void TitleScene::Draw3D()
{
    archiveScene_->Draw3D();
}

void TitleScene::DrawParticle()
{
    archiveScene_->DrawParticle();
}

void TitleScene::DrawImGui()
{
    archiveScene_->DrawImGui();
}
