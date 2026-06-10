#ifndef NEURAL_CORE_H
#define NEURAL_CORE_H

#include <M5Unified.h>

enum class NeuralState {
    Idle,
    Listening,
    Transmitting,
    Thinking,
    Streaming,
    Complete,
    Offline
};

struct NeuralSpine {
    float angle;        // 原始角度
    float length;       // 当前长度
    float targetLength; // 目标长度
    float velocity;     // 速度
};

class NeuralCore {
public:
    NeuralCore();
    
    // 核心生命周期
    void update(float soundLevel, float lux, bool isWifiConnected);
    void draw(M5Canvas* canvas, int cx, int cy);
    
    // 状态事件通知
    void notifyAISyncStarted();
    void notifyAIResponseReceived();
    
    // 状态读写
    void setState(NeuralState newState);
    NeuralState getState() const { return currentState; }

private:
    NeuralState currentState;
    NeuralSpine spines[32];
    
    // 核心球体物理状态
    float coreRadius;
    float targetCoreRadius;
    float coreVelocity;

    // 波纹扩散系统 (用于 Streaming 状态下的从核心向外传导波纹)
    float rippleProgress;
    uint32_t lastTokenTime;
    int tokenPulseCount;

    // Thinking 状态下的神经簇重组
    uint32_t lastClusterRegenTime;
    float clusterSeed;

    // 状态定时器
    uint32_t stateTimer;
    uint32_t lastUpdateTime;

    // 动力学常量
    static constexpr float SPRING_SPINE = 0.14f;  // 尖刺弹簧系数
    static constexpr float DAMPING_SPINE = 0.82f; // 尖刺阻尼系数
    static constexpr float SPRING_CORE = 0.18f;   // 核心球体弹簧系数
    static constexpr float DAMPING_CORE = 0.80f;  // 核心球体阻尼系数
    static constexpr float BASE_CORE_R = 4.5f;    // 核心基准半径
    static constexpr float BASE_SPINE_L = 3.5f;   // 尖刺基准长度
};

#endif // NEURAL_CORE_H
