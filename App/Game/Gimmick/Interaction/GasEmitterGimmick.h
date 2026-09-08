/**
 * @file GasEmitterGimmick.h
 * @brief ガス発生装置の動作クラス
 */
#pragma once
#include "App/Game/Gimmick/BaseMapChipGimmick.h"
#include "App/Game/Gimmick/Interaction/GasEmitterParam.h"
#include "Engine/Effect/EffectManager.h"
#include <vector>
#include <memory>

class Object3d;
class MapChipStage;

/**
 * @brief 指定イベントを受信するとガスを発生し、着火されると爆発するギミック
 */
class GasEmitterGimmick : public BaseMapChipGimmick {
public:
    enum class State {
        Idle,       // 停止中
        Filling,    // 充満中（着火無効）
        Active,     // 充満完了（着火有効）
        Ignited,    // 着火済み（爆発待ち）
        Finished    // 爆発完了
    };

    struct GasEmitterState : public IGimmickState {
        State currentState;
    };

    GasEmitterGimmick();
    ~GasEmitterGimmick() override;

    bool Initialize(
        const Vector3& position,
        const std::string& texturePath,
        const BaseGimmickParam* gimmickParam = nullptr) override;

    void Update() override;
    void Draw() override;
    void SetEditorMode(bool isEditorMode) override;
    
    AABB GetAABB() const override;
    void SetStage(MapChipStage* stage) override;
    bool IsSolid() const override { return false; }
    
    std::shared_ptr<IGimmickState> CreateSnapshot() const override;
    void RestoreFromSnapshot(const IGimmickState* state) override;

    void OnSpark(const Vector3& origin) override;

    /**
     * @brief ガスエリア（範囲）を取得する
     * @return ガスの範囲を表すAABB
     */
    AABB GetGasAABB() const;
    
    /**
     * @brief ガスを放出中かどうか
     */
    bool IsEmitting() const { return currentState_ != State::Idle && currentState_ != State::Finished; }

private:
    void StartEmitting();
    void ChangeState(State nextState);
    void StartParticles();
    void StopParticles();
    void UpdateParticles();

private:
    std::unique_ptr<Object3d> object_;
    std::unique_ptr<GasEmitterParam> param_;
    MapChipStage* stage_;
    
    Vector3 position_;
    Vector3 size_;
    
    bool isEditorMode_;
    
    // 状態管理
    State currentState_ = State::Idle;
    float stateTimer_ = 0.0f;
    
    // エフェクト管理
    std::vector<EffectHandle> effectHandles_;
    // 煙が上に昇る性質を考慮し、発生源をブロックの中心より下（-0.5）に設定します。
    // （これまでは +0.5 だったため、上に1ブロック分ズレているように見えていました）
    Vector3 particleOffset_ = { 0.0f, -0.5f, 0.0f };
};
