#include "EditorScene.h"

#include "Engine/Input/Input.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include "SceneManager.h"
#include "StageSelectScene.h"
#include <iostream>
#include <thread>
#include <cstdlib>
#include <cmath>
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include <windows.h>

namespace {
    constexpr const char* kStage1Json = "resources/Maps/stage1.json";
    constexpr int kUdpPort = 50000;
}

void EditorScene::Initialize()
{
    // カメラ設定
    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->SetTranslate({0.0f, 0.0f, -15.0f});
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    // エディタシーンではデバッグ線(マス目)を強制表示する
    DebugRenderer::GetInstance()->SetVisible(true);

    // 初期マップロード
    LevelDataLoader loader;
    LevelData levelData = loader.Load(kStage1Json);
    LevelData::TileMapData mapData{};
    if (!levelData.tileMaps.empty()) {
        mapData = levelData.tileMaps[0];
    }
    mapChipStage_.Initialize(mapData);

    // UDPサーバーの起動
    udpServer_ = std::make_unique<UdpServer>();
    if (udpServer_->Initialize(kUdpPort)) {
        Logger::Log("UdpServer Initialized on port " + std::to_string(kUdpPort) + "\n");
    } else {
        Logger::Log("Failed to initialize UdpServer\n");
    }

    // Python ツールの自動起動 (黒いターミナルウィンドウを出さないように ShellExecute を使用)
    ShellExecuteA(
        nullptr,
        "open",
        "pythonw",
        "C:\\Users\\flone\\.gemini\\antigravity-ide\\brain\\61c407c1-55ac-4150-b0f6-731f9df6b871\\scratch\\editor_tool.py",
        nullptr,
        SW_HIDE
    );
}

void EditorScene::Finalize()
{
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(nullptr);
    
    if (udpServer_) {
        udpServer_->Finalize();
    }
}

void EditorScene::Update()
{
    // シーン遷移による Finalize で nullptr に上書きされるのを防ぐため、毎フレーム再設定する
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());
    SkinningObject3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    // シーン遷移
    if (Input::GetInstance()->IsKeyTrigger(DIK_BACKSPACE)) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<StageSelectScene>());
        return;
    }

    // UDP 受信
    std::string receivedMsg;
    if (udpServer_ && udpServer_->Receive(receivedMsg)) {
        ProcessUdpCommand(receivedMsg);
    }

    // カメラ更新とレイキャストエディット
    UpdateCamera();
    UpdateRaycastEdit();
    
    camera_->Update();
    mapChipStage_.Update();
}

void EditorScene::Draw2D()
{
}

void EditorScene::Draw3D()
{
    // グリッドの描画
    auto debugRenderer = DebugRenderer::GetInstance();
    uint32_t width = mapChipStage_.GetField().GetBlockWidth();
    uint32_t height = mapChipStage_.GetField().GetBlockHeight();
    
    float kChipWidth = 1.0f;
    float kChipHeight = 1.0f;
    Vector4 gridColor = { 1.0f, 0.0f, 0.0f, 1.0f }; // 赤
    float thickness = 5.0f; // より太くする
    float zPos = -2.0f; // ブロックより確実に手前に出す
    
    // 縦線
    for (uint32_t x = 0; x <= width; ++x) {
        float posX = static_cast<float>(x) * kChipWidth - 0.5f;
        Vector3 start{ posX, -0.5f, zPos };
        Vector3 end{ posX, static_cast<float>(height) * kChipHeight - 0.5f, zPos };
        debugRenderer->AddLine(start, end, gridColor, thickness);
    }
    // 横線
    for (uint32_t y = 0; y <= height; ++y) {
        float posY = static_cast<float>(y) * kChipHeight - 0.5f;
        Vector3 start{ -0.5f, posY, zPos };
        Vector3 end{ static_cast<float>(width) * kChipWidth - 0.5f, posY, zPos };
        debugRenderer->AddLine(start, end, gridColor, thickness);
    }

    Object3dManager::GetInstance()->PreDraw();
    mapChipStage_.Draw();
}

void EditorScene::DrawParticle()
{
}

void EditorScene::DrawImGui()
{
}

