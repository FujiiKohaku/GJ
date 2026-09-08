#pragma once

#include "BaseScene.h"
#include "Engine/2D/Sprite.h"
#include <memory>
#include <string>

class GamePlayScene;

class LoadingScene : public BaseScene {
public:
    explicit LoadingScene(std::string levelPath);

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    std::string levelPath_;
    std::unique_ptr<GamePlayScene> targetScene_;
    std::unique_ptr<Sprite> backgroundSprite_;
    std::unique_ptr<Sprite> progressTrackSprite_;
    std::unique_ptr<Sprite> progressFillSprite_;
    float elapsedTime_ = 0.0f;
    bool initializationComplete_ = false;
    bool transitionRequested_ = false;
};
