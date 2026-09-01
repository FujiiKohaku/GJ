#include "EditorScene.h"

#include "Engine/Input/Input.h"
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Logger/Logger.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/SkinningObject3dManager.h"
#include "Engine/3D/SkyBox/SkyBoxManager.h"
#include "Engine/TextureManager/TextureManager.h"
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
    constexpr const char* kSkyBoxTexture = "resources/Textures/skybox.dds";
    constexpr int kUdpPort = 50000;
    
    // エディタ専用の巨大キャンバスサイズ
    constexpr uint32_t kEditorCanvasWidth = 256;
    constexpr uint32_t kEditorCanvasHeight = 64;

    // ロードしたマップデータを巨大キャンバスに左下基準で拡張する
    LevelData::TileMapData ExpandTileMapData(const LevelData::TileMapData& src, uint32_t targetWidth, uint32_t targetHeight) {
        LevelData::TileMapData dest;
        dest.name = src.name;
        dest.width = targetWidth > src.width ? targetWidth : src.width;
        dest.height = targetHeight > src.height ? targetHeight : src.height;
        dest.data.resize(dest.width * dest.height, 0);

        uint32_t offsetY = dest.height - src.height; // 上端の余白(これを足すことで元配列が下にシフトし、Y=0が保たれる)
        
        for (uint32_t sy = 0; sy < src.height; ++sy) {
            for (uint32_t sx = 0; sx < src.width; ++sx) {
                uint32_t destIndex = (sy + offsetY) * dest.width + sx;
                uint32_t srcIndex = sy * src.width + sx;
                dest.data[destIndex] = src.data[srcIndex];
            }
        }
        return dest;
    }

    // セーブ時にブロックが存在する最小領域にトリミングする（左端・下端の余白は絶対座標がずれるため残す）
    LevelData::TileMapData TrimTileMapData(const LevelData::TileMapData& src) {
        uint32_t maxX = 0;
        uint32_t minY = src.height; // yIndexが最小(一番上にあるブロック)
        
        bool hasBlock = false;
        for (uint32_t y = 0; y < src.height; ++y) {
            for (uint32_t x = 0; x < src.width; ++x) {
                uint32_t index = y * src.width + x;
                if (src.data[index] != 0) {
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    hasBlock = true;
                }
            }
        }
        
        if (!hasBlock) {
            LevelData::TileMapData dest = src;
            dest.width = 1;
            dest.height = 1;
            dest.data = {0};
            return dest;
        }
        
        uint32_t destWidth = maxX + 1;
        uint32_t destHeight = src.height - minY;
        
        LevelData::TileMapData dest;
        dest.name = src.name;
        dest.width = destWidth;
        dest.height = destHeight;
        dest.data.resize(destWidth * destHeight, 0);
        
        for (uint32_t dy = 0; dy < destHeight; ++dy) {
            uint32_t sy = dy + minY;
            for (uint32_t dx = 0; dx < destWidth; ++dx) {
                uint32_t sx = dx;
                uint32_t destIndex = dy * destWidth + dx;
                uint32_t srcIndex = sy * src.width + sx;
                dest.data[destIndex] = src.data[srcIndex];
            }
        }
        return dest;
    }
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
        mapData = ExpandTileMapData(levelData.tileMaps[0], kEditorCanvasWidth, kEditorCanvasHeight);
    }
    mapChipStage_.Initialize(mapData);

    // 背景(SkyBox)の初期化: これがないとポストエフェクト合成時に空中が透明扱いになりデバッグ線が消える
    TextureManager::GetInstance()->LoadTexture(kSkyBoxTexture);
    skyBox_ = std::make_unique<SkyBox>();
    skyBox_->Initialize(DirectXCommon::GetInstance());
    skyBox_->SetTexture(kSkyBoxTexture);

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
    
    if (skyBox_) {
        skyBox_->Update(camera_.get());
    }
}

void EditorScene::Draw2D()
{
}

void EditorScene::Draw3D()
{
    // グリッドの描画
    auto debugRenderer = DebugRenderer::GetInstance();
    // グリッドは実際のブロック数ではなく、固定のキャンバスサイズ全体に引く
    uint32_t width = kEditorCanvasWidth;
    uint32_t height = kEditorCanvasHeight;
    
    float kChipWidth = 1.0f;
    float kChipHeight = 1.0f;
    Vector4 gridColor = { 1.0f, 0.0f, 0.0f, 1.0f }; // 赤
    float thickness = 2.5f; // 現在の太さの1/2に縮小
    // 透視投影(パース)でズレて見えないよう、ブロックの前面(z=-0.5f)にぴったり合わせつつ、
    // ブロックの面とZ-Fighting(重なって見えなくなる現象)を避けるため、ほんのわずかだけ手前にする
    float zPos = -0.51f; 
    
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

    if (skyBox_) {
        SkyBoxManager::GetInstance()->PreDraw();
        skyBox_->Draw(DirectXCommon::GetInstance()->GetCommandList());
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
            LevelData::TileMapData expandedData = ExpandTileMapData(levelData.tileMaps[0], kEditorCanvasWidth, kEditorCanvasHeight);
            mapChipStage_.Initialize(expandedData);
            Logger::Log("Loaded map: " + filename + "\n");
        }
    }
    else if (command.find("SAVE:") == 0) {
        std::string filename = command.substr(5);
        LevelData::TileMapData rawData = mapChipStage_.GetField().GetTileMapData();
        LevelData::TileMapData trimmedData = TrimTileMapData(rawData);
        
        LevelData levelData;
        levelData.tileMaps.push_back(trimmedData);
        // ※ PlayerSpawn等のオブジェクト情報は現在ロード・保存に対応していないため一旦空とする。
        // 将来的にはロード時に保持しておき、ここで再合成する必要がある。
        
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
