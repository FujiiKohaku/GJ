/**
 * @file GasEmitterGimmick.cpp
 * @brief ガス発生装置の実装
 */
#include "GasEmitterGimmick.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/3D/Object3d.h"
#include "App/Game/Map/MapChipStage.h"
#include "Engine/Logger/Logger.h"
#include "Engine/LevelEditor/GimmickMetaDataManager.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Player/MapChipPlayer.h"
#include "Engine/CollisionManager/CollisionManager.h"

namespace {
    const float kFillDelay = 1.0f;     // ガスが充満するまでのディレイ
    const float kIgnitionDelay = 0.5f; // 引火してから爆発するまでのディレイ
}

GasEmitterGimmick::GasEmitterGimmick()
    : stage_(nullptr)
    , position_({0, 0, 0})
    , size_({1, 1, 1})
    , isEditorMode_(false)
{
}

GasEmitterGimmick::~GasEmitterGimmick()
{
    StopAllParticles();
}

bool GasEmitterGimmick::Initialize(
    const Vector3& position,
    const std::string& texturePath,
    const BaseGimmickParam* gimmickParam)
{
    position_ = position;

    if (gimmickParam) {
        param_ = std::unique_ptr<GasEmitterParam>(static_cast<GasEmitterParam*>(gimmickParam->Clone().release()));
    } else {
        param_ = std::make_unique<GasEmitterParam>();
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());

    std::string finalModelPath = texturePath;
    if (const auto* metaData = GimmickMetaDataManager::GetInstance()->GetMetaData("GasEmitter")) {
        finalModelPath = metaData->defaultModelPath;
    }

    if (!finalModelPath.empty()) {
        ModelManager::GetInstance()->Load(finalModelPath);
        object_->SetModel(finalModelPath);
    } else {
        Model* model = ModelManager::GetInstance()->CreateCube();
        if (model) {
            object_->SetModel(model);
        }
    }

    object_->SetTranslate(position_);
    object_->SetScale(size_);
    object_->SetEnableLighting(true);
    object_->Update();

    return true;
}

std::shared_ptr<IGimmickState> GasEmitterGimmick::CreateSnapshot() const
{
    auto state = std::make_shared<GasEmitterState>();
    state->currentState = currentState_;
    return state;
}

void GasEmitterGimmick::RestoreFromSnapshot(const IGimmickState* state)
{
    if (const auto* gasState = dynamic_cast<const GasEmitterState*>(state)) {
        currentState_ = gasState->currentState;
        
        if (currentState_ == State::Active) {
            stateTimer_ = 0.0f;
            StopAllParticles();
            StartCloudParticles();
        } else if (currentState_ == State::Finished) {
            StopAllParticles();
        } else if (currentState_ == State::Ignited) {
            // Ignitedのままスナップショットが取られることはほぼ無いが、フェイルセーフとしてActiveに戻す
            currentState_ = State::Active;
            stateTimer_ = 0.0f;
            StopAllParticles();
            StartCloudParticles();
        } else if (currentState_ == State::Idle) {
            StopAllParticles();
        }
    }
}

void GasEmitterGimmick::Update()
{
    if (object_) {
        object_->Update();
    }

    if (isEditorMode_) {
        StopAllParticles(); // エディタモードに切り替わったら再生停止
        return;
    }

    // FSM Update
    if (currentState_ != State::Idle && currentState_ != State::Finished) {
        stateTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        UpdateParticles();

        // 充満中(Filling)、充満完了(Active)、着火済み(Ignited)の間はプレイヤーを死亡させる
        if (stage_ && stage_->GetPlayer()) {
            AABB playerBox = stage_->GetPlayer()->GetAABB();
            AABB gasBox = GetGasAABB();
            if (CollisionManager::Intersect(playerBox, gasBox).isHit) {
                stage_->GetPlayer()->Kill();
            }
        }

        switch (currentState_) {
        case State::Filling:
            if (stateTimer_ >= kFillDelay) {
                Logger::Log("GasEmitterGimmick: stateTimer_ reached kFillDelay, State -> Active\n");
                ChangeState(State::Active);
            }
            break;
        case State::Ignited:
            if (stateTimer_ >= kIgnitionDelay) {
                Logger::Log("GasEmitterGimmick: stateTimer_ reached kIgnitionDelay, State -> Finished (Exploding)\n");
                ChangeState(State::Finished);
            }
            break;
        default:
            break;
        }
    }
}