void EditorScene::ProcessUdpCommand(const std::string& command)
{
    // コマンド解析 (例: "PALETTE:1", "LOAD:filename.json", "SAVE:filename.json")
    if (command.find("PALETTE:") == 0) {
        std::string valStr = command.substr(8);
        currentPalette_ = std::stoi(valStr);
        Logger::Log("Palette changed to: " + std::to_string(currentPalette_) + "\n");
    }
    else if (command.find("LOAD:") == 0) {
        std::string filename = command.substr(5);
        LevelDataLoader loader;
        LevelData levelData = loader.Load(filename);
        if (!levelData.tileMaps.empty()) {
            mapChipStage_.Initialize(levelData.tileMaps[0]);
            Logger::Log("Loaded map: " + filename + "\n");
        }
    }
    else if (command.find("SAVE:") == 0) {
        std::string filename = command.substr(5);
        LevelData levelData;
        LevelData::TileMapData mapData = mapChipStage_.GetField().GetTileMapData();
        levelData.tileMaps.push_back(mapData);
        LevelDataLoader loader;
        loader.Save(filename, levelData);
        Logger::Log("Saved map to: " + filename + "\n");
    }
}

void EditorScene::UpdateCamera()
{
    auto input = Input::GetInstance();
    Vector3 translation = camera_->GetTranslate();

    // Shift + 左ドラッグでパン (平行移動)
    if (input->IsKeyPressed(DIK_LSHIFT) && input->IsMousePressed(0)) {
        float dx = static_cast<float>(input->GetMouseDeltaX());
        float dy = static_cast<float>(input->GetMouseDeltaY());
        translation.x -= dx * 0.05f;
        translation.y += dy * 0.05f;
    }

    // WASD または 矢印キーでも移動できるようにしておく
    if (input->IsKeyPressed(DIK_W) || input->IsKeyPressed(DIK_UP))    { translation.y += cameraSpeed_; }
    if (input->IsKeyPressed(DIK_S) || input->IsKeyPressed(DIK_DOWN))  { translation.y -= cameraSpeed_; }
    if (input->IsKeyPressed(DIK_D) || input->IsKeyPressed(DIK_RIGHT)) { translation.x += cameraSpeed_; }
    if (input->IsKeyPressed(DIK_A) || input->IsKeyPressed(DIK_LEFT))  { translation.x -= cameraSpeed_; }

    // マウスホイールでズーム (Z軸移動)
    float wheel = static_cast<float>(input->GetMouseWheel());
    if (wheel != 0.0f) {
        translation.z += wheel * 0.01f;
    }

    camera_->SetTranslate(translation);
}

void EditorScene::UpdateRaycastEdit()
{
    auto input = Input::GetInstance();
    
    // Shiftを押していない時の左クリックで配置、右クリックで削除とする
    bool isLeftClick = !input->IsKeyPressed(DIK_LSHIFT) && input->IsMousePressed(0);
    bool isRightClick = input->IsMousePressed(1);

    if (isLeftClick || isRightClick) {
        Vector2 mousePos = input->GetMousePosition();
        Ray ray = camera_->ScreenToRay(mousePos);

        // Z=0平面との交差判定: origin.z + direction.z * t = 0
        if (std::abs(ray.direction.z) > 1e-5f) {
            float t = -ray.origin.z / ray.direction.z;
            if (t >= 0.0f) {
                Vector3 intersection = ray.origin + ray.direction * t;
                
                uint32_t height = mapChipStage_.GetField().GetBlockHeight();
                uint32_t width = mapChipStage_.GetField().GetBlockWidth();
                if (height > 0 && width > 0) {
                    float kChipWidth = 1.0f;
                    float kChipHeight = 1.0f;
                    
                    int xIndex = static_cast<int>(std::round(intersection.x / kChipWidth));
                    int yIndex = static_cast<int>(height) - 1 - static_cast<int>(std::round(intersection.y / kChipHeight));
                    
                    Logger::Log("Mouse Click: Screen(" + std::to_string(mousePos.x) + ", " + std::to_string(mousePos.y) + 
                                ") World(" + std::to_string(intersection.x) + ", " + std::to_string(intersection.y) + 
                                ") Index(" + std::to_string(xIndex) + ", " + std::to_string(yIndex) + ")\n");
                    
                    if (xIndex >= 0 && yIndex >= 0 && xIndex < static_cast<int>(width) && yIndex < static_cast<int>(height)) {
                        MapChipType type = isLeftClick ? static_cast<MapChipType>(currentPalette_) : MapChipType::Blank;
                        MapChipType currentType = mapChipStage_.GetField().GetMapChipTypeByIndex(xIndex, yIndex);
                        
                        if (currentType != type) {
                            mapChipStage_.GetField().SetMapChipTypeByIndex(static_cast<uint32_t>(xIndex), static_cast<uint32_t>(yIndex), type);
                            
                            // 配列を変更したあとに 3D モデルを再構築して画面に反映する
                            // Object3d の作り直しによる GPU リソース解放エラー（アクセス違反）を防ぐため、GPU の実行完了を待つ
                            DirectXCommon::GetInstance()->WaitForGPU();
                            mapChipStage_.Initialize(mapChipStage_.GetField().GetTileMapData());
                        }
                    }
                }
            }
        }
    }
}
