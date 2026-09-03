#pragma once

#include "Engine/3D/Object3d.h"
#include "Engine/Math/MathStruct.h"
#include <vector>
#include <memory>

struct SlimeParticle {
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };
    Vector3 rotationSpeed = { 0.0f, 0.0f, 0.0f };
    
    float baseScale = 0.5f;
    float squashY = 1.0f;          // 縦の伸縮率 (1.0が通常, <1.0で潰れ, >1.0で伸び)
    float squashYVelocity = 0.0f;  // スカッシュ変形のバネ速度
    
    Vector4 color = { 0.2f, 0.7f, 1.0f, 1.0f };
    float lifeTime = 12.0f;
    float age = 0.0f;
    bool isActive = false;
    bool isGrounded = false;
    
    std::unique_ptr<Object3d> object;
};

class DeathSlimeShower {
public:
    DeathSlimeShower();
    ~DeathSlimeShower();

    void Initialize(uint32_t maxCapacity = 120);
    void SpawnRain(uint32_t count, const Vector3& center = { 0.0f, 12.0f, 0.0f }, float spread = 4.5f);
    void Update(float deltaTime);
    void Draw();
    void Clear();

    // 物理・ぽよぽよパラメータ
    float gravity = -20.0f;          // 重力
    float restitution = 0.65f;       // 床の跳ね返り係数 (0.0〜1.0)
    float friction = 0.90f;          // 床の横滑り摩擦
    float springStiffness = 160.0f;  // ぽよぽよバネの硬さ
    float springDamping = 10.0f;     // ぽよぽよバネの減衰
    float floorY = 0.0f;             // 地面の高さ
    float boundaryExtent = 9.0f;     // 壁の範囲 (X, Z)

    uint32_t GetActiveCount() const;

private:
    std::vector<SlimeParticle> slimes_;
    uint32_t maxCapacity_ = 120;
};
