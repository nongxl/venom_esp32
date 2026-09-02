#include "RhythmDetector.h"
#include <cmath>

RhythmDetector::RhythmDetector() {
    init();
}

void RhythmDetector::init() {
    for (int i = 0; i < 4; ++i) tap_history[i] = 0;
    tap_count = 0;
    last_tap_time = 0;
    ready_to_mimic = false;
    debounce_timer = 0.0f;

    for (int i = 0; i < 6; ++i) beat_history[i] = 0;
    beat_count = 0;
    last_beat_time = 0;
    rolling_bass_avg = 0.15f;
    music_confidence = 0.0f;
    avg_beat_interval_ms = 500;
    beat_phase = 0.0f;
    pending_music_note = false;
    note_trigger_cooldown = 0.0f;
}

void RhythmDetector::registerTap(unsigned long now) {
    if (tap_count > 0 && (now - last_tap_time > 1800)) {
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
        evaluateTapPattern();
    }
}

void RhythmDetector::evaluateTapPattern() {
    if (tap_count < 3) return;

    unsigned long d1 = tap_history[tap_count - 2] - tap_history[tap_count - 3];
    unsigned long d2 = tap_history[tap_count - 1] - tap_history[tap_count - 2];

    if (d1 >= 250 && d1 <= 950 && d2 >= 250 && d2 <= 950) {
        float ratio = (float)d1 / (float)d2;
        if (ratio >= 0.75f && ratio <= 1.35f) {
            ready_to_mimic = true;
            mimic_interval_ms = (d1 + d2) / 2;
            mimic_tap_count = 2 + (rand() % 2);
            tap_count = 0;
        }
    }
}

void RhythmDetector::feedAudioBeat(float low_band_energy, float mid_band_energy, float total_db, unsigned long now_ms) {
    // 追踪低频基线能量
    rolling_bass_avg = rolling_bass_avg * 0.90f + low_band_energy * 0.10f;
    if (rolling_bass_avg < 0.04f) rolling_bass_avg = 0.04f;

    // 重音判断：低频能量高于滑动均值 38% 且分贝大于 45dB
    float threshold = rolling_bass_avg * 1.38f + 0.10f;
    bool is_onset = (low_band_energy > threshold) && (total_db > 45.0f);

    if (is_onset) {
        if (last_beat_time == 0 || (now_ms - last_beat_time >= 220)) {
            // 记录有效鼓点 (最小去抖 220ms，对应最大 270 BPM)
            if (beat_count < 6) {
                beat_history[beat_count++] = now_ms;
            } else {
                for (int i = 0; i < 5; ++i) beat_history[i] = beat_history[i + 1];
                beat_history[5] = now_ms;
            }
            last_beat_time = now_ms;

            evaluateMusicBeats();
        }
    }
}

void RhythmDetector::evaluateMusicBeats() {
    if (beat_count < 4) return;

    // 计算最近连续 3 个拍子时间差
    unsigned long intervals[3];
    int count = 0;
    for (int i = beat_count - 1; i >= beat_count - 3 && i > 0; --i) {
        intervals[count++] = beat_history[i] - beat_history[i - 1];
    }

    // 检查拍子间隔是否落在常见音乐速度 (240ms ~ 1100ms 对应 55 ~ 250 BPM)
    bool all_valid = true;
    for (int i = 0; i < count; ++i) {
        if (intervals[i] < 240 || intervals[i] > 1100) {
            all_valid = false;
            break;
        }
    }

    if (all_valid && count >= 2) {
        float r1 = (float)intervals[0] / (float)intervals[1];
        // 允许标准拍 (1:1) 或切分双倍拍 (1:2 / 2:1)
        bool is_steady = (r1 >= 0.78f && r1 <= 1.28f) || (r1 >= 1.75f && r1 <= 2.30f) || (r1 >= 0.42f && r1 <= 0.58f);

        if (is_steady) {
            music_confidence = std::min(1.0f, music_confidence + 0.38f);
            avg_beat_interval_ms = (intervals[0] + intervals[1]) / 2;
            if (avg_beat_interval_ms > 1000) avg_beat_interval_ms /= 2;

            if (music_confidence >= 0.65f && note_trigger_cooldown <= 0.0f) {
                pending_music_note = true;
                note_trigger_cooldown = 4.0f + (rand() % 25) * 0.1f; // 每 4.0~6.5 秒喷出一个音乐符号
            }
            return;
        }
    }

    music_confidence = std::max(0.0f, music_confidence - 0.12f);
}

void RhythmDetector::update(float dt, float imu_shake, float audio_high) {
    debounce_timer -= dt;
    if (note_trigger_cooldown > 0.0f) {
        note_trigger_cooldown -= dt;
    }

    // 音乐置信度自然缓慢衰减
    if (millis() - last_beat_time > 1800) {
        music_confidence = std::max(0.0f, music_confidence - dt * 0.35f);
    }

    // 律动相位更新 (随检测到的 BPM 周期 0.0 ~ 1.0 循环，用于身体弹性起伏)
    if (avg_beat_interval_ms > 0) {
        float beat_sec = (float)avg_beat_interval_ms * 0.001f;
        beat_phase += dt / beat_sec;
        if (beat_phase > 1.0f) beat_phase -= std::floor(beat_phase);
    }

    // 敲击去抖特征
    if (debounce_timer <= 0.0f && (imu_shake > 0.65f || audio_high > 0.82f)) {
        registerTap(millis());
        debounce_timer = 0.18f;
    }
}

float RhythmDetector::getDetectedBPM() const {
    if (avg_beat_interval_ms <= 0) return 120.0f;
    return 60000.0f / (float)avg_beat_interval_ms;
}

bool RhythmDetector::checkAndConsumeMusicNoteEvent() {
    if (pending_music_note) {
        pending_music_note = false;
        return true;
    }
    return false;
}