void GasEmitterGimmick::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

void GasEmitterGimmick::SetEditorMode(bool isEditorMode)
{
    isEditorMode_ = isEditorMode;
}

AABB GasEmitterGimmick::GetAABB() const
{
    AABB aabb;
    aabb.center = position_;
    aabb.size = size_;
    return aabb;
}

AABB GasEmitterGimmick::GetGasAABB() const
{
    AABB aabb;
    if (param_) {
        float sizeX = (param_->leftBlocks_ + param_->rightBlocks_ + 1) * 1.0f;
        float sizeY = (param_->downBlocks_ + param_->upBlocks_ + 1) * 1.0f;
        float sizeZ = 1.0f;
        
        float centerX = position_.x + (static_cast<float>(param_->rightBlocks_) - static_cast<float>(param_->leftBlocks_)) * 0.5f;
        float centerY = position_.y + (static_cast<float>(param_->upBlocks_) - static_cast<float>(param_->downBlocks_)) * 0.5f;
        
        aabb.center = {centerX, centerY, position_.z};
        aabb.size = {sizeX, sizeY, sizeZ};
    } else {
        aabb.center = position_;
        aabb.size = {3.0f, 3.0f, 3.0f}; // フォールバック
    }
    return aabb;
}

void GasEmitterGimmick::SetStage(MapChipStage* stage)
{
    stage_ = stage;
    if (stage_ && param_) {
        if (!param_->listenEventName_.empty()) {
            stage_->GetEventManager().Subscribe(param_->listenEventName_, [this]() {
                StartEmitting();
            });
        }
    }
}

void GasEmitterGimmick::StartEmitting()
{
    if (currentState_ != State::Idle) return;
    Logger::Log("GasEmitterGimmick: Started emitting gas, State -> Filling\n");
    ChangeState(State::Filling);
}

void GasEmitterGimmick::ChangeState(State nextState)
{
    if (currentState_ == nextState) return;
    currentState_ = nextState;
    stateTimer_ = 0.0f;

    switch (currentState_) {
    case State::Idle:
        StopAllParticles();
        break;
    case State::Filling:
        StartBurstParticles();
        StartCloudParticles(); // 充満開始時に両方同時に出し始めることで隙間を無くす
        break;
    case State::Active:
        // 充満完了、着火待ち（勢いのある噴き出しだけを停止し、滞留用はそのまま維持）
        StopBurstParticles();
        break;
    case State::Ignited:
        // 引火演出を追加する場合はここに記述
        break;
    case State::Finished:
        // 爆発を発生させる
        if (param_) {
            stage_->CreateExplosionGrid(position_, param_->leftBlocks_, param_->rightBlocks_, param_->upBlocks_, param_->downBlocks_);
        } else {
            stage_->CreateExplosion(position_, 3.0f);
        }
        StopAllParticles(); // 爆発と同時にエフェクト停止
        
        // 視覚的な爆発エフェクト（Explosion）を再生する（Volume Matching）
        if (param_) {
            EffectManager* effects = EffectManager::GetInstance();
            for (int y = -static_cast<int>(param_->downBlocks_); y <= static_cast<int>(param_->upBlocks_); ++y) {
                for (int x = -static_cast<int>(param_->leftBlocks_); x <= static_cast<int>(param_->rightBlocks_); ++x) {
                    Vector3 offset = {
                        static_cast<float>(x) * 1.0f,
                        static_cast<float>(y) * 1.0f,
                        0.0f
                    };
                    Vector3 source = position_ + particleOffset_ + offset;
                    effects->PlayEffect("Explosion", source);
                }
            }
        } else {
            EffectManager::GetInstance()->PlayEffect("Explosion", position_);
        }
        break;
    }
}

void GasEmitterGimmick::OnSpark(const Vector3& origin)
{
    // ガスが充満している (Active) 状態の時のみ着火を受け付ける
    if (currentState_ != State::Active || !stage_ || isEditorMode_) return;

    // スパーク座標が自分のガスエリアに入っているか判定する
    AABB gasArea = GetGasAABB();
    Vector3 diff = origin - gasArea.center;
    // 簡単のため AABB と 点(origin) の内包判定
    bool inRangeX = std::abs(diff.x) <= gasArea.size.x * 0.5f;
    bool inRangeY = std::abs(diff.y) <= gasArea.size.y * 0.5f;
    bool inRangeZ = std::abs(diff.z) <= gasArea.size.z * 0.5f;

    if (inRangeX && inRangeY && inRangeZ) {
        // ガスに引火
        Logger::Log("GasEmitterGimmick: Spark hit gas area! State -> Ignited\n");
        ChangeState(State::Ignited);
    }
}

