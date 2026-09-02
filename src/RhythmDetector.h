#pragma once
#include <Arduino.h>

class RhythmDetector {
public:
    RhythmDetector();

    void init();
    void update(float dt, float imu_shake, float audio_high);
    void feedAudioBeat(float low_band_energy, float mid_band_energy, float total_db, unsigned long now_ms);

    // 查询是否有待模仿的敲击节拍序列
    bool hasRhythmToMimic() const { return ready_to_mimic; }
    int getMimicTapCount() const { return mimic_tap_count; }
    unsigned long getMimicIntervalMs() const { return mimic_interval_ms; }
    void consumeMimicEvent() { ready_to_mimic = false; }

    // 音乐规律律动感知
    bool isMusicPlaying() const { return music_confidence >= 0.65f; }
    float getMusicConfidence() const { return music_confidence; }
    float getDetectedBPM() const;
    float getBeatPhase() const { return beat_phase; }
    bool checkAndConsumeMusicNoteEvent();

private:
    // 敲击节奏
    unsigned long tap_history[4];
    int tap_count = 0;
    unsigned long last_tap_time = 0;
    bool ready_to_mimic = false;
    unsigned long mimic_interval_ms = 450;
    int mimic_tap_count = 2;
    float debounce_timer = 0.0f;

    // 音乐音频节拍追踪器 (Beat Tracker)
    float rolling_bass_avg = 0.15f;
    unsigned long last_beat_time = 0;
    unsigned long beat_history[6];
    int beat_count = 0;
    float music_confidence = 0.0f;
    unsigned long avg_beat_interval_ms = 500;
    float beat_phase = 0.0f; // 0.0 ~ 1.0 节拍相位，用于身体弹性律动
    bool pending_music_note = false;
    float note_trigger_cooldown = 0.0f;

    void registerTap(unsigned long now);
    void evaluateTapPattern();
    void evaluateMusicBeats();
};
