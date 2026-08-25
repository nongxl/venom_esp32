#include "TentacleRenderer.h"
#include <cmath>

TentacleRenderer::TentacleRenderer() {
    for (int i = 0; i < MAX_TENTACLES; ++i) tentacles[i].active = false;
    grapple.active = false;
}

void TentacleRenderer::init() {
    for (int i = 0; i < MAX_TENTACLES; ++i) tentacles[i].active = false;
    grapple.active = false;
    auto_spawn_timer = 0.0f;
}

void TentacleRenderer::startGrappleCrawl(float from_x, float from_y, float to_x, float to_y) {
    grapple.active = true;
    grapple.stage = GRAPPLE_SHOOT;
    grapple.timer = 0.0f;

    grapple.start_x = from_x;
    grapple.start_y = from_y;
    grapple.target_x = to_x;
    grapple.target_y = to_y;

    grapple.hand_x = from_x;
    grapple.hand_y = from_y;

    grapple.shoot_progress = 0.0f;
    grapple.pull_progress = 0.0f;
    grapple.palm_spread = 0.0f;

    // 轻微弧线扰动（营造自然弹道）
    float dx = to_x - from_x;
    float dy = to_y - from_y;
    grapple.ctrl_offset_x = -dy * 0.15f;
    grapple.ctrl_offset_y = dx * 0.15f;
}

void TentacleRenderer::startCeilingSwing(float from_x, float from_y, float anchor_x, float anchor_y, float rope_length) {
    grapple.active = true;
    grapple.stage = GRAPPLE_SWING;
    grapple.timer = 0.0f;

    grapple.start_x = from_x;
    grapple.start_y = from_y;
    grapple.target_x = anchor_x;
    grapple.target_y = anchor_y;

    grapple.hand_x = anchor_x;
    grapple.hand_y = anchor_y;
    grapple.palm_spread = 1.0f;
    grapple.rope_length = rope_length;

    grapple.ctrl_offset_x = 0.0f;
    grapple.ctrl_offset_y = 0.0f;
}

void TentacleRenderer::endCeilingSwing() {
    if (grapple.stage == GRAPPLE_SWING) {
        grapple.stage = GRAPPLE_FUSE;
        grapple.timer = 0.0f;
    }
}

