#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DeathSlimeShower.h"

#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include <random>
#include <cmath>
#include <algorithm>

namespace {
constexpr const char* kSlimeModelPath = "slime_mesh.obj";
constexpr const char* kSlimeTexturePath = "resources/Textures/white.png";

// カラフルなスライム用カラーパレット
const Vector4 kSlimeColors[] = {
    { 0.15f, 0.75f, 1.00f, 1.0f }, // 水色 (Cyan/Sky)
    { 0.20f, 0.95f, 0.45f, 1.0f }, // ライムグリーン (Green)
    { 1.00f, 0.35f, 0.65f, 1.0f }, // ピンク (Pink)
    { 1.00f, 0.75f, 0.15f, 1.0f }, // イエロー/ゴールド (Yellow)
    { 0.75f, 0.35f, 1.00f, 1.0f }, // パープル (Purple)
    { 1.00f, 0.45f, 0.20f, 1.0f }, // オレンジ (Orange)
    { 0.30f, 0.50f, 1.00f, 1.0f }, // ディープブルー (Blue)
};
constexpr size_t kSlimeColorCount = sizeof(kSlimeColors) / sizeof(kSlimeColors[0]);
}

DeathSlimeShower::DeathSlimeShower() = default;
DeathSlimeShower::~DeathSlimeShower() = default;

void DeathSlimeShower::Initialize(uint32_t maxCapacity)
{
    maxCapacity_ = maxCapacity;
    slimes_.clear();
    slimes_.resize(maxCapacity_);

    // モデルをロード
    Model* slimeModel = ModelManager::GetInstance()->Load(kSlimeModelPath);
    slimeModel->SetTexture(kSlimeTexturePath);

    for (auto& slime : slimes_) {
        slime.object = std::make_unique<Object3d>();
        slime.object->Initialize(Object3dManager::GetInstance());
        slime.object->SetModel(slimeModel);
        slime.object->SetEnableLighting(false); // 鮮やかなポップカラーにするためライティングOFF
        slime.object->SetTranslate({ 0.0f, -100.0f, 0.0f });
        slime.object->Update();
        slime.isActive = false;
    }

    // 起動時にすぐ上から降ってくるように自動スポーン
    SpawnRain(30, { 0.0f, 6.0f, 0.0f }, 3.5f);
}

void DeathSlimeShower::SpawnRain(uint32_t count, const Vector3& center, float spread)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> disSpread(-spread, spread);
    std::uniform_real_distribution<float> disHeight(0.0f, 4.0f);
    std::uniform_real_distribution<float> disVelX(-1.5f, 1.5f);
    std::uniform_real_distribution<float> disVelZ(-1.5f, 1.5f);
    std::uniform_real_distribution<float> disVelY(-4.0f, 1.0f);
    std::uniform_real_distribution<float> disScale(0.35f, 0.65f);
    std::uniform_real_distribution<float> disRotSpeed(-3.0f, 3.0f);
    std::uniform_real_distribution<float> disSpawnDelay(0.0f, spawnStaggerDuration);
    std::uniform_int_distribution<size_t> disColor(0, kSlimeColorCount - 1);

    uint32_t spawned = 0;
    for (auto& slime : slimes_) {
        if (!slime.isActive) {
            slime.isActive = true;
            slime.age = 0.0f;
            slime.spawnDelay = disSpawnDelay(gen);
            slime.isGrounded = false;
            
            // 上空ランダム位置にスポーン
            float spawnX = center.x + disSpread(gen);
            float spawnZ = center.z + disSpread(gen);
            if (centerExclusionHalfWidth > 0.0f &&
                centerExclusionHalfDepth > 0.0f &&
                std::abs(spawnX - center.x) < centerExclusionHalfWidth &&
                std::abs(spawnZ - center.z) < centerExclusionHalfDepth) {
                const float side = spawnX < center.x ? -1.0f : 1.0f;
                spawnX = center.x + side *
                    (centerExclusionHalfWidth + 0.25f + std::abs(disSpread(gen)) * 0.15f);
            }
            slime.position = {
                spawnX,
                center.y + disHeight(gen),
                spawnZ
            };

            // 初速
            slime.velocity = {
                disVelX(gen),
                disVelY(gen),
                disVelZ(gen)
            };

            slime.baseScale = disScale(gen);
            slime.squashY = 1.0f;
            slime.squashYVelocity = 0.0f;

            slime.rotation = { 0.0f, disSpread(gen) * 3.14f, 0.0f };
            slime.rotationSpeed = { disRotSpeed(gen) * 0.3f, disRotSpeed(gen), disRotSpeed(gen) * 0.3f };

            // カラフルな色
            slime.color = kSlimeColors[disColor(gen)];
            if (slime.object) {
                slime.object->SetColor(slime.color);
            }

            spawned++;
            if (spawned >= count) {
                break;
            }
        }
    }
}

