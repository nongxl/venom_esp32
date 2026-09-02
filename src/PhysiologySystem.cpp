#include "PhysiologySystem.h"
#include <cmath>

PhysiologySystem::PhysiologySystem() {}

void PhysiologySystem::init() {
    energy     = 0.85f;
    stress     = 0.10f;
    curiosity  = 0.50f;
    comfort    = 0.80f;
    attachment = 0.20f;
    boredom    = 0.0f;
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

void PhysiologySystem::triggerShock(float amount) {
    stress = std::min(1.0f, stress + amount);
    comfort = std::max(0.0f, comfort - amount * 0.7f);
    neuro_tension = std::min(1.0f, neuro_tension + amount * 1.2f);
}

void PhysiologySystem::updateInternalDynamics(float dt) {
    // 1. 5 频段专业音乐频谱均衡器快攻慢释 (Fast-Attack Peak-Hold & Smooth Decay)
    for (int i = 0; i < 5; ++i) {
        if (raw_spectrum_bands[i] > smoothed_spectrum_bands[i]) {
            smoothed_spectrum_bands[i] = smoothed_spectrum_bands[i] * 0.35f + raw_spectrum_bands[i] * 0.65f; // 极速瞬态爆发
        } else {
            smoothed_spectrum_bands[i] = smoothed_spectrum_bands[i] * 0.82f + raw_spectrum_bands[i] * 0.18f; // 优雅阻尼回落
        }
    }

    smoothed_audio_low  = std::max(smoothed_spectrum_bands[0], smoothed_spectrum_bands[1]);
    smoothed_audio_mid  = smoothed_spectrum_bands[2];
    smoothed_audio_high = std::max(smoothed_spectrum_bands[3], smoothed_spectrum_bands[4]);

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

    // 基础代谢自然消耗能量 (剧烈运动额外消耗，形成 2~4 分钟真实体力消耗闭环)
    energy = std::max(0.05f, energy - dt * 0.0030f);
}

void PhysiologySystem::updateEmotionState() {
    // 基于宽区间滞后门限 (Hysteresis) 的纯自然生物情绪演化
    switch (current_emotion) {
        case EMOTION_CALM:
            if (energy < 0.35f) {
                current_emotion = EMOTION_EXHAUSTED;
            } else if (stress > 0.45f) {
                current_emotion = (smoothed_audio_high > 0.6f || neuro_tension > 0.7f) ? EMOTION_ANGER : EMOTION_STRESS;
            } else if (curiosity > 0.60f && comfort > 0.40f) {
                current_emotion = EMOTION_CURIOSITY;
            }
            break;

        case EMOTION_EXHAUSTED:
            if (energy > 0.60f) {
                // 睡醒或精力恢复，回归平静
                current_emotion = EMOTION_CALM;
            }
            break;

        case EMOTION_CURIOSITY:
            if (energy < 0.35f) {
                current_emotion = EMOTION_EXHAUSTED;
            } else if (stress > 0.45f) {
                current_emotion = EMOTION_STRESS;
            } else if (curiosity < 0.35f || comfort < 0.30f) {
                current_emotion = EMOTION_CALM;
            }
            break;

        case EMOTION_STRESS:
            if (energy < 0.15f) {
                current_emotion = EMOTION_EXHAUSTED;
            } else if (stress > 0.70f) {
                current_emotion = (smoothed_audio_high > 0.6f || neuro_tension > 0.75f) ? EMOTION_ANGER : EMOTION_FEAR;
            } else if (stress < 0.20f) {
                // 压力完全释放，平稳回归平静
                current_emotion = EMOTION_CALM;
            }
            break;

        case EMOTION_FEAR:
        case EMOTION_ANGER:
            if (stress < 0.35f) {
                current_emotion = EMOTION_STRESS; // 暴躁/恐惧渐进式降温回落
            }
            break;
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
    float base_spike = 0.0f;
    if (current_emotion == EMOTION_ANGER) {
        base_spike = 0.85f + smoothed_audio_high * 0.15f;
    } else if (current_emotion == EMOTION_FEAR || current_emotion == EMOTION_STRESS) {
        base_spike = 0.45f + smoothed_audio_high * 0.35f;
    }
    // 音乐节拍与强声压激发尖刺律动 (即使在平静/好奇听音乐状态下，尖刺也会随音乐重音猛烈爆发)
    float music_spike = std::max(raw_audio_low * 0.95f, raw_audio_high * 1.15f) * (0.6f + sound_excitation * 0.4f);
    return std::min(1.0f, std::max(base_spike, music_spike));
}
