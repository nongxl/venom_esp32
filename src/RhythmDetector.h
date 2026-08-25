#pragma once
#include <Arduino.h>

class RhythmDetector {
public:
    RhythmDetector();

    void init();
    void update(float dt, float imu_shake, float audio_high);

    // 查询是否有待模仿的节拍序列
    bool hasRhythmToMimic() const { return ready_to_mimic; }
    int getMimicTapCount() const { return mimic_tap_count; }
    unsigned long getMimicIntervalMs() const { return mimic_interval_ms; }

    // 消耗节拍事件
    void consumeMimicEvent() { ready_to_mimic = false; }

private:
    unsigned long tap_history[4];
    int tap_count = 0;
    unsigned long last_tap_time = 0;

    bool ready_to_mimic = false;
    unsigned long mimic_interval_ms = 450;
    int mimic_tap_count = 2;
    float debounce_timer = 0.0f;

    void registerTap(unsigned long now);
    void evaluatePattern();
};
