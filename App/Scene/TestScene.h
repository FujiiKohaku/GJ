#pragma once

#include "BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Fluid/GpuSphFluid.h"
#include "Engine/Fluid/FluidForceRenderer.h"

#include <memory>

class TestScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<GpuSphFluid> gpuSphFluid_;
    std::unique_ptr<FluidForceRenderer> fluidForceRenderer_;

    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> instructionText_;

    bool showForces_ = false;
};
