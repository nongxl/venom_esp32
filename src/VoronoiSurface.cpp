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
    float audio_mid = physiology.getAudioMid();
    float audio_high = physiology.getAudioHigh();

    // 情绪影响抖动能量与细胞壁厚度
    switch (emotion) {
        case EMOTION_CALM:
            jitter_energy = 0.05f;
            current_membrane_threshold = 1.0f;
            break;
        case EMOTION_STRESS:
            jitter_energy = 0.40f + audio_high * 0.3f;
            current_membrane_threshold = 1.8f; // 细碎裂变
            break;
        case EMOTION_FEAR:
            jitter_energy = 0.65f;
            current_membrane_threshold = 0.8f; // 紧缩硬化
            break;
        case EMOTION_ANGER:
            jitter_energy = 0.80f + audio_high * 0.4f;
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

    // 叠加麦克风实时环境音量分贝驱动的表面高频翻滚噪波
    float sound_ex = physiology.getSoundExcitation();
    jitter_energy += sound_ex * 0.85f;

    for (int i = 0; i < VORONOI_SEEDS; ++i) {
        VoronoiSeed &seed = seeds[i];
        const SkeletonNode &node = skeleton.getNode(seed.attach_node);

        // 基础依附位置
        float target_x = node.x + seed.offset_x;
        float target_y = node.y + seed.offset_y;

        // 情绪驱动的偏移扰动
        if (emotion == EMOTION_CURIOSITY) {
            // 好奇时向注视方向偏移流动
            float ldx = look_x - node.x;
            float ldy = look_y - node.y;
            float ldist = std::sqrt(ldx * ldx + ldy * ldy);
            if (ldist > 1.0f) {
                target_x += (ldx / ldist) * 6.0f;
                target_y += (ldy / ldist) * 6.0f;
            }
        } else if (emotion == EMOTION_FEAR) {
            // 恐惧向骨架中心收缩塌陷
            target_x = node.x + seed.offset_x * 0.45f;
            target_y = node.y + seed.offset_y * 0.45f;
        } else if (emotion == EMOTION_ANGER) {
            // 愤怒向外辐射突刺
            target_x = node.x + seed.offset_x * 1.45f;
            target_y = node.y + seed.offset_y * 1.45f;
        }

        // 叠加神经中频与高频微抖
        if (jitter_energy > 0.05f) {
            float jx = ((rand() % 100) - 50) * 0.08f * jitter_energy;
            float jy = ((rand() % 100) - 50) * 0.08f * jitter_energy;
            target_x += jx;
            target_y += jy;
        }

        // 弹簧阻尼跟随
        float fx = (target_x - seed.x) * 0.25f;
        float fy = (target_y - seed.y) * 0.25f;
        seed.vx = (seed.vx + fx) * 0.70f;
        seed.vy = (seed.vy + fy) * 0.70f;

        seed.x += seed.vx * dt * 30.0f;
        seed.y += seed.vy * dt * 30.0f;
    }
}

void VoronoiSurface::update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, float look_x, float look_y) {
    updateSeedDynamics(dt, skeleton, physiology, look_x, look_y);
}
