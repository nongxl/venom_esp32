#pragma once
#include <Arduino.h>
#include "config.h"

class PhysiologySystem {
public:
    PhysiologySystem();

    void init();
    void update(float dt, float imu_shake, bool is_upside_down, bool btn_pressed);

    // 注入麦克风实时环境分贝数 (dB)
    void feedMicDecibels(float db) {
        current_mic_db = db;
        // 声控激发强度 [0.0, 1.0] (从 48dB 开始灵敏感知，80dB 达满载，适配音乐与人声)
        sound_excitation = std::max(0.0f, std::min(1.0f, (db - 48.0f) / 32.0f));
    }
    float getMicDecibels() const { return current_mic_db; }
    float getSoundExcitation() const { return sound_excitation; }

    // 综合音乐与节拍鼓点脉冲能量 (0.0 ~ 1.0)
    float getMusicBeatPulse() const {
        float beat = std::max(raw_audio_low * 1.35f, raw_audio_mid * 0.85f);
        return std::max(beat, sound_excitation * 0.95f);
    }

    // 触发外界直接刺激
    void triggerShock(float amount);

    void applyStimulus(float stress_delta, float comfort_delta) {
        if (stress_delta > 0.0f) {
            stress = std::min(1.0f, stress + stress_delta);
            neuro_tension = std::min(1.0f, neuro_tension + stress_delta);
        }
        if (comfort_delta > 0.0f) {
            comfort = std::min(1.0f, comfort + comfort_delta);
            curiosity = std::min(1.0f, curiosity + comfort_delta * 0.5f);
        }
        updateEmotionState();
    }

    // 捕食进食补充
    void feed(float energy_amount = 0.30f) {
        energy = std::min(1.0f, energy + energy_amount);
        comfort = std::min(1.0f, comfort + 0.20f);
        stress = std::max(0.0f, stress - 0.15f);
    }

    // 体力消耗与睡眠恢复
    void consumeEnergy(float amount) {
        energy = std::max(0.05f, energy - amount);
    }
    void recoverEnergy(float amount) {
        energy = std::min(1.0f, energy + amount);
    }

    // 获取六维生理心理参数
    float getEnergy()     const { return energy; }
    float getStress()     const { return stress; }
    float getCuriosity()  const { return curiosity; }
    float getComfort()    const { return comfort; }
    float getAttachment() const { return attachment; }
    float getBoredom()    const { return boredom; }

    // 无聊度动态控制
    void addBoredom(float amount) { boredom = std::min(1.0f, boredom + amount); }
    void reduceBoredom(float amount) { boredom = std::max(0.0f, boredom - amount); }
    void resetBoredom() { boredom = 0.0f; }

    // 获取当前主导情绪
    EmotionState getEmotion() const { return current_emotion; }
    const char* getEmotionName() const;

    // 5 频段专业音乐频谱均衡器 (5-Band Spectrum Equalizer)
    // 0: Sub-Bass (60~200Hz), 1: Bass (250~600Hz), 2: Mid (700~1600Hz), 3: Presence (1800~2800Hz), 4: Treble (3000~4000Hz)
    void feedSpectrumBands(float b0, float b1, float b2, float b3, float b4) {
        raw_spectrum_bands[0] = b0;
        raw_spectrum_bands[1] = b1;
        raw_spectrum_bands[2] = b2;
        raw_spectrum_bands[3] = b3;
        raw_spectrum_bands[4] = b4;
        raw_audio_low  = std::max(b0, b1);
        raw_audio_mid  = b2;
        raw_audio_high = std::max(b3, b4);
    }
    void feedAudioBands(float low, float mid, float high) {
        raw_audio_low = low;
        raw_audio_mid = mid;
        raw_audio_high = high;
        raw_spectrum_bands[0] = low * 1.1f;
        raw_spectrum_bands[1] = low * 0.9f;
        raw_spectrum_bands[2] = mid;
        raw_spectrum_bands[3] = high * 0.8f;
        raw_spectrum_bands[4] = high * 1.2f;
    }
    float getSpectrumBand(int idx) const {
        if (idx < 0 || idx >= 5) return 0.0f;
        return raw_spectrum_bands[idx];
    }
    float getSmoothedSpectrumBand(int idx) const {
        if (idx < 0 || idx >= 5) return 0.0f;
        return smoothed_spectrum_bands[idx];
    }

    // 获取音频各频段经情绪惯性平滑后的能量
    float getAudioLow()  const { return smoothed_audio_low; }
    float getAudioMid()  const { return smoothed_audio_mid; }
    float getAudioHigh() const { return smoothed_audio_high; }

    // 获取音频瞬时实时能量 (供表面流体噪波/鼓包零延迟频谱律动)
    float getRawAudioLow()  const { return raw_audio_low; }
    float getRawAudioMid()  const { return raw_audio_mid; }
    float getRawAudioHigh() const { return raw_audio_high; }

    // 神经张力与情绪波扩散
    float getNeuroTension() const { return neuro_tension; }
    float getNeuroWavePhase() const { return neuro_wave_phase; }

    // 表面尖刺概率与强度
    float getSpikeIntensity() const;

private:
    // 六维生理心理参数 [0.0, 1.0]
    float energy     = 0.85f;
    float stress     = 0.10f;
    float curiosity  = 0.50f;
    float comfort    = 0.80f;
    float attachment = 0.20f;
    float boredom    = 0.0f;

    // 音频能量与 5 频段频谱均衡器
    float current_mic_db = 38.0f;
    float sound_excitation = 0.0f;
    float raw_audio_low   = 0.0f;
    float raw_audio_mid   = 0.0f;
    float raw_audio_high  = 0.0f;
    float smoothed_audio_low  = 0.0f;
    float smoothed_audio_mid  = 0.0f;
    float smoothed_audio_high = 0.0f;

    float raw_spectrum_bands[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float smoothed_spectrum_bands[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // 神经张力与波扩散
    float neuro_tension = 0.0f;
    float neuro_wave_phase = 0.0f;

    EmotionState current_emotion = EMOTION_CALM;

    void updateInternalDynamics(float dt);
    void updateEmotionState();
};
