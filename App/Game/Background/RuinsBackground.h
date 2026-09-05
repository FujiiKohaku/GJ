#pragma once

#include "Engine/3D/Object3d.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Model;

class RuinsBackground {
public:
    struct Settings {
        float mapLength = 1.0f;
        float groundStartZ = 1.0f;
        float groundDepth = 100.0f;
        float groundBaseY = -0.5f;
        float groundSlope = 0.06f;
        uint32_t seed = 0x51A6E5u;
    };

    void Initialize(const Settings& settings);
    void Update();
    void Draw(bool drawGround) const;

private:
    float GroundHeight(float z) const;
    float Random01(uint32_t index) const;
    Model* LoadWhiteModel(const std::string& path) const;
    void AddObject(
        const std::string& modelPath,
        const Vector3& position,
        float scale,
        const Vector4& color,
        bool lighting);
    void CreateGrass();
    void CreateRocks();
    void CreateRuins();

    Settings settings_;
    float groundAngle_ = 0.0f;
    std::unique_ptr<Object3d> ground_;
    std::vector<std::unique_ptr<Object3d>> objects_;
};
