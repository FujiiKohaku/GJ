#pragma once

#include "App/Game/Map/MapChipStage.h"
#include "BaseScene.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Network/UdpServer.h"
#include "Engine/3D/SkyBox/SkyBox.h"
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
};
