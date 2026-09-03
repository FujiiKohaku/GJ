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
#include <list>

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
    bool isDraggingGoal_ = false;

    // 選択中のギミック
    int32_t selectedX_ = -1;
    int32_t selectedY_ = -1;
    
    // 外部ツールのプロセスハンドル
    void* toolProcessHandle_ = nullptr;
    
    LevelData::ObjectData* GetSelectedGimmick();

    // --- Undo / Redo 用の履歴管理 ---
    static const size_t kMaxHistory = 30; // 履歴の最大保存数
    std::list<LevelData> history_;
    std::list<LevelData>::iterator historyCurrent_;
    bool isDragging_ = false; // マウスドラッグ操作中かどうかのトラッキング
    bool hasUnsavedChanges_ = false; // 値が変更され、スナップショット保存待ちか
    
    // 現在の状態を履歴に保存する
    void SaveSnapshot();
    // 履歴を戻す/進める
    void Undo();
    void Redo();
};
