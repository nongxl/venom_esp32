#include "PhysiologySystem.h"
#include <cmath>

PhysiologySystem::PhysiologySystem() {}

void PhysiologySystem::init() {
    energy     = 0.85f;
    stress     = 0.10f;
    curiosity  = 0.50f;
    comfort    = 0.80f;
    attachment = 0.20f;
    current_emotion = EMOTION_CALM;
}

const char* PhysiologySystem::getEmotionName() const {
    switch (current_emotion) {
        case EMOTION_CALM:      return "CALM";
        case EMOTION_STRESS:    return "STRESS";
        case EMOTION_FEAR:      return "FEAR";
        case EMOTION_ANGER:     return "ANGER";
        case EMOTION_CURIOSITY: return "CURIOSITY";
        case EMOTION_EXHAUSTED: return "EXHAUSTED";
        default:                return "UNKNOWN";
    }
}

void PhysiologySystem::feedAudioBands(float low, float mid, float high) {
    raw_audio_low  = low;
    raw_audio_mid  = mid;
    raw_audio_high = high;
}

void PhysiologySystem::triggerShock(float amount) {
    stress = std::min(1.0f, stress + amount);
    comfort = std::max(0.0f, comfort - amount * 0.7f);
    neuro_tension = std::min(1.0f, neuro_tension + amount * 1.2f);
}

void PhysiologySystem::updateInternalDynamics(float dt) {
    // 1. 音频能量的攻击与长释放包络（情绪惯性与残留）
    smoothed_audio_low  = smoothed_audio_low  * 0.70f + raw_audio_low  * 0.30f;
    smoothed_audio_mid  = smoothed_audio_mid  * 0.75f + raw_audio_mid  * 0.25f;
    smoothed_audio_high = smoothed_audio_high * 0.85f + raw_audio_high * 0.15f;

    // 极高频突发巨响/尖锐冲击激发 stress，音乐旋律与低频重音激发好奇探究 (CURIOSITY)
    if (raw_audio_high > 0.65f) {
        stress = std::min(1.0f, stress + raw_audio_high * dt * 0.6f);
    }
    if ((raw_audio_low > 0.15f || raw_audio_mid > 0.20f) && stress < 0.35f) {
        curiosity = std::min(1.0f, curiosity + (raw_audio_low + raw_audio_mid) * dt * 0.25f);
        comfort = std::min(1.0f, comfort + raw_audio_low * dt * 0.10f);
    }

    // 2. 神经张力演化 (主要受压力驱动，低频音乐产生柔和律动波)
    float target_tension = stress * 0.7f + smoothed_audio_high * 0.35f + smoothed_audio_low * 0.25f;
    neuro_tension = neuro_tension * 0.90f + target_tension * 0.10f;

    // 3. 神经波扩散相位递增
    float wave_speed = 3.0f + neuro_tension * 8.0f;
    neuro_wave_phase += dt * wave_speed;

    // 4. 生理参数自发慢演化
    // 压力自然衰减（半衰期约 4~6 秒）
    stress = std::max(0.02f, stress - dt * 0.08f);

    // 舒适度在低 stress 时缓慢回升
    if (stress < 0.25f) {
        comfort = std::min(1.0f, comfort + dt * 0.05f);
    } else {
        comfort = std::max(0.0f, comfort - dt * 0.12f);
    }

    // 好奇心在平静时积累
    if (comfort > 0.6f && stress < 0.2f) {
        curiosity = std::min(1.0f, curiosity + dt * 0.04f);
    } else if (stress > 0.5f) {
        curiosity = std::max(0.1f, curiosity - dt * 0.15f);
    }

    // 基础代谢自然缓慢消耗能量 (每分钟消耗约 0.35)
    energy = std::max(0.05f, energy - dt * 0.006f);
}

void PhysiologySystem::updateEmotionState() {
    // 综合判定主导情绪
    if (energy < 0.25f) {
        current_emotion = EMOTION_EXHAUSTED; // 疲惫困倦
    } else if (stress > 0.65f) {
        if (smoothed_audio_high > 0.5f || neuro_tension > 0.75f) {
            current_emotion = EMOTION_ANGER; // 受强烈持续刺激时转为愤怒攻击形态
        } else {
            current_emotion = EMOTION_FEAR;  // 剧烈恐惧收缩
        }
    } else if (stress > 0.30f) {
        current_emotion = EMOTION_STRESS;    // 紧张不安
    } else if (curiosity > 0.60f && comfort > 0.50f) {
        current_emotion = EMOTION_CURIOSITY; // 好奇探究
    } else {
        current_emotion = EMOTION_CALM;      // 平静舒缓
    }
}

void PhysiologySystem::update(float dt, float imu_shake, bool is_upside_down, bool btn_pressed) {
    if (btn_pressed) {
        triggerShock(0.45f);
    }

    if (imu_shake > 0.2f) {
        stress = std::min(1.0f, stress + imu_shake * dt * 1.5f);
    }

    // 设备倒置时产生额外不安
    if (is_upside_down) {
        stress = std::min(1.0f, stress + dt * 0.15f);
    }

    updateInternalDynamics(dt);
    updateEmotionState();
}

float PhysiologySystem::getSpikeIntensity() const {
    if (current_emotion == EMOTION_ANGER) {
        return 0.8f + smoothed_audio_high * 0.2f;
    } else if (current_emotion == EMOTION_FEAR || current_emotion == EMOTION_STRESS) {
        return 0.35f + smoothed_audio_high * 0.3f;
    }
    return smoothed_audio_high * 0.2f;
}
