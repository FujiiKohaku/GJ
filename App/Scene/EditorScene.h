#pragma once

#include "App/Game/Map/MapChipStage.h"
#include "BaseScene.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Network/UdpServer.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/3D/Model.h"
#include "Engine/3D/Object3d.h"
#include <memory>
#include <string>

class EditorScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    void ProcessUdpCommand(const std::string& command);
    void UpdateCamera();
    void UpdateRaycastEdit();

    std::unique_ptr<Camera> camera_;
    MapChipStage mapChipStage_;
    std::unique_ptr<UdpServer> udpServer_;
    std::unique_ptr<SkyBox> skyBox_;
    
    // 現在選択中のパレット（ブロックの種類）
    int currentPalette_ = 1;
    
    // カメラの操作パラメータ
    float cameraSpeed_ = 0.5f;
    
    // JSONのロード情報を保持する
    LevelData currentLevelData_;
    
    // プレビュー表示用
    Model* playerModel_ = nullptr;
    std::unique_ptr<Object3d> playerPreview_;
    
    // 操作状態
    bool isDraggingPlayer_ = false;

    // 選択中のギミック
    int32_t selectedX_ = -1;
    int32_t selectedY_ = -1;
    
    // 外部ツールのプロセスハンドル
    void* toolProcessHandle_ = nullptr;
    
    LevelData::ObjectData* GetSelectedGimmick();
};
