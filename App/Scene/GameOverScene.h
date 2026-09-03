#pragma once

#include "BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include "App/Effect/DeathSlimeShower.h"
#include <memory>
#include <vector>

class GameOverScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    struct FallingProp {
        std::unique_ptr<Object3d> object;
        Vector3 position {};
        Vector3 velocity {};
        Vector3 rotation {};
        Vector3 angularVelocity {};
        float delay = 0.0f;
        float halfHeight = 0.0f;
        Vector3 halfExtent {};
        bool settled = false;
    };

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> backdrop_;
    std::vector<FallingProp> fallingProps_;
    std::unique_ptr<DeathSlimeShower> slimeShower_;
    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> instructionText_;
    float sceneTime_ = 0.0f;
    float transitionTime_ = 0.0f;
    bool isTransitioning_ = false;
};
