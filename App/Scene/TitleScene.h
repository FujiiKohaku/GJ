#pragma once

#include "BaseScene.h"
#include <memory>

class StageSelectScene;

// タイトルとステージ選択を同じ資料庫空間で連続表示する入口シーン。
class TitleScene : public BaseScene {
public:
    TitleScene();
    ~TitleScene() override;
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    std::unique_ptr<StageSelectScene> archiveScene_;
};
