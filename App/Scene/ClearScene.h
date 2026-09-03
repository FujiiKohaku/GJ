#pragma once

#include "BaseScene.h"
#include "Engine/2D/Sprite.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/3D/SkyBox/SkyBox.h"
#include "Engine/Camera/Camera.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ClearScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    void InitializeArchiveBook();
    void UpdateArchiveBook(float riffleProgress);
    const std::string& GetPrintedPagePath(uint32_t page) const;
    void SetPrintedPage(Object3d* object, uint32_t page);
    void LaunchFirework();

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<SkyBox> skyBox_;
    std::unique_ptr<Object3d> archiveRoom_;
    std::unique_ptr<Object3d> grassGround_;
    std::vector<std::unique_ptr<Object3d>> meadowTrees_;
    std::vector<std::unique_ptr<Object3d>> meadowTreeCanopies_;
    std::vector<std::unique_ptr<Object3d>> meadowMountains_;
    std::unique_ptr<Object3d> leftBookCover_;
    std::unique_ptr<Object3d> rightBookCover_;
    std::unique_ptr<Object3d> leftPageBlock_;
    std::unique_ptr<Object3d> rightPageBlock_;
    std::unique_ptr<Object3d> bookSpine_;
    std::vector<std::unique_ptr<Object3d>> bookFittings_;
    std::vector<std::unique_ptr<Object3d>> openingPageStrips_;
    std::vector<bool> openingPageVisible_;
    std::vector<std::string> printedPagePaths_;
    std::unique_ptr<Sprite> flashSprite_;
    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> instructionText_;
    float sceneTime_ = 0.0f;
    float fireworkTimer_ = 0.0f;
    int fireworkIndex_ = 0;
    bool meadowRevealed_ = false;
};