void DeathSlimeShower::Update(float deltaTime)
{
    if (deltaTime <= 0.0f) return;
    // 物理シミュレーションの安定化のためサブステップ分割 (最大 1/60s 単位)
    const float maxDt = 1.0f / 60.0f;
    float remainingDt = std::min(deltaTime, 0.1f);

    while (remainingDt > 0.0f) {
        float dt = std::min(remainingDt, maxDt);
        remainingDt -= dt;

        for (auto& slime : slimes_) {
            if (!slime.isActive) continue;

            if (slime.spawnDelay > 0.0f) {
                slime.spawnDelay -= dt;
                continue;
            }

            slime.age += dt;
            if (slime.age >= slime.lifeTime) {
                slime.isActive = false;
                continue;
            }

            // --- 1. 重力と位置更新 ---
            slime.velocity.y += gravity * dt;
            slime.position.x += slime.velocity.x * dt;
            slime.position.y += slime.velocity.y * dt;
            slime.position.z += slime.velocity.z * dt;

            // 回転更新
            slime.rotation.x += slime.rotationSpeed.x * dt;
            slime.rotation.y += slime.rotationSpeed.y * dt;
            slime.rotation.z += slime.rotationSpeed.z * dt;

            // --- 2. 地面・床とのバウンス衝突 ---
            const float bottomRadius = slime.baseScale * 0.35f;
            const float currentFloor = floorY + bottomRadius;

            if (slime.position.y <= currentFloor) {
                slime.position.y = currentFloor;

                // 衝突前の落下速度で「グニャッ」と潰す (Squash)
                if (slime.velocity.y < -1.0f) {
                    float impactStrength = (std::min)(std::abs(slime.velocity.y) * 0.08f, 0.65f);
                    // 縦がギュッと縮む
                    slime.squashY -= impactStrength;
                    slime.squashYVelocity -= impactStrength * 25.0f;

                    // 上向きに跳ね返る
                    slime.velocity.y = -slime.velocity.y * restitution;
                    
                    // 回転にランダムな勢い
                    slime.rotationSpeed.y *= 0.8f;
                } else {
                    slime.velocity.y = 0.0f;
                    slime.isGrounded = true;
                }

                // 地面摩擦
                slime.velocity.x *= std::pow(friction, dt * 60.0f);
                slime.velocity.z *= std::pow(friction, dt * 60.0f);
                slime.rotationSpeed.x *= std::pow(0.85f, dt * 60.0f);
                slime.rotationSpeed.z *= std::pow(0.85f, dt * 60.0f);
            }

            // 壁（バウンディング範囲）との反射
            if (std::abs(slime.position.x) > boundaryExtent) {
                slime.position.x = (slime.position.x > 0 ? 1.0f : -1.0f) * boundaryExtent;
                slime.velocity.x = -slime.velocity.x * 0.7f;
            }
            if (std::abs(slime.position.z) > boundaryExtent) {
                slime.position.z = (slime.position.z > 0 ? 1.0f : -1.0f) * boundaryExtent;
                slime.velocity.z = -slime.velocity.z * 0.7f;
            }

            // 本や台座を包む簡易ボックスとの衝突。
            for (const CollisionBox& box : collisionBoxes_) {
                const float radius = slime.baseScale * 0.35f;
                const Vector3 minimum = {
                    box.center.x - box.halfExtent.x - radius,
                    box.center.y - box.halfExtent.y - radius,
                    box.center.z - box.halfExtent.z - radius
                };
                const Vector3 maximum = {
                    box.center.x + box.halfExtent.x + radius,
                    box.center.y + box.halfExtent.y + radius,
                    box.center.z + box.halfExtent.z + radius
                };
                if (slime.position.x < minimum.x || slime.position.x > maximum.x ||
                    slime.position.y < minimum.y || slime.position.y > maximum.y ||
                    slime.position.z < minimum.z || slime.position.z > maximum.z) {
                    continue;
                }

                const float distances[] = {
                    slime.position.x - minimum.x, maximum.x - slime.position.x,
                    slime.position.y - minimum.y, maximum.y - slime.position.y,
                    slime.position.z - minimum.z, maximum.z - slime.position.z
                };
                size_t nearestFace = 0;
                for (size_t face = 1; face < 6; ++face) {
                    if (distances[face] < distances[nearestFace]) nearestFace = face;
                }
                switch (nearestFace) {
                case 0: slime.position.x = minimum.x; slime.velocity.x = -std::abs(slime.velocity.x) * 0.55f; break;
                case 1: slime.position.x = maximum.x; slime.velocity.x = std::abs(slime.velocity.x) * 0.55f; break;
                case 2: slime.position.y = minimum.y; slime.velocity.y = -std::abs(slime.velocity.y) * 0.45f; break;
                case 3:
                    slime.position.y = maximum.y;
                    slime.velocity.y = std::abs(slime.velocity.y) * restitution;
                    slime.squashY -= 0.18f;
                    break;
                case 4: slime.position.z = minimum.z; slime.velocity.z = -std::abs(slime.velocity.z) * 0.55f; break;
                case 5: slime.position.z = maximum.z; slime.velocity.z = std::abs(slime.velocity.z) * 0.55f; break;
                }
            }

            // --- 3. ぽよぽよバネ物理 (Squash & Stretch Spring Vibration) ---
            // 理想の形 (target = 1.0f) に対するフックの法則 + ダンピング
            float displacement = slime.squashY - 1.0f;
            float springForce = -springStiffness * displacement - springDamping * slime.squashYVelocity;
            slime.squashYVelocity += springForce * dt;
            slime.squashY += slime.squashYVelocity * dt;

            // 制限 (極端な潰れや伸びを防ぐ)
            slime.squashY = (std::clamp)(slime.squashY, 0.35f, 1.8f);

            // --- 4. 体積保存 (Volume Preservation) による横幅の計算 ---
            // 縦が squashY 倍になったら、体積一定のため横 (X, Z) は 1 / sqrt(squashY) 倍に膨らむ
            float squashXZ = 1.0f / std::sqrt((std::max)(slime.squashY, 0.1f));

            Vector3 finalScale = {
                slime.baseScale * squashXZ,
                slime.baseScale * slime.squashY,
                slime.baseScale * squashXZ
            };

            // 描画オブジェクトへのTransform反映
            if (slime.object) {
                slime.object->SetTranslate(slime.position);
                slime.object->SetScale(finalScale);
                slime.object->SetRotate(slime.rotation);
                slime.object->Update();
            }
        }

        // スライム同士を球として押し戻し、同じ場所への重なりを防ぐ。
        for (size_t first = 0; first < slimes_.size(); ++first) {
            SlimeParticle& a = slimes_[first];
            if (!a.isActive || a.spawnDelay > 0.0f) continue;
            for (size_t second = first + 1; second < slimes_.size(); ++second) {
                SlimeParticle& b = slimes_[second];
                if (!b.isActive || b.spawnDelay > 0.0f) continue;
                const Vector3 delta = {
                    b.position.x - a.position.x,
                    b.position.y - a.position.y,
                    b.position.z - a.position.z
                };
                const float distanceSquared =
                    delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
                const float minimumDistance = (a.baseScale + b.baseScale) * 0.32f;
                if (distanceSquared <= 0.000001f ||
                    distanceSquared >= minimumDistance * minimumDistance) continue;

                const float distance = std::sqrt(distanceSquared);
                const float push = (minimumDistance - distance) * 0.5f;
                const Vector3 normal = {
                    delta.x / distance, delta.y / distance, delta.z / distance
                };
                a.position.x -= normal.x * push;
                a.position.y -= normal.y * push;
                a.position.z -= normal.z * push;
                b.position.x += normal.x * push;
                b.position.y += normal.y * push;
                b.position.z += normal.z * push;
                const float relativeVelocity =
                    (b.velocity.x - a.velocity.x) * normal.x +
                    (b.velocity.y - a.velocity.y) * normal.y +
                    (b.velocity.z - a.velocity.z) * normal.z;
                if (relativeVelocity < 0.0f) {
                    const float impulse = -relativeVelocity * 0.35f;
                    a.velocity.x -= normal.x * impulse;
                    a.velocity.y -= normal.y * impulse;
                    a.velocity.z -= normal.z * impulse;
                    b.velocity.x += normal.x * impulse;
                    b.velocity.y += normal.y * impulse;
                    b.velocity.z += normal.z * impulse;
                }
            }
        }
    }
}

void DeathSlimeShower::Draw()
{
    Object3dManager* objMgr = Object3dManager::GetInstance();
    objMgr->PreDraw();

    for (const auto& slime : slimes_) {
        if (slime.isActive && slime.spawnDelay <= 0.0f && slime.object) {
            slime.object->Draw();
        }
    }
}

void DeathSlimeShower::Clear()
{
    for (auto& slime : slimes_) {
        slime.isActive = false;
    }
}

uint32_t DeathSlimeShower::GetActiveCount() const
{
    uint32_t count = 0;
    for (const auto& slime : slimes_) {
        if (slime.isActive) count++;
    }
    return count;
}
