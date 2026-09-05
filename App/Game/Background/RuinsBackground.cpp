#include "RuinsBackground.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr const char* kWhiteTexture = "resources/Textures/white.png";
constexpr Vector4 kGrassColors[] = {
    { 0.28f, 0.50f, 0.17f, 1.0f },
    { 0.34f, 0.57f, 0.20f, 1.0f },
    { 0.40f, 0.63f, 0.24f, 1.0f },
    { 0.31f, 0.54f, 0.19f, 1.0f },
};
constexpr Vector4 kRockColors[] = {
    { 0.44f, 0.44f, 0.39f, 1.0f },
    { 0.50f, 0.49f, 0.42f, 1.0f },
    { 0.54f, 0.52f, 0.44f, 1.0f },
};
constexpr const char* kNearRuins[] = {
    "Ruins/ruin_broken_wall.obj",
    "Ruins/ruin_fallen_pillar.obj",
    "Ruins/ruin_circular_altar.obj",
    "Ruins/ruin_dry_fountain.obj",
};
constexpr const char* kMidRuins[] = {
    "Ruins/ruin_broken_arch.obj",
    "Ruins/ruin_pillar.obj",
    "Ruins/ruin_broken_wall.obj",
};
}

void RuinsBackground::Initialize(const Settings& settings)
{
    settings_ = settings;
    settings_.mapLength = (std::max)(settings_.mapLength, 1.0f);
    settings_.groundDepth = (std::max)(settings_.groundDepth, 1.0f);
    groundAngle_ = std::atan(settings_.groundSlope);
    objects_.clear();

    ground_ = std::make_unique<Object3d>();
    ground_->Initialize(Object3dManager::GetInstance());
    ground_->SetModel(ModelManager::GetInstance()->CreatePlane(kWhiteTexture));
    const float groundWidth = settings_.mapLength + 24.0f;
    ground_->SetScale({ groundWidth,
        settings_.groundDepth * std::sqrt(1.0f + settings_.groundSlope * settings_.groundSlope),
        1.0f });
    ground_->SetRotate({ 1.57079632679f - groundAngle_, 0.0f, 0.0f });
    ground_->SetTranslate({ settings_.mapLength * 0.5f,
        settings_.groundBaseY + settings_.groundDepth * 0.5f * settings_.groundSlope,
        settings_.groundStartZ + settings_.groundDepth * 0.5f });
    ground_->SetEnableLighting(false);
    ground_->GetMaterial()->enableLighting = 9;
    ground_->SetColor({ 0.58f, 0.74f, 0.40f, 1.0f });
    ground_->Update();

    CreateGrass();
    CreateRocks();
    CreateRuins();
    CreateTallBackground();
}

void RuinsBackground::Update()
{
    if (ground_) {
        ground_->Update();
    }
    for (const auto& object : objects_) {
        object->Update();
    }
}

void RuinsBackground::Draw(bool drawGround) const
{
    if (drawGround && ground_) {
        ground_->Draw();
    }
    for (const auto& object : objects_) {
        object->Draw();
    }
}

float RuinsBackground::GroundHeight(float z) const
{
    return settings_.groundBaseY +
        (z - settings_.groundStartZ) * settings_.groundSlope;
}

float RuinsBackground::Random01(uint32_t index) const
{
    uint32_t value = index + settings_.seed;
    value ^= value >> 16;
    value *= 0x7FEB352Du;
    value ^= value >> 15;
    value *= 0x846CA68Bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00FFFFFFu) / 16777215.0f;
}

Model* RuinsBackground::LoadModel(const std::string& path, bool useModelTextures) const
{
    Model* model = ModelManager::GetInstance()->Load(path);
    if (!useModelTextures) {
        for (uint32_t index = 0; index < model->GetModelData().materials.size(); ++index) {
            model->SetTexture(kWhiteTexture, index);
        }
    }
    return model;
}

void RuinsBackground::AddObject(
    const std::string& modelPath,
    const Vector3& position,
    float scale,
    const Vector4& color,
    bool lighting,
    bool useModelTextures)
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(Object3dManager::GetInstance());
    object->SetModel(LoadModel(modelPath, useModelTextures));
    object->SetTranslate({ position.x, GroundHeight(position.z) + position.y, position.z });
    object->SetRotate({ -groundAngle_, 0.0f, 0.0f });
    object->SetScale({ scale, scale, scale });
    object->SetColor(color);
    object->SetEnableLighting(lighting);
    object->Update();
    objects_.push_back(std::move(object));
}

