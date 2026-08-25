#pragma once
#include <Arduino.h>
#include "config.h"

class PhysiologySystem {
public:
    PhysiologySystem();

    void init();
    void update(float dt, float imu_shake, bool is_upside_down, bool btn_pressed);

    // 注入音频三频段能量 (由 main.cpp 中的 FFT/滤波器计算传入，范围 0.0 ~ 1.0)
    void feedAudioBands(float low, float mid, float high);

    // 触发外界直接刺激
    void triggerShock(float amount);

    // 捕食小虫子进食营养与饱腹补充
    void feedNutrition(float energy_amount = 0.25f, float comfort_amount = 0.20f);

    // 获取五维生理心理参数
    float getEnergy()     const { return energy; }
    float getStress()     const { return stress; }
    float getCuriosity()  const { return curiosity; }
    float getComfort()    const { return comfort; }
    float getAttachment() const { return attachment; }

    // 获取当前主导情绪
    EmotionState getEmotion() const { return current_emotion; }
    const char* getEmotionName() const;

    // 获取音频各频段经情绪惯性平滑后的能量
    float getAudioLow()  const { return smoothed_audio_low; }
    float getAudioMid()  const { return smoothed_audio_mid; }
    float getAudioHigh() const { return smoothed_audio_high; }

    // 神经张力与情绪波扩散
    float getNeuroTension() const { return neuro_tension; }
    float getNeuroWavePhase() const { return neuro_wave_phase; }

    // 表面尖刺概率与强度
    float getSpikeIntensity() const;

private:
    // 五维生理心理参数 [0.0, 1.0]
    float energy     = 0.85f;
    float stress     = 0.10f;
    float curiosity  = 0.50f;
    float comfort    = 0.80f;
    float attachment = 0.20f;

    // 音频能量与情绪惯性
    float raw_audio_low   = 0.0f;
    float raw_audio_mid   = 0.0f;
    float raw_audio_high  = 0.0f;
    float smoothed_audio_low  = 0.0f;
    float smoothed_audio_mid  = 0.0f;
    float smoothed_audio_high = 0.0f;

    // 神经张力与波扩散
    float neuro_tension = 0.0f;
    float neuro_wave_phase = 0.0f;

    EmotionState current_emotion = EMOTION_CALM;

    void updateInternalDynamics(float dt);
    void updateEmotionState();
};
