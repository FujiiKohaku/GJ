#include "GameOverScene.h"

#include "Engine/2D/Text/TextRenderer.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/PostEffect/PostEffectType.h"
#include "Engine/Time/TimeManager.h"
#include "SceneManager.h"
#include "ArchiveScene.h"
#include "PageTransition.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr const char* kArchiveRoomModel = "StageSelectBook/ArchiveRoom.obj";
constexpr const char* kBookLeather = "resources/Models/StageSelectBook/BookLeather.png";
constexpr const char* kPrintedPage = "resources/Models/StageSelectBook/Pages/page_001.png";
constexpr const char* kDefaultFont =
    "resources/Fonts/NotoSansJP/NotoSansJP-Variable.ttf";
}

void GameOverScene::Initialize()
{
    SceneManager::GetInstance()->SetPostEffectType(PostEffectType::ArchiveAtmosphere);
    SceneManager::GetInstance()->SetArchiveApproach(1.0f);

    camera_ = std::make_unique<Camera>();
    camera_->Initialize();
    camera_->LookAt({ 0.0f, 4.2f, -28.0f }, { 0.0f, -2.2f, 1.0f });
    camera_->Update();
    Object3dManager::GetInstance()->SetDefaultCamera(camera_.get());

    backdrop_ = std::make_unique<Object3d>();
    backdrop_->Initialize(Object3dManager::GetInstance());
    backdrop_->SetModel(ModelManager::GetInstance()->Load(kArchiveRoomModel));
    backdrop_->SetEnableLighting(false);
    backdrop_->SetColor({ 0.32f, 0.20f, 0.22f, 1.0f });
    backdrop_->Update();

    Object3dManager* objectManager = Object3dManager::GetInstance();
    ModelManager* modelManager = ModelManager::GetInstance();
    const auto addFallingProp = [&](Model* model, const Vector3& scale,
                                    const Vector3& position, const Vector4& color,
                                    float delay, const Vector3& velocity,
                                    const Vector3& angularVelocity) {
        FallingProp prop;
        prop.object = std::make_unique<Object3d>();
        prop.object->Initialize(objectManager);
        prop.object->SetModel(model);
        prop.object->SetScale(scale);
        prop.object->SetTranslate(position);
        prop.object->SetColor(color);
        prop.object->SetEnableLighting(false);
        prop.object->Update();
        prop.position = position;
        prop.velocity = velocity;
        prop.angularVelocity = angularVelocity;
        prop.delay = delay;
        prop.halfHeight = scale.y * 0.5f;
        prop.halfExtent = { scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f };
        fallingProps_.push_back(std::move(prop));
    };

    Model* leatherModel = modelManager->CreateCube(kBookLeather);
    Model* pageModel = modelManager->CreateCube(kPrintedPage);
    addFallingProp(leatherModel, { 4.75f, 5.70f, 0.22f },
        { -2.38f, -0.15f, 0.35f }, { 0.70f, 0.58f, 0.42f, 1.0f },
        0.55f, { -1.4f, 0.0f, 0.2f }, { 0.4f, -0.3f, 1.2f });
    addFallingProp(leatherModel, { 4.75f, 5.70f, 0.22f },
        { 2.38f, -0.15f, 0.35f }, { 0.70f, 0.58f, 0.42f, 1.0f },
        0.70f, { 1.3f, 0.0f, -0.1f }, { -0.3f, 0.4f, -1.1f });
    addFallingProp(pageModel, { 4.42f, 5.03f, 0.24f },
        { -2.27f, -0.08f, 0.02f }, { 0.92f, 0.87f, 0.72f, 1.0f },
        0.78f, { -0.7f, 0.4f, 0.4f }, { 0.8f, 0.2f, 0.7f });
    addFallingProp(pageModel, { 4.42f, 5.03f, 0.24f },
        { 2.27f, -0.08f, 0.02f }, { 0.92f, 0.87f, 0.72f, 1.0f },
        0.90f, { 0.8f, 0.3f, 0.3f }, { -0.7f, -0.2f, -0.8f });
    addFallingProp(leatherModel, { 0.24f, 5.45f, 0.62f },
        { 0.0f, -0.15f, 0.18f }, { 0.45f, 0.24f, 0.12f, 1.0f },
        1.00f, { 0.2f, 0.0f, 0.0f }, { 0.2f, 0.8f, 1.5f });

    addFallingProp(leatherModel, { 8.6f, 4.0f, 3.2f },
        { 0.0f, -4.95f, 1.0f }, { 0.28f, 0.16f, 0.09f, 1.0f },
        0.0f, {}, {});
    fallingProps_.back().settled = true;

    slimeShower_ = std::make_unique<DeathSlimeShower>();
    slimeShower_->Initialize(120);
    slimeShower_->spawnStaggerDuration = 3.0f;
    slimeShower_->floorY = -7.0f;
    slimeShower_->boundaryExtent = 8.0f;
    slimeShower_->centerExclusionHalfWidth = 4.25f;
    slimeShower_->centerExclusionHalfDepth = 3.0f;
    slimeShower_->Clear();
    slimeShower_->SpawnRain(60, { 0.0f, 8.0f, 1.5f }, 5.5f);

    titleText_ = std::make_unique<Text>();
    titleText_->Initialize(kDefaultFont);
    titleText_->SetText("GAME OVER");
    titleText_->SetPosition({ 640.0f, 260.0f });
    titleText_->SetAnchorPoint({ 0.5f, 0.5f });
    titleText_->SetFontSize(60.0f);
    titleText_->SetColor({ 1.0f, 0.35f, 0.40f, 1.0f });
    titleText_->SetOutlineColor({ 0.08f, 0.0f, 0.01f, 1.0f });
    titleText_->SetOutlineWidth(2.0f);

    instructionText_ = std::make_unique<Text>();
    instructionText_->Initialize(kDefaultFont);
    instructionText_->SetText("ENTER / SPACE : TITLE");
    instructionText_->SetPosition({ 640.0f, 500.0f });
    instructionText_->SetAnchorPoint({ 0.5f, 0.5f });
    instructionText_->SetFontSize(28.0f);
    instructionText_->SetColor({ 0.82f, 0.74f, 0.74f, 1.0f });
    sceneTime_ = 0.0f;
    slimeRevealTime_ = 0.0f;
    slimeRevealActive_ = PageTransition::ConsumeSlimeReveal();
    SceneManager::GetInstance()->SetSlimeScreenProgress(0.0f);
    if (slimeRevealActive_) {
        SceneManager::GetInstance()->SetSlimeScreenProgress(1.0f);
        SceneManager::GetInstance()->AddPostEffect(
            PostEffectType::SlimeScreen, PostEffectStage::AfterParticle);
        titleText_->SetColor({ 1.0f, 0.35f, 0.40f, 0.0f });
        instructionText_->SetColor({ 0.82f, 0.74f, 0.74f, 0.0f });
    }
}

