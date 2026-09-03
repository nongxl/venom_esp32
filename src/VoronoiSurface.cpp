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

void VoronoiSurface::updateSeedDynamics(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, float look_x, float look_y, bool is_sleeping) {
    EmotionState emotion = physiology.getEmotion();
    float tension = physiology.getNeuroTension();
    float raw_low  = physiology.getRawAudioLow();
    float raw_mid  = physiology.getRawAudioMid();
    float raw_high = physiology.getRawAudioHigh();
    float sound_ex = physiology.getSoundExcitation();

    // 情绪基础抖动能量与细胞壁厚度
    if (is_sleeping) {
        // 睡眠状态下极度放缓：表层膜张力降低、神经激惹归零、细胞壁宽厚舒缓
        jitter_energy = 0.005f;
        current_membrane_threshold = 0.40f; // 舒缓松弛，避免出现细碎裂纹
    } else {
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
    }

    // 【音乐频谱物理律动注入 (Audio Visualizer EQ Ripple)】- 睡眠状态屏蔽高频声音噪波激励
    float music_spectrum = is_sleeping ? 0.0f : (raw_low * 0.75f + raw_mid * 0.55f + raw_high * 0.40f);
    if (!is_sleeping) {
        jitter_energy += music_spectrum * 1.10f + sound_ex * 0.30f;
        current_membrane_threshold += music_spectrum * 0.70f;
    }

    for (int i = 0; i < VORONOI_SEEDS; ++i) {
        VoronoiSeed &seed = seeds[i];
        const SkeletonNode &node = skeleton.getNode(seed.attach_node);

        // 基础依附位置
        float target_x = node.x + seed.offset_x;
        float target_y = node.y + seed.offset_y;

        // 情绪驱动的偏移流动 (睡眠时回缩紧贴肉身骨架，拒绝漂移散落)
        if (is_sleeping) {
            target_x = node.x + seed.offset_x * 0.50f;
            target_y = node.y + seed.offset_y * 0.50f;
        } else if (emotion == EMOTION_CURIOSITY) {
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
        if (is_sleeping) {
            // 【睡眠深度休眠噪波】：频率从 1.2 rad/s 大幅降低至 0.22 rad/s（周期约 28.5 秒），波幅缩至 0.08px，极度安详静谧
            float phase = (float)millis() * 0.001f * 0.22f + (float)i * 1.256f;
            target_x += std::sin(phase) * 0.08f;
            target_y += std::cos(phase) * 0.08f;
        } else if (music_spectrum > 0.05f || tension > 0.3f) {
            float freq = 3.0f + music_spectrum * 12.0f;
            float phase = (float)millis() * 0.001f * freq + (float)i * 1.256f;
            float amp = std::min(3.2f, 0.4f + music_spectrum * 2.8f);

            target_x += std::sin(phase) * amp;
            target_y += std::cos(phase) * amp;
        } else {
            // 日常平静状态下的表面噪波律动 (波幅增大以凸显有机流体感)
            float phase = (float)millis() * 0.001f * 1.5f + (float)i * 1.256f;
            target_x += std::sin(phase) * 0.95f;
            target_y += std::cos(phase) * 0.95f;
        }

        // 强阻尼平滑跟随 (睡眠时高粘滞阻尼，如深稠黑蜜，绝无超调振荡)
        float follow_rate = is_sleeping ? 0.06f : 0.20f;
        float damp_factor = is_sleeping ? 0.85f : 0.55f;
        float fx = (target_x - seed.x) * follow_rate;
        float fy = (target_y - seed.y) * follow_rate;
        seed.vx = (seed.vx + fx) * damp_factor;
        seed.vy = (seed.vy + fy) * damp_factor;

        seed.x += seed.vx * dt * 30.0f;
        seed.y += seed.vy * dt * 30.0f;
    }
}

void VoronoiSurface::update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, float look_x, float look_y, bool is_sleeping) {
    updateSeedDynamics(dt, skeleton, physiology, look_x, look_y, is_sleeping);
}