void GasEmitterGimmick::StartBurstParticles()
{
    if (!burstEffectHandles_.empty()) { return; }
    if (!param_) { return; }
    EffectManager* effects = EffectManager::GetInstance();
    
    // ガスが充満する設定範囲（上下左右のブロック数）をループし、
    // 各ブロックの中心座標にエフェクト（Smoke）を敷き詰めます。
    // ブロックサイズは 1.0f として計算します。
    for (int y = -static_cast<int>(param_->downBlocks_); y <= static_cast<int>(param_->upBlocks_); ++y) {
        for (int x = -static_cast<int>(param_->leftBlocks_); x <= static_cast<int>(param_->rightBlocks_); ++x) {
            Vector3 offset = {
                static_cast<float>(x) * 1.0f,
                static_cast<float>(y) * 1.0f,
                0.0f
            };
            // 噴き出し用は下から上へ飛ぶためオフセットを下げる
            Vector3 source = position_ + particleOffset_ + offset;
            
            EffectHandle handle = effects->PlayLoopEffect("PoisonGas", source);
            if (handle != kInvalidEffectHandle) {
                burstEffectHandles_.push_back(handle);
            }
        }
    }
}

void GasEmitterGimmick::StartCloudParticles()
{
    if (!cloudEffectHandles_.empty()) { return; }
    if (!param_) { return; }
    EffectManager* effects = EffectManager::GetInstance();
    
    for (int y = -static_cast<int>(param_->downBlocks_); y <= static_cast<int>(param_->upBlocks_); ++y) {
        for (int x = -static_cast<int>(param_->leftBlocks_); x <= static_cast<int>(param_->rightBlocks_); ++x) {
            Vector3 offset = {
                static_cast<float>(x) * 1.0f,
                static_cast<float>(y) * 1.0f,
                0.0f
            };
            // 滞留用はブロックの中心にピッタリ出すためオフセット無し
            Vector3 source = position_ + offset;
            
            EffectHandle handle = effects->PlayLoopEffect("PoisonGasCloud", source);
            if (handle != kInvalidEffectHandle) {
                cloudEffectHandles_.push_back(handle);
            }
        }
    }
}

void GasEmitterGimmick::StopBurstParticles()
{
    if (!burstEffectHandles_.empty()) {
        EffectManager* effects = EffectManager::GetInstance();
        for (EffectHandle handle : burstEffectHandles_) {
            effects->StopEffect(handle);
        }
        burstEffectHandles_.clear();
    }
}

void GasEmitterGimmick::StopCloudParticles()
{
    if (!cloudEffectHandles_.empty()) {
        EffectManager* effects = EffectManager::GetInstance();
        for (EffectHandle handle : cloudEffectHandles_) {
            effects->StopEffect(handle);
        }
        cloudEffectHandles_.clear();
    }
}

void GasEmitterGimmick::StopAllParticles()
{
    StopBurstParticles();
    StopCloudParticles();
}

void GasEmitterGimmick::UpdateParticles()
{
    if (!param_) { return; }
    EffectManager* effects = EffectManager::GetInstance();
    
    auto updateHandles = [&](const std::vector<EffectHandle>& handles, const Vector3& globalOffset) {
        if (handles.empty()) { return; }
        int index = 0;
        for (int y = -static_cast<int>(param_->downBlocks_); y <= static_cast<int>(param_->upBlocks_); ++y) {
            for (int x = -static_cast<int>(param_->leftBlocks_); x <= static_cast<int>(param_->rightBlocks_); ++x) {
                if (index >= handles.size()) { break; }
                EffectHandle handle = handles[index++];
                
                if (effects->IsEffectAlive(handle)) {
                    Vector3 offset = {
                        static_cast<float>(x) * 1.0f,
                        static_cast<float>(y) * 1.0f,
                        0.0f
                    };
                    Vector3 source = position_ + globalOffset + offset;
                    effects->SetEffectPosition(handle, source);
                }
            }
        }
    };

    updateHandles(burstEffectHandles_, particleOffset_);
    updateHandles(cloudEffectHandles_, {0.0f, 0.0f, 0.0f});
}