void GameOverScene::Finalize()
{
    SceneManager::GetInstance()->RemovePostEffect(PostEffectType::SlimeScreen);
    SceneManager::GetInstance()->SetSlimeScreenProgress(0.0f);
    Object3dManager::GetInstance()->SetDefaultCamera(nullptr);
}

void GameOverScene::Update()
{
    Input* input = Input::GetInstance();
    if (!slimeRevealActive_ && sceneTime_ >= 1.0f &&
        (input->IsKeyTrigger(DIK_RETURN) || input->IsKeyTrigger(DIK_SPACE))) {
        SceneManager::GetInstance()->SetNextScene(std::make_unique<ArchiveScene>());
        return;
    }

    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    sceneTime_ += deltaTime;
    if (slimeRevealActive_) {
        constexpr float kRevealDuration = 1.05f;
        slimeRevealTime_ += deltaTime;
        const float progress = 1.0f - std::clamp(slimeRevealTime_ / kRevealDuration, 0.0f, 1.0f);
        SceneManager::GetInstance()->SetSlimeScreenProgress(progress);
        titleText_->SetColor({ 1.0f, 0.35f, 0.40f, 1.0f - progress });
        instructionText_->SetColor({ 0.82f, 0.74f, 0.74f, 1.0f - progress });
        if (progress <= 0.0f) {
            slimeRevealActive_ = false;
            SceneManager::GetInstance()->RemovePostEffect(PostEffectType::SlimeScreen);
        }
    }
    camera_->Update();
    backdrop_->Update();
    for (FallingProp& prop : fallingProps_) {
        if (sceneTime_ >= prop.delay && !prop.settled) {
            prop.velocity.y += -14.0f * deltaTime;
            prop.position.x += prop.velocity.x * deltaTime;
            prop.position.y += prop.velocity.y * deltaTime;
            prop.position.z += prop.velocity.z * deltaTime;
            prop.rotation.x += prop.angularVelocity.x * deltaTime;
            prop.rotation.y += prop.angularVelocity.y * deltaTime;
            prop.rotation.z += prop.angularVelocity.z * deltaTime;

            const float groundCenter = -7.0f + prop.halfHeight;
            if (prop.position.y <= groundCenter) {
                prop.position.y = groundCenter;
                if (std::abs(prop.velocity.y) > 1.0f) {
                    prop.velocity.y = -prop.velocity.y * 0.22f;
                    prop.velocity.x *= 0.55f;
                    prop.velocity.z *= 0.55f;
                    prop.angularVelocity.x *= 0.50f;
                    prop.angularVelocity.y *= 0.50f;
                    prop.angularVelocity.z *= 0.50f;
                } else {
                    prop.velocity = {};
                    prop.angularVelocity = {};
                    prop.settled = true;
                }
            }

            // 最後の要素は台座。本が台座を貫通したら上面へ戻し、外側へ滑らせる。
            if (&prop != &fallingProps_.back()) {
                const FallingProp& pedestal = fallingProps_.back();
                const float pedestalTop = pedestal.position.y + pedestal.halfExtent.y;
                const bool overlapsX =
                    std::abs(prop.position.x - pedestal.position.x) <
                    prop.halfExtent.x + pedestal.halfExtent.x;
                const bool overlapsZ =
                    std::abs(prop.position.z - pedestal.position.z) <
                    prop.halfExtent.z + pedestal.halfExtent.z;
                const bool crossesTop =
                    prop.position.y - prop.halfExtent.y < pedestalTop &&
                    prop.position.y > pedestal.position.y;
                if (overlapsX && overlapsZ && crossesTop) {
                    prop.position.y = pedestalTop + prop.halfExtent.y + 0.03f;
                    prop.velocity.y = (std::max)(prop.velocity.y, 0.0f);
                    const float slideDirection = prop.position.x < pedestal.position.x ? -1.0f : 1.0f;
                    prop.velocity.x += slideDirection * 4.0f * deltaTime;
                }
            }
        }
        prop.object->SetTranslate(prop.position);
        prop.object->SetRotate(prop.rotation);
        prop.object->Update();
    }
    std::vector<DeathSlimeShower::CollisionBox> propCollisions;
    propCollisions.reserve(fallingProps_.size());
    for (const FallingProp& prop : fallingProps_) {
        // 回転物は少し小さめの軸平行ボックスで包み、引っ掛かり過ぎを防ぐ。
        propCollisions.push_back({
            prop.position,
            { prop.halfExtent.x * 0.82f,
              prop.halfExtent.y * 0.82f,
              prop.halfExtent.z * 0.82f }
        });
    }
    slimeShower_->SetCollisionBoxes(propCollisions);
    slimeShower_->Update(deltaTime);
    titleText_->Update();
    instructionText_->Update();
}

void GameOverScene::Draw2D()
{
    TextRenderer::GetInstance()->PreDraw();
    titleText_->Draw();
    instructionText_->Draw();
}

void GameOverScene::Draw3D()
{
    Object3dManager::GetInstance()->PreDraw();
    backdrop_->Draw();
    for (const FallingProp& prop : fallingProps_) {
        prop.object->Draw();
    }
    slimeShower_->Draw();
}

void GameOverScene::DrawParticle()
{
}

void GameOverScene::DrawImGui()
{
}
