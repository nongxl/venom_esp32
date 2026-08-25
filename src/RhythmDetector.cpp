#include "RhythmDetector.h"
#include <cmath>

RhythmDetector::RhythmDetector() {
    for (int i = 0; i < 4; ++i) tap_history[i] = 0;
}

void RhythmDetector::init() {
    for (int i = 0; i < 4; ++i) tap_history[i] = 0;
    tap_count = 0;
    last_tap_time = 0;
    ready_to_mimic = false;
    debounce_timer = 0.0f;
}

void RhythmDetector::registerTap(unsigned long now) {
    if (tap_count > 0 && (now - last_tap_time > 1800)) {
        // 超时重置
        tap_count = 0;
    }

    if (tap_count < 4) {
        tap_history[tap_count++] = now;
    } else {
        tap_history[0] = tap_history[1];
        tap_history[1] = tap_history[2];
        tap_history[2] = tap_history[3];
        tap_history[3] = now;
    }
    last_tap_time = now;

    if (tap_count >= 3) {
        evaluatePattern();
    }
}

void RhythmDetector::evaluatePattern() {
    if (tap_count < 3) return;

    unsigned long d1 = tap_history[tap_count - 2] - tap_history[tap_count - 3];
    unsigned long d2 = tap_history[tap_count - 1] - tap_history[tap_count - 2];

    if (d1 >= 250 && d1 <= 950 && d2 >= 250 && d2 <= 950) {
        float ratio = (float)d1 / (float)d2;
        if (ratio >= 0.75f && ratio <= 1.35f) {
            ready_to_mimic = true;
            mimic_interval_ms = (d1 + d2) / 2;
            mimic_tap_count = 2 + (rand() % 2);
            tap_count = 0; // 成功匹配后复位
        }
    }
}

void RhythmDetector::update(float dt, float imu_shake, float audio_high) {
    debounce_timer -= dt;

    // 敲击特征：短促高频冲击
    if (debounce_timer <= 0.0f && (imu_shake > 0.65f || audio_high > 0.82f)) {
        registerTap(millis());
        debounce_timer = 0.18f; // 去抖 180ms
    }
}
