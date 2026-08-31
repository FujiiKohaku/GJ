#pragma once

#include "BaseScene.h"
#include "Engine/2D/Text/Text.h"
#include "Engine/3D/Object3d.h"
#include "Engine/Camera/Camera.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class StageSelectScene : public BaseScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw2D() override;
    void Draw3D() override;
    void DrawParticle() override;
    void DrawImGui() override;

private:
    enum class BookSelectState {
        CardOpening,
        Idle,
        CardClosing,
        PageTurning,
        StageConfirmed
    };

    struct StageData {
        std::string name;
        std::string description;
        bool opensTestScene = false;
    };

    void InitializeStageData();
    void InitializeBookObjects();
    void InitializeTurningPage();
    void InitializeInterface();
    void StartPageTurn(int32_t direction);
    void UpdateCardOpening(float deltaTime);
    void UpdateCardIdle();
    void UpdateCardClosing(float deltaTime);
    void UpdatePageTurning(float deltaTime);
    void UpdateTurningPage();
    void UpdateCardTransform(float progress, float alpha);
    void ChangeStageIndex();
    void RefreshStageText();
    void ConfirmStage();

    static float Clamp01(float value);
    static float SmoothStep(float value);
    static float EaseOutBack(float value);

private:
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Object3d> backdrop_;
    std::unique_ptr<Object3d> bookCover_;
    std::unique_ptr<Object3d> bookSpine_;
    std::unique_ptr<Object3d> leftPageBlock_;
    std::unique_ptr<Object3d> rightPageBlock_;
    std::vector<std::unique_ptr<Object3d>> turningPageStrips_;
    std::unique_ptr<Object3d> stageCardShadow_;
    std::unique_ptr<Object3d> stageCard_;

    std::unique_ptr<Text> titleText_;
    std::unique_ptr<Text> stageText_;
    std::unique_ptr<Text> descriptionText_;
    std::unique_ptr<Text> pageText_;
    std::unique_ptr<Text> instructionText_;

    std::vector<StageData> stages_;
    BookSelectState state_ = BookSelectState::CardOpening;
    int32_t currentStageIndex_ = 0;
    int32_t pageTurnDirection_ = 1;
    float animationTime_ = 0.0f;
    float pageTurnProgress_ = 0.0f;
    bool stageIndexChanged_ = false;
};
