#include "GamePlayScene.h"

#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Input/Input.h"
#include "Engine/Logger/Logger.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/TextureManager/TextureManager.h"
#include "SceneManager.h"
#include "StageSelectScene.h"
#include <string>

namespace {
constexpr const char* kSkyBoxTexture = "resources/Textures/skybox.dds";
constexpr const char* kMapChipTexture = "resources/Textures/checkerboard.png";
constexpr const char* kMapChipCsv = "resources/Maps/blocks.csv";
constexpr float kCameraDistance = 15.0f;
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
}

void GamePlayScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::Copy);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    if (!mapChipStage_.Initialize(kMapChipCsv)) {
        Logger::Log("GamePlayScene: Failed to load map chip CSV");
    }

    Model* playerModel =
        ModelManager::GetInstance()->CreatePlane(kMapChipTexture);
    player_ = std::make_unique<MapChipPlayer>();
    player_->Initialize(playerModel, &mapChipStage_.GetField());
    UpdateFollowCamera();
    camera_->Update();

    TextureManager::GetInstance()->LoadTexture(kSkyBoxTexture);
    skyBox_ = std::make_unique<SkyBox>();
    skyBox_->Initialize(DirectXCommon::GetInstance());
    skyBox_->SetTexture(kSkyBoxTexture);
    skyBox_->Update(camera_.get());

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText(
        "MOVE : A/D OR LEFT/RIGHT   JUMP : SPACE/W/UP   "
        "BACKSPACE : STAGE SELECT");
    instructionText_->SetPosition({ 32.0f, 32.0f });
    instructionText_->SetFontSize(24.0f);
    instructionText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    instructionText_->SetOutlineColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    instructionText_->SetOutlineWidth(2.0f);

    collisionText_ = std::make_unique<Text>();
    collisionText_->Initialize(kDefaultFont);
    collisionText_->SetPosition({ 32.0f, 68.0f });
    collisionText_->SetFontSize(22.0f);
    collisionText_->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f });
    collisionText_->SetOutlineColor({ 0.0f, 0.0f, 0.0f, 1.0f });
    collisionText_->SetOutlineWidth(2.0f);
    UpdateCollisionText();
}

void GamePlayScene::Finalize()
{
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void GamePlayScene::Update()
{
    if (Input::GetInstance()->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<StageSelectScene>());
        return;
    }

    player_->Update();
    UpdateFollowCamera();
    camera_->Update();
    skyBox_->Update(camera_.get());
    mapChipStage_.Update();
    instructionText_->Update();
    UpdateCollisionText();
    collisionText_->Update();
}

void GamePlayScene::Draw2D()
{
    TextRenderer::GetInstance()->PreDraw();
    instructionText_->Draw();
    collisionText_->Draw();
}

void GamePlayScene::Draw3D()
{
    SkyBoxManager::GetInstance()->PreDraw();
    skyBox_->Draw(DirectXCommon::GetInstance()->GetCommandList());

    Object3dManager::GetInstance()->PreDraw();
    mapChipStage_.Draw();
    player_->Draw();

}

void GamePlayScene::DrawParticle()
{
}

void GamePlayScene::DrawImGui()
{
}

void GamePlayScene::UpdateFollowCamera()
{
    if (!player_) {
        return;
    }
    const Vector3 playerPosition = player_->GetPosition();
    camera_->LookAt(
        { playerPosition.x, playerPosition.y, -kCameraDistance },
        { playerPosition.x, playerPosition.y, 0.0f });
}

void GamePlayScene::UpdateCollisionText()
{
    if (!collisionText_ || !player_) {
        return;
    }

    std::string state = "AIR";
    if (player_->IsGrounded()) {
        state = "GROUND COLLISION";
    } else if (player_->IsColliding()) {
        state = "WALL/CEILING COLLISION";
    }
    const Vector3 position = player_->GetPosition();
    collisionText_->SetText(
        "COLLISION : " + state +
        "   PLAYER X=" + std::to_string(position.x) +
        " Y=" + std::to_string(position.y));
}
