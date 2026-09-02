#include "GamePlayScene.h"

#include "Engine/2D/SpriteManager.h"
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
#include "Engine/LevelEditor/LevelDataLoader.h"
#include "Engine/Time/TimeManager.h"
#include "SceneManager.h"
#include "ArchiveScene.h"
#include <string>

namespace {
constexpr const char* kSkyBoxTexture = "resources/Textures/skybox.dds";
constexpr const char* kMapChipTexture = "resources/Textures/checkerboard.png";
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr const char* kStage1Json = "resources/Maps/stage1.json";
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

    LevelDataLoader loader;
    LevelData levelData = loader.Load(kStage1Json);

    LevelData::TileMapData mapData{};
    if (!levelData.tileMaps.empty()) {
        mapData = levelData.tileMaps[0];
    }
    // levelData 自体を渡して初期化する
    mapChipStage_.Initialize(levelData);

    Vector3 playerStartPos = { 0.0f, 0.0f, 0.0f };
    if (!levelData.playerSpawns.empty()) {
        playerStartPos = levelData.playerSpawns[0].translation;
    }

    Model* playerModel =
        ModelManager::GetInstance()->CreatePlane(kMapChipTexture);
    player_ = std::make_unique<MapChipPlayer>();
    player_->Initialize(playerModel, &mapChipStage_.GetField(), playerStartPos);
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
        "TAB : MENU   BACKSPACE : STAGE SELECT");
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

    // メニューUIの初期化
    TextureManager::GetInstance()->LoadTexture(kWhiteTexture);

    // 全画面半透明暗幕
    menuBackgroundSprite_ = std::make_unique<Sprite>();
    menuBackgroundSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    menuBackgroundSprite_->SetSize({ 1280.0f, 720.0f });
    menuBackgroundSprite_->SetPosition({ 0.0f, 0.0f });
    menuBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.65f });

    // 中央パネル
    menuPanelSprite_ = std::make_unique<Sprite>();
    menuPanelSprite_->Initialize(SpriteManager::GetInstance(), kWhiteTexture);
    menuPanelSprite_->SetSize({ 640.0f, 380.0f });
    menuPanelSprite_->SetPosition({ 320.0f, 170.0f });
    menuPanelSprite_->SetColor({ 0.08f, 0.12f, 0.16f, 0.95f });

    // メニュータイトル
    menuTitleText_ = std::make_unique<Text>();
    menuTitleText_->Initialize(kDefaultFont);
    menuTitleText_->SetText("PAUSE MENU");
    menuTitleText_->SetPosition({ 640.0f, 230.0f });
    menuTitleText_->SetAnchorPoint({ 0.5f, 0.5f });
    menuTitleText_->SetFontSize(44.0f);
    menuTitleText_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // メニュー説明文
    menuInstructionText_ = std::make_unique<Text>();
    menuInstructionText_->Initialize(kDefaultFont);
    menuInstructionText_->SetText("TAB : RESUME GAME\n\nBACKSPACE : STAGE SELECT");
    menuInstructionText_->SetPosition({ 640.0f, 360.0f });
    menuInstructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    menuInstructionText_->SetFontSize(24.0f);
    menuInstructionText_->SetColor({ 0.8f, 0.88f, 0.95f, 1.0f });

    pageReveal_.InitializeIfRequested();
}

void GamePlayScene::Finalize()
{
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void GamePlayScene::Update()
{
    pageReveal_.Update(TimeManager::GetInstance()->GetDeltaTime());

    // TABキーでメニュー開閉
    if (Input::GetInstance()->IsKeyTrigger(DIK_TAB)) {
        isMenuOpen_ = !isMenuOpen_;
    }

    // メニューが開いているときはゲーム内処理を行わずに早期リターン
    if (isMenuOpen_) {
        if (Input::GetInstance()->IsKeyTrigger(DIK_BACKSPACE)) {
            SceneManager::GetInstance()->SetNextScene(
                std::make_unique<ArchiveScene>());
            return;
        }

        menuBackgroundSprite_->Update();
        menuPanelSprite_->Update();
        menuTitleText_->Update();
        menuInstructionText_->Update();
        return;
    }

    if (Input::GetInstance()->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(
            std::make_unique<ArchiveScene>());
        return;
    }

    mapChipStage_.Update(); // Playerの前にGimmickを更新して移動量を出しておくのが理想的
    player_->Update(mapChipStage_.GetGimmicks());
    UpdateFollowCamera();
    camera_->Update();
    skyBox_->Update(camera_.get());
    instructionText_->Update();
    UpdateCollisionText();
    collisionText_->Update();
}

void GamePlayScene::Draw2D()
{
    TextRenderer::GetInstance()->PreDraw();
    instructionText_->Draw();
    collisionText_->Draw();
    pageReveal_.Draw();

    // メニュー表示中は最前面に暗幕とメニューパネルを描画
    if (isMenuOpen_) {
        SpriteManager::GetInstance()->PreDraw();
        menuBackgroundSprite_->Draw();
        menuPanelSprite_->Draw();

        TextRenderer::GetInstance()->PreDraw();
        menuTitleText_->Draw();
        menuInstructionText_->Draw();
    }
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