void TentacleRenderer::updateGrappleCrawl(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology) {
    if (!grapple.active) return;

    grapple.timer += dt;
    float hx, hy;
    skeleton.getHeadPos(hx, hy);
    grapple.start_x = hx;
    grapple.start_y = hy;

    switch (grapple.stage) {
        case GRAPPLE_SHOOT: {
            constexpr float DURATION = 0.32f; // 自然探出与伸展 (0.32s)
            float t = std::min(1.0f, grapple.timer / DURATION);
            grapple.shoot_progress = t;
            grapple.hand_x = grapple.start_x + (grapple.target_x - grapple.start_x) * t;
            grapple.hand_y = grapple.start_y + (grapple.target_y - grapple.start_y) * t;

            if (grapple.timer >= DURATION) {
                grapple.stage = GRAPPLE_ANCHOR;
                grapple.timer = 0.0f;
                grapple.hand_x = grapple.target_x;
                grapple.hand_y = grapple.target_y;
            }
            break;
        }

        case GRAPPLE_ANCHOR: {
            constexpr float DURATION = 0.18f;
            grapple.hand_x = grapple.target_x;
            grapple.hand_y = grapple.target_y;
            grapple.palm_spread = std::min(1.0f, grapple.timer / DURATION);

            if (grapple.timer >= DURATION) {
                grapple.stage = GRAPPLE_PULL;
                grapple.timer = 0.0f;
            }
            break;
        }

        case GRAPPLE_PULL: {
            constexpr float DURATION = 0.75f; // 沉稳有力的液态肉身拉动 (0.75s)
            grapple.hand_x = grapple.target_x;
            grapple.hand_y = grapple.target_y;
            grapple.palm_spread = 1.0f;

            // 强力牵引头部向掌心移动
            float pull_ratio = std::min(1.0f, grapple.timer / DURATION);
            grapple.pull_progress = pull_ratio;

            float tension = physiology.getNeuroTension();
            skeleton.setPullTarget(grapple.target_x, grapple.target_y, 1.85f + tension * 0.6f);

            float dx = grapple.target_x - hx;
            float dy = grapple.target_y - hy;
            float dist = std::sqrt(dx * dx + dy * dy);

            // 当头部接近抓取点，进入 GRAPPLE_HOLD 阶段（继续吸住 0.8~1.5s 保持吸力抵抗重力！）
            if (dist < 10.0f || grapple.timer >= DURATION) {
                grapple.stage = GRAPPLE_HOLD;
                grapple.timer = 0.0f;
                grapple.hold_duration = 0.8f + (rand() % 8) * 0.1f;
            }
            break;
        }

        case GRAPPLE_HOLD: {
            // 【手部吸附保持阶段】：掌心死死贴在玻璃上，死锁头部位置抗重力挂住！
            grapple.hand_x = grapple.target_x;
            grapple.hand_y = grapple.target_y;
            grapple.palm_spread = 1.0f;
            skeleton.setPullTarget(grapple.target_x, grapple.target_y, 2.0f);

            if (grapple.timer >= grapple.hold_duration) {
                grapple.stage = GRAPPLE_FUSE;
                grapple.timer = 0.0f;
                skeleton.clearPullTarget();
            }
            break;
        }

        case GRAPPLE_SWING: {
            // 高空秋千悬挂状态：掌心死死锚定在天花板
            grapple.hand_x = grapple.target_x;
            grapple.hand_y = grapple.target_y;
            grapple.palm_spread = 1.0f;

            // 头部位置随摆动自然游动，触手紧绷连接
            if (!skeleton.isHanging()) {
                // 若骨架系统脱离悬挂，触手自然融回收起
                grapple.stage = GRAPPLE_FUSE;
                grapple.timer = 0.0f;
            }
            break;
        }

        case GRAPPLE_FUSE: {
            constexpr float DURATION = 0.20f;
            skeleton.clearPullTarget();
            float t = grapple.timer / DURATION;
            grapple.palm_spread = std::max(0.0f, 1.0f - t);

            if (grapple.timer >= DURATION) {
                grapple.active = false;
                grapple.stage = GRAPPLE_INACTIVE;
            }
            break;
        }

        default:
            grapple.active = false;
            break;
    }
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

            // 触手向四周开阔空间或最近玻璃边缘摸索
            float angle = (rand() % 360) * 0.017453f;
            float reach = 22.0f + (rand() % 28);
            t.target_x = std::max(4.0f, std::min(SCREEN_W - 4.0f, node.x + std::cos(angle) * reach));
            t.target_y = std::max(4.0f, std::min(SCREEN_H - 4.0f, node.y + std::sin(angle) * reach));

            t.max_length = reach + 6.0f;
            t.duration = 2.0f + (rand() % 15) * 0.1f; // 2.0 ~ 3.5 秒生命周期

            t.ctrl_x = (t.start_x + t.target_x) * 0.5f + ((rand() % 24) - 12);
            t.ctrl_y = (t.start_y + t.target_y) * 0.5f + ((rand() % 24) - 12);

            t.length_progress = 0.0f;
            t.wave_phase = (rand() % 100) * 0.1f;
            t.life_timer = 0.0f;
            t.base_thickness = 3.5f;
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

    float wave_speed = 3.5f + physiology.getNeuroTension() * 4.5f;
    t.wave_phase += dt * wave_speed;

    if (t.life_timer < 0.35f) {
        t.length_progress = t.life_timer / 0.35f;
    } else if (t.life_timer < t.duration - 0.35f) {
        t.length_progress = 1.0f;
        float wave = std::sin(t.wave_phase) * 3.5f;
        t.ctrl_x += wave * dt * 2.0f;
    } else {
        float retract = (t.duration - t.life_timer) / 0.35f;
        t.length_progress = (retract < 0.0f) ? 0.0f : retract;
        if (t.length_progress <= 0.01f) {
            t.active = false;
            return;
        }
    }

    t.end_x = t.start_x + (t.target_x - t.start_x) * t.length_progress;
    t.end_y = t.start_y + (t.target_y - t.start_y) * t.length_progress;
}

