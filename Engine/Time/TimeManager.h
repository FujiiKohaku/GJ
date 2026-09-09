#pragma once

#include <chrono>
#include <cstdint>

class TimeManager {
public:
    static TimeManager* GetInstance();

    void Initialize();
    void Update();

    float GetDeltaTime() const { return deltaTime_; }
    float GetUnscaledDeltaTime() const { return unscaledDeltaTime_; }
    float GetTimeScale() const { return timeScale_; }
    float GetMaxDeltaTime() const { return maxDeltaTime_; }
    double GetTotalTime() const { return totalTime_; }
    double GetUnscaledTotalTime() const { return unscaledTotalTime_; }
    uint64_t GetFrameCount() const { return frameCount_; }

    void SetTimeScale(float timeScale);
    void SetMaxDeltaTime(float maxDeltaTime);

    // Restore render-frame time automatically after a deterministic online tick.
    class SimulationStep {
    public:
        explicit SimulationStep(float seconds) : time_(*GetInstance()),
            delta_(time_.deltaTime_), unscaled_(time_.unscaledDeltaTime_) {
            time_.deltaTime_ = time_.unscaledDeltaTime_ = seconds;
        }
        SimulationStep(float scaledSeconds, float unscaledSeconds) : time_(*GetInstance()),
            delta_(time_.deltaTime_), unscaled_(time_.unscaledDeltaTime_) {
            time_.deltaTime_ = scaledSeconds;
            time_.unscaledDeltaTime_ = unscaledSeconds;
        }
        ~SimulationStep() { time_.deltaTime_ = delta_; time_.unscaledDeltaTime_ = unscaled_; }
        SimulationStep(const SimulationStep&) = delete;
        SimulationStep& operator=(const SimulationStep&) = delete;
    private:
        TimeManager& time_;
        float delta_, unscaled_;
    };

private:
    TimeManager() = default;

    using Clock = std::chrono::steady_clock;

    Clock::time_point previousTime_ {};
    float deltaTime_ = 0.0f;
    float unscaledDeltaTime_ = 0.0f;
    float timeScale_ = 1.0f;
    float maxDeltaTime_ = 0.1f;
    double totalTime_ = 0.0;
    double unscaledTotalTime_ = 0.0;
    uint64_t frameCount_ = 0;
    bool isInitialized_ = false;
};
