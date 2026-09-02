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
    rolling_bass_avg = rolling_bass_avg * 0.92f + low_band_energy * 0.08f;
    if (rolling_bass_avg < 0.04f) rolling_bass_avg = 0.04f;

    // 超时重置节拍序列 (超过 1300ms 无新鼓点，重置计数)
    if (last_beat_time > 0 && (now_ms - last_beat_time > 1300)) {
        beat_count = 0;
    }

    // 严谨音乐重音判断：
    // 1. 环境总音量必须达到 55.0dB (避开人声低语与环境底噪)
    // 2. 低频鼓点能量必须显著跃升 (> 均值 60% 且绝对值 > 0.28)
    float threshold = rolling_bass_avg * 1.60f + 0.16f;
    bool is_onset = (low_band_energy > threshold) && (low_band_energy > 0.28f) && (total_db > 55.0f);

    if (is_onset) {
        if (last_beat_time == 0 || (now_ms - last_beat_time >= 260)) {
            // 记录有效鼓点 (最小去抖 260ms，对应最大 230 BPM)
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
    // 必须捕获至少 5 个连续鼓点（即 4 个连续时间间隔 IBI）
    if (beat_count < 5) return;

    // 提取最近连续 4 个节拍时间差
    unsigned long intervals[4];
    unsigned long sum_intervals = 0;
    for (int i = 0; i < 4; ++i) {
        int idx = beat_count - 1 - i;
        intervals[i] = beat_history[idx] - beat_history[idx - 1];
        sum_intervals += intervals[i];
    }

    // 1. 速度区间检查：严格限定在标准音乐常见速度 (280ms ~ 850ms 对应 70 ~ 214 BPM)
    for (int i = 0; i < 4; ++i) {
        if (intervals[i] < 280 || intervals[i] > 850) {
            music_confidence = std::max(0.0f, music_confidence - 0.30f);
            return;
        }
    }

    // 2. 4 拍等间隔严谨性检验 (四分音符/八分音符稳定节奏)：
    // 计算平均拍子间隔与最大偏离度，最大偏离必须 <= 平均值的 12% (严禁人声杂音误触发！)
    float avg_ibi = (float)sum_intervals / 4.0f;
    float max_dev = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float dev = std::abs((float)intervals[i] - avg_ibi);
        if (dev > max_dev) max_dev = dev;
    }

    if (max_dev <= avg_ibi * 0.12f) {
        // 连续 4 拍高度吻合音乐节拍律动！置信度大幅攀升
        music_confidence = std::min(1.0f, music_confidence + 0.35f);
        avg_beat_interval_ms = (unsigned long)avg_ibi;

        // 仅当高置信度 (>=0.88) 且冷却完毕时，才由身体表皮破空喷出音符
        if (music_confidence >= 0.88f && note_trigger_cooldown <= 0.0f) {
            pending_music_note = true;
            note_trigger_cooldown = 7.0f + (rand() % 30) * 0.1f; // 音乐播放期间每 7.0~10.0 秒优雅吐出一枚音符
        }
    } else {
        // 拍子不规律（如人声说话或环境偶发噪声），置信度迅速衰减
        music_confidence = std::max(0.0f, music_confidence - 0.25f);
    }
}

void RhythmDetector::update(float dt, float imu_shake, float audio_high) {
    debounce_timer -= dt;
    if (note_trigger_cooldown > 0.0f) {
        note_trigger_cooldown -= dt;
    }

    // 音乐停播后置信度迅速归零 (1.4s 内完全衰减)
    if (millis() - last_beat_time > 1300) {
        music_confidence = std::max(0.0f, music_confidence - dt * 0.65f);
        beat_count = 0;
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

