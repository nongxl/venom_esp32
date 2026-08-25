#include "TentacleRenderer.h"
#include <cmath>

TentacleRenderer::TentacleRenderer() {
    for (int i = 0; i < MAX_TENTACLES; ++i) {
        tentacles[i].active = false;
    }
}

void TentacleRenderer::init() {
    for (int i = 0; i < MAX_TENTACLES; ++i) {
        tentacles[i].active = false;
    }
    auto_spawn_timer = 0.0f;
}

void TentacleRenderer::spawnTentacle(const SkeletonSystem &skeleton, bool cling_edge) {
    for (int i = 0; i < MAX_TENTACLES; ++i) {
        if (!tentacles[i].active) {
            Tentacle &t = tentacles[i];
            t.active = true;
            t.attach_node_idx = 1 + (rand() % (SKELETON_NODE_COUNT - 2));
            const SkeletonNode &node = skeleton.getNode(t.attach_node_idx);

            t.start_x = node.x;
            t.start_y = node.y;
            t.is_clinging = cling_edge;

            if (cling_edge) {
                // 倒置或恐惧时，强力朝最近边缘锚定
                float dist_top = node.y;
                float dist_bot = SCREEN_H - node.y;
                float dist_l   = node.x;
                float dist_r   = SCREEN_W - node.x;
                float min_d = std::min({dist_top, dist_bot, dist_l, dist_r});

                if (min_d == dist_top) {
                    t.target_x = node.x + (rand() % 40 - 20);
                    t.target_y = 0.0f;
                } else if (min_d == dist_bot) {
                    t.target_x = node.x + (rand() % 40 - 20);
                    t.target_y = SCREEN_H;
                } else if (min_d == dist_l) {
                    t.target_x = 0.0f;
                    t.target_y = node.y + (rand() % 40 - 20);
                } else {
                    t.target_x = SCREEN_W;
                    t.target_y = node.y + (rand() % 40 - 20);
                }
                t.max_length = min_d + 10.0f;
                t.duration = 4.0f;
            } else {
                float angle = ((rand() % 180) - 90) * 0.017453f - 1.57f;
                t.max_length = 25.0f + (rand() % 30);
                t.target_x = t.start_x + std::cos(angle) * t.max_length;
                t.target_y = t.start_y + std::sin(angle) * t.max_length;
                t.duration = 2.0f + (rand() % 20) * 0.1f;
            }

            t.ctrl_x = (t.start_x + t.target_x) * 0.5f + ((rand() % 20) - 10);
            t.ctrl_y = (t.start_y + t.target_y) * 0.5f + ((rand() % 20) - 10);

            t.length_progress = 0.0f;
            t.wave_phase = (rand() % 100) * 0.1f;
            t.life_timer = 0.0f;
            t.base_thickness = 3.0f + (rand() % 2);
            break;
        }
    }
}

void TentacleRenderer::updateTentacle(int idx, float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology) {
    Tentacle &t = tentacles[idx];
    if (!t.active) return;

    const SkeletonNode &node = skeleton.getNode(t.attach_node_idx);
    t.start_x = node.x;
    t.start_y = node.y;

    t.life_timer += dt;

    float wave_speed = 4.0f + physiology.getAudioMid() * 8.0f + physiology.getNeuroTension() * 6.0f;
    t.wave_phase += dt * wave_speed;

    if (t.life_timer < 0.5f) {
        t.length_progress = t.life_timer / 0.5f;
    } else if (t.life_timer < t.duration - 0.5f) {
        t.length_progress = 1.0f;
        float wave = std::sin(t.wave_phase) * (4.0f + physiology.getAudioMid() * 8.0f);
        t.ctrl_x += wave * dt * 2.0f;
        t.ctrl_y += std::cos(t.wave_phase * 1.3f) * 3.0f * dt;
    } else {
        float retract = (t.duration - t.life_timer) / 0.5f;
        t.length_progress = (retract < 0.0f) ? 0.0f : retract;
        if (t.length_progress <= 0.01f) {
            t.active = false;
            return;
        }
    }

    t.end_x = t.start_x + (t.target_x - t.start_x) * t.length_progress;
    t.end_y = t.start_y + (t.target_y - t.start_y) * t.length_progress;
}

void TentacleRenderer::update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, bool is_upside_down) {
    auto_spawn_timer += dt;

    EmotionState emotion = physiology.getEmotion();
    float spawn_interval = (emotion == EMOTION_FEAR || is_upside_down) ? 0.6f : 2.5f;

    if (auto_spawn_timer > spawn_interval) {
        auto_spawn_timer = 0.0f;
        if ((rand() % 100) < (is_upside_down ? 90 : 55)) {
            spawnTentacle(skeleton, is_upside_down || emotion == EMOTION_FEAR);
        }
    }

    for (int i = 0; i < MAX_TENTACLES; ++i) {
        updateTentacle(i, dt, skeleton, physiology);
    }
}

void TentacleRenderer::draw(M5Canvas &canvas) const {
    for (int i = 0; i < MAX_TENTACLES; ++i) {
        const Tentacle &t = tentacles[i];
        if (!t.active || t.length_progress < 0.05f) continue;

        constexpr int SEGMENTS = 8;
        float prev_x = t.start_x;
        float prev_y = t.start_y;

        for (int step = 1; step <= SEGMENTS; ++step) {
            float s = (float)step / (float)SEGMENTS;
            float one_minus_s = 1.0f - s;

            float cur_x = one_minus_s * one_minus_s * t.start_x +
                          2.0f * one_minus_s * s * t.ctrl_x +
                          s * s * t.end_x;
            float cur_y = one_minus_s * one_minus_s * t.start_y +
                          2.0f * one_minus_s * s * t.ctrl_y +
                          s * s * t.end_y;

            int thickness = (int)std::round(t.base_thickness * (1.0f - s * 0.7f));
            if (thickness <= 1) {
                canvas.drawLine((int)prev_x, (int)prev_y, (int)cur_x, (int)cur_y, COLOR_VENOM_BLACK);
            } else {
                for (int off = -thickness / 2; off <= thickness / 2; ++off) {
                    canvas.drawLine((int)prev_x + off, (int)prev_y, (int)cur_x + off, (int)cur_y, COLOR_VENOM_BLACK);
                    canvas.drawLine((int)prev_x, (int)prev_y + off, (int)cur_x, (int)cur_y + off, COLOR_VENOM_BLACK);
                }
            }

            prev_x = cur_x;
            prev_y = cur_y;
        }
    }
}