void RuinsBackground::CreateGrass()
{
    // Roughly one pair per four map units; jitter avoids a repeated fence-like rhythm.
    const uint32_t groupCount = static_cast<uint32_t>(std::ceil(settings_.mapLength / 4.0f)) + 2;
    for (uint32_t group = 0; group < groupCount; ++group) {
        const float centerX = -3.0f + static_cast<float>(group) * 4.0f;
        for (uint32_t member = 0; member < 2; ++member) {
            const uint32_t key = group * 11u + member * 37u;
            const float x = centerX + (Random01(key) - 0.5f) * 3.2f;
            const float z = 12.0f + Random01(key + 1u) * 16.0f;
            const float scale = 0.42f + Random01(key + 2u) * 0.58f;
            const uint32_t colorIndex = static_cast<uint32_t>(Random01(key + 3u) * 3.99f);
            AddObject("Nature/grass_clump.obj", { x, 0.0f, z }, scale,
                kGrassColors[colorIndex], true);
        }
    }
}

void RuinsBackground::CreateRocks()
{
    const uint32_t count = static_cast<uint32_t>(std::ceil(settings_.mapLength / 10.0f)) + 1;
    for (uint32_t index = 0; index < count; ++index) {
        const float x = -2.0f + static_cast<float>(index) * 10.0f +
            (Random01(index + 201u) - 0.5f) * 5.0f;
        const float z = 14.0f + Random01(index + 202u) * 13.0f;
        const float scale = 0.28f + Random01(index + 203u) * 0.42f;
        const uint32_t colorIndex = static_cast<uint32_t>(Random01(index + 204u) * 2.99f);
        AddObject("Nature/ground_rock.obj", { x, 0.0f, z }, scale,
            kRockColors[colorIndex], true);
    }
}

void RuinsBackground::CreateRuins()
{
    const uint32_t clusterCount = static_cast<uint32_t>(std::ceil(settings_.mapLength / 18.0f)) + 1;
    for (uint32_t cluster = 0; cluster < clusterCount; ++cluster) {
        const float x = 2.0f + static_cast<float>(cluster) * 18.0f +
            (Random01(cluster + 401u) - 0.5f) * 4.0f;
        const uint32_t nearIndex = cluster % static_cast<uint32_t>(std::size(kNearRuins));
        const uint32_t midIndex = (cluster * 2u + 1u) % static_cast<uint32_t>(std::size(kMidRuins));
        AddObject(kNearRuins[nearIndex], { x - 3.0f, 0.0f, 14.0f },
            0.68f + Random01(cluster + 402u) * 0.28f,
            { 0.60f, 0.59f, 0.55f, 1.0f }, true);
        AddObject(kMidRuins[midIndex], { x + 3.2f, 0.0f, 25.0f },
            0.90f + Random01(cluster + 403u) * 0.32f,
            { 0.68f, 0.68f, 0.65f, 1.0f }, true);
    }

    const uint32_t distantCount = static_cast<uint32_t>(std::ceil(settings_.mapLength / 42.0f)) + 1;
    for (uint32_t index = 0; index < distantCount; ++index) {
        AddObject("Ruins/ruin_distant_temple.obj",
            { static_cast<float>(index) * 42.0f, 0.0f, 46.0f },
            0.82f + Random01(index + 501u) * 0.22f,
            { 0.58f, 0.60f, 0.61f, 1.0f }, false);
    }
}

void RuinsBackground::CreateTallBackground()
{
    // Tall silhouettes continue above the ground-level ruins when the camera rises.
    const uint32_t towerCount =
        static_cast<uint32_t>(std::ceil(settings_.mapLength / 38.0f)) + 1;
    for (uint32_t index = 0; index < towerCount; ++index) {
        const float x = 13.0f + static_cast<float>(index) * 38.0f +
            (Random01(index + 701u) - 0.5f) * 6.0f;
        const float scale = 0.82f + Random01(index + 702u) * 0.22f;
        AddObject("Ruins/ruin_watchtower.obj", { x, 0.0f, 40.0f }, scale,
            { 0.80f, 0.82f, 0.80f, 1.0f }, true, true);
    }
}
