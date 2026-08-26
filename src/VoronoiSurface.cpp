#include "VoronoiSurface.h"
#include <cmath>

VoronoiSurface::VoronoiSurface() {
    for (int i = 0; i < VORONOI_SEEDS; ++i) {
        seeds[i].attach_node = i % SKELETON_NODE_COUNT;
        float angle = (i * 360.0f / VORONOI_SEEDS) * 0.017453f;
        float dist = 8.0f + (i % 3) * 6.0f;
        seeds[i].offset_x = std::cos(angle) * dist;
        seeds[i].offset_y = std::sin(angle) * dist;
    }
}

void VoronoiSurface::init() {
    for (int i = 0; i < VORONOI_SEEDS; ++i) {
        seeds[i].x = SCREEN_W * 0.5f;
        seeds[i].y = SCREEN_H * 0.5f;
        seeds[i].vx = 0.0f;
        seeds[i].vy = 0.0f;
        seeds[i].activity = 0.0f;
    }
}

void VoronoiSurface::updateSeedDynamics(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, float look_x, float look_y) {
    EmotionState emotion = physiology.getEmotion();
    float tension = physiology.getNeuroTension();
    float raw_low  = physiology.getRawAudioLow();
    float raw_mid  = physiology.getRawAudioMid();
    float raw_high = physiology.getRawAudioHigh();
    float sound_ex = physiology.getSoundExcitation();

    // 情绪基础抖动能量与细胞壁厚度
    switch (emotion) {
        case EMOTION_CALM:
            jitter_energy = 0.05f;
            current_membrane_threshold = 1.0f;
            break;
        case EMOTION_STRESS:
            jitter_energy = 0.35f + raw_high * 0.3f;
            current_membrane_threshold = 1.8f; // 细碎裂变
            break;
        case EMOTION_FEAR:
            jitter_energy = 0.50f;
            current_membrane_threshold = 0.8f; // 紧缩硬化
            break;
        case EMOTION_ANGER:
            jitter_energy = 0.70f + raw_high * 0.4f;
            current_membrane_threshold = 2.4f; // 放射状明显裂纹
            break;
        case EMOTION_CURIOSITY:
            jitter_energy = 0.15f;
            current_membrane_threshold = 1.2f;
            break;
        case EMOTION_EXHAUSTED:
            jitter_energy = 0.02f;
            current_membrane_threshold = 0.6f;
            break;
    }

    // 【音乐频谱物理律动注入 (Audio Visualizer EQ Ripple)】
    // 综合低频鼓点、中频旋律、高频镲片能量
    float music_spectrum = raw_low * 0.75f + raw_mid * 0.55f + raw_high * 0.40f;
    jitter_energy += music_spectrum * 1.10f + sound_ex * 0.30f;
    current_membrane_threshold += music_spectrum * 0.70f;

    for (int i = 0; i < VORONOI_SEEDS; ++i) {
        VoronoiSeed &seed = seeds[i];
        const SkeletonNode &node = skeleton.getNode(seed.attach_node);

        // 基础依附位置
        float target_x = node.x + seed.offset_x;
        float target_y = node.y + seed.offset_y;

        // 情绪驱动的偏移流动
        if (emotion == EMOTION_CURIOSITY) {
            float ldx = look_x - node.x;
            float ldy = look_y - node.y;
            float ldist = std::sqrt(ldx * ldx + ldy * ldy);
            if (ldist > 1.0f) {
                target_x += (ldx / ldist) * 5.0f;
                target_y += (ldy / ldist) * 5.0f;
            }
        } else if (emotion == EMOTION_FEAR) {
            target_x = node.x + seed.offset_x * 0.45f;
            target_y = node.y + seed.offset_y * 0.45f;
        } else if (emotion == EMOTION_ANGER) {
            target_x = node.x + seed.offset_x * 1.35f;
            target_y = node.y + seed.offset_y * 1.35f;
        }

        // 纯正连续流体谐波行波 (完全消除白噪声随机抖动，保证极其平滑顺滑)
        if (music_spectrum > 0.05f || tension > 0.3f) {
            float freq = 3.0f + music_spectrum * 12.0f;
            float phase = (float)millis() * 0.001f * freq + (float)i * 1.256f;
            float amp = std::min(3.2f, 0.4f + music_spectrum * 2.8f);

            target_x += std::sin(phase) * amp;
            target_y += std::cos(phase) * amp;
        } else {
            // 平静状态下极缓慢柔和的微呼吸流动 (波幅仅 0.35px, 0 抖动)
            float phase = (float)millis() * 0.001f * 1.2f + (float)i * 1.256f;
            target_x += std::sin(phase) * 0.35f;
            target_y += std::cos(phase) * 0.35f;
        }

        // 强阻尼平滑跟随 (平稳无超调振荡)
        float fx = (target_x - seed.x) * 0.20f;
        float fy = (target_y - seed.y) * 0.20f;
        seed.vx = (seed.vx + fx) * 0.55f;
        seed.vy = (seed.vy + fy) * 0.55f;

        seed.x += seed.vx * dt * 30.0f;
        seed.y += seed.vy * dt * 30.0f;
    }
}

void VoronoiSurface::update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, float look_x, float look_y) {
    updateSeedDynamics(dt, skeleton, physiology, look_x, look_y);
}