void TentacleRenderer::update(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology, bool is_upside_down) {
    // 1. 爬行抓取触手更新
    updateGrappleCrawl(dt, skeleton, physiology);

    // 2. 全天候多触手高频自发摸索生长 (Ambient Spawning)
    auto_spawn_timer += dt;
    float spawn_threshold = (physiology.getNeuroTension() > 0.4f || is_upside_down) ? 0.45f : 0.85f;
    if (auto_spawn_timer >= spawn_threshold) {
        auto_spawn_timer = 0.0f;
        if ((rand() % 100) < 85) {
            spawnTentacle(skeleton, is_upside_down);
        }
    }

    for (int i = 0; i < MAX_TENTACLES; ++i) {
        updateTentacle(i, dt, skeleton, physiology);
    }
}

void TentacleRenderer::drawGrappleTendril(M5Canvas &canvas) const {
    if (!grapple.active) return;

    float sx = grapple.start_x;
    float sy = grapple.start_y;
    float hx = grapple.hand_x;
    float hy = grapple.hand_y;

    float cx = (sx + hx) * 0.5f + grapple.ctrl_offset_x * (1.0f - grapple.pull_progress);
    float cy = (sy + hy) * 0.5f + grapple.ctrl_offset_y * (1.0f - grapple.pull_progress);

    constexpr int SEGMENTS = 10;
    float prev_x = sx, prev_y = sy;

    // 1. 绘制实心纯黑贝塞尔肉柱触手
    for (int step = 1; step <= SEGMENTS; ++step) {
        float s = (float)step / (float)SEGMENTS;
        float one_minus_s = 1.0f - s;

        float cur_x = one_minus_s * one_minus_s * sx +
                      2.0f * one_minus_s * s * cx +
                      s * s * hx;
        float cur_y = one_minus_s * one_minus_s * sy +
                      2.0f * one_minus_s * s * cy +
                      s * s * hy;

        // 根部粗 (5.5px) -> 前端逐渐变细 (3.2px)
        int thickness = (int)std::round(5.5f * (1.0f - s * 0.42f));
        for (int off = -thickness / 2; off <= thickness / 2; ++off) {
            canvas.drawLine((int)prev_x + off, (int)prev_y, (int)cur_x + off, (int)cur_y, COLOR_VENOM_CORE);
            canvas.drawLine((int)prev_x, (int)prev_y + off, (int)cur_x, (int)cur_y + off, COLOR_VENOM_CORE);
        }

        // 荡秋千状态下在触手核心绘制一条紧绷的高光拉丝微光
        if (grapple.stage == GRAPPLE_SWING && step % 2 == 0) {
            canvas.drawPixel((int)cur_x, (int)cur_y, COLOR_DITHER_GRAY);
        }

        prev_x = cur_x;
        prev_y = cur_y;
    }

    // 2. 绘制目的地掌心（Palm Hand）肉垫
    int palm_r = (int)std::round(4.0f + grapple.palm_spread * 1.8f);
    canvas.fillCircle((int)hx, (int)hy, palm_r, COLOR_VENOM_CORE);

    // 3. 绘制掌心抓附微指（3 根张开吸附在目的地的爪指）
    if (grapple.palm_spread > 0.05f) {
        float dx = hx - sx;
        float dy = hy - sy;
        float main_angle = std::atan2(dy, dx);
        float finger_len = 5.0f + grapple.palm_spread * 4.5f;

        for (int f = -1; f <= 1; ++f) {
            float f_angle = main_angle + (float)f * 0.45f;
            float fx = hx + std::cos(f_angle) * finger_len;
            float fy = hy + std::sin(f_angle) * finger_len;

            canvas.drawLine((int)hx, (int)hy, (int)fx, (int)fy, COLOR_VENOM_CORE);
            canvas.drawLine((int)hx + 1, (int)hy, (int)fx + 1, (int)fy, COLOR_VENOM_CORE);
        }
    }
}

void TentacleRenderer::draw(M5Canvas &canvas) const {
    // 1. 绘制爬行射出触手与掌心爪盘
    drawGrappleTendril(canvas);

    // 2. 绘制倒置吸附微丝
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

            int thickness = (int)std::round(t.base_thickness * (1.0f - s * 0.6f));
            for (int off = -thickness / 2; off <= thickness / 2; ++off) {
                canvas.drawLine((int)prev_x + off, (int)prev_y, (int)cur_x + off, (int)cur_y, COLOR_VENOM_CORE);
                canvas.drawLine((int)prev_x, (int)prev_y + off, (int)cur_x, (int)cur_y + off, COLOR_VENOM_CORE);
            }

            prev_x = cur_x;
            prev_y = cur_y;
        }
    }
}
