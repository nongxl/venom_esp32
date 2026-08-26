#include "TentacleRenderer.h"
#include <cmath>

TentacleRenderer::TentacleRenderer() {
    for (int i = 0; i < MAX_TENTACLES; ++i) tentacles[i].active = false;
    grapple.active = false;
    initMicroPodia();
}

void TentacleRenderer::init() {
    for (int i = 0; i < MAX_TENTACLES; ++i) tentacles[i].active = false;
    grapple.active = false;
    auto_spawn_timer = 0.0f;
    initMicroPodia();
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
            t.base_thickness = 4.0f; // 自发探索触手
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

void TentacleRenderer::initMicroPodia() {
    podia_wave_phase = 0.0f;
    creep_dir = 1.0f;
    creep_switch_timer = 4.0f + (rand() % 60) * 0.1f;
    for (int i = 0; i < MAX_MICRO_PODIA; ++i) {
        podia[i].node_idx = (i * SKELETON_NODE_COUNT) / MAX_MICRO_PODIA;
        podia[i].offset_x = ((float)(i % 2 == 0 ? -1 : 1)) * (3.0f + (float)i * 1.5f);
        podia[i].offset_y = 0.0f;
        podia[i].phase = (float)i * 0.785f; // 45 度相移纤毛波
        podia[i].leg_len = 5.0f + (float)(rand() % 20) * 0.1f;
        podia[i].is_planted = false;
    }
}

void TentacleRenderer::updateMicroPodia(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology) {
    if (grapple.stage == GRAPPLE_SWING) return;

    // 定期随机微调漫步蠕动方向
    creep_switch_timer -= dt;
    if (creep_switch_timer <= 0.0f) {
        creep_switch_timer = 5.0f + (rand() % 80) * 0.1f;
        creep_dir = (rand() % 100 < 50) ? 1.0f : -1.0f;
    }

    // 异相波推进角速度 (慢速优雅 4.0 rad/s)
    float wave_speed = 3.6f + physiology.getNeuroTension() * 2.5f;
    podia_wave_phase += dt * wave_speed;

    float total_creep_impulse_x = 0.0f;
    float total_creep_impulse_y = 0.0f;
    int planted_count = 0;

    for (int i = 0; i < MAX_MICRO_PODIA; ++i) {
        MicroPodia &p = podia[i];
        const SkeletonNode &node = skeleton.getNode(p.node_idx);

        bool on_bottom = (node.contact_bottom > 0.05f);
        bool on_top = (node.contact_top > 0.05f);
        bool on_left = (node.contact_left > 0.05f);
        bool on_right = (node.contact_right > 0.05f);

        // 依附在身体边缘腹部
        if (on_bottom) {
            p.base_x = node.x + p.offset_x;
            p.base_y = node.y + node.radius_y * 0.75f;
        } else if (on_top) {
            p.base_x = node.x + p.offset_x;
            p.base_y = node.y - node.radius_y * 0.75f;
        } else if (on_left) {
            p.base_x = node.x - node.radius_x * 0.75f;
            p.base_y = node.y + p.offset_x;
        } else if (on_right) {
            p.base_x = node.x + node.radius_x * 0.75f;
            p.base_y = node.y + p.offset_x;
        } else {
            p.base_x = node.x + p.offset_x;
            p.base_y = node.y + node.radius_y * 0.65f;
        }

        // 计算该小触手的当前蠕动步态相位
        float cur_phase = podia_wave_phase + p.phase;
        float cycle_s = std::sin(cur_phase);
        float cycle_c = std::cos(cur_phase);

        if (on_bottom) {
            // 地面水平漫步蠕动
            float step_x = cycle_c * 4.5f * creep_dir;
            float step_y = (cycle_s > 0.0f) ? (cycle_s * -2.5f) : 1.5f;

            p.cur_tip_x = p.base_x + step_x;
            p.cur_tip_y = std::min((float)(SCREEN_H - 1), p.base_y + p.leg_len + step_y);
            p.is_planted = (cycle_s <= 0.0f);

            if (p.is_planted) {
                total_creep_impulse_x += creep_dir * 0.035f;
                planted_count++;
            }
        } else if (on_left || on_right) {
            // 侧壁垂直漫步蠕动
            float step_y = cycle_c * 4.5f * creep_dir;
            float step_x = (cycle_s > 0.0f) ? (cycle_s * (on_left ? 2.5f : -2.5f)) : 0.0f;

            p.cur_tip_x = on_left ? std::max(1.0f, p.base_x - p.leg_len + step_x) : std::min((float)(SCREEN_W - 1), p.base_x + p.leg_len + step_x);
            p.cur_tip_y = p.base_y + step_y;
            p.is_planted = (cycle_s <= 0.0f);

            if (p.is_planted) {
                total_creep_impulse_y += creep_dir * 0.035f;
                planted_count++;
            }
        } else {
            p.cur_tip_x = p.base_x + cycle_c * 3.5f;
            p.cur_tip_y = p.base_y + p.leg_len + cycle_s * 2.0f;
            p.is_planted = false;
        }
    }

    // 当没有大动作牵引且有小触手踩地时，施加轻柔平缓的流体蠕动微位移
    if (!grapple.active && planted_count > 0) {
        skeleton.applyCreepMotion(total_creep_impulse_x, total_creep_impulse_y);
    }
}

void TentacleRenderer::drawMicroPodia(M5Canvas &canvas) const {
    if (grapple.stage == GRAPPLE_SWING) return;

    for (int i = 0; i < MAX_MICRO_PODIA; ++i) {
        const MicroPodia &p = podia[i];
        int bx = (int)std::round(p.base_x);
        int by = (int)std::round(p.base_y);
        int tx = (int)std::round(p.cur_tip_x);
        int ty = (int)std::round(p.cur_tip_y);

        // 绘制微小流体触手小腿 (2px 粗纯黑柔性肉丝)
        canvas.drawLine(bx, by, tx, ty, COLOR_VENOM_CORE);
        canvas.drawLine(bx + 1, by, tx + 1, ty, COLOR_VENOM_CORE);

        // 尖端微小吸盘抓垫 (踩地时带有高光微光)
        if (p.is_planted) {
            canvas.drawPixel(tx, ty, COLOR_GLOW_CYAN); // 荧光吸盘小脚
        } else {
            canvas.drawPixel(tx, ty, COLOR_DITHER_GRAY);
        }
    }
}

void TentacleRenderer::update(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology, bool is_upside_down) {
    // 1. 爬行与秋千抓取触手更新
    updateGrappleCrawl(dt, skeleton, physiology);

    // 2. 皮肤表面微小触手蠕动管足更新
    updateMicroPodia(dt, skeleton, physiology);

    // 3. 全天候多触手高频自发摸索生长 (Ambient Spawning)
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

    // 【1. 荡秋千专用：牛顿摆紧绷吊索与天花板强力吸附大爪掌】
    if (grapple.stage == GRAPPLE_SWING) {
        for (int off = -1; off <= 1; ++off) {
            canvas.drawLine((int)hx + off, (int)hy, (int)sx + off, (int)sy, COLOR_VENOM_CORE);
        }
        canvas.drawLine((int)hx, (int)hy, (int)sx, (int)sy, COLOR_DITHER_GRAY);

        canvas.fillEllipse((int)hx, (int)hy + 1, 9, 5, COLOR_VENOM_CORE);
        canvas.fillCircle((int)hx, (int)hy, 5, COLOR_VENOM_CORE);

        float claw_angles[4] = { -2.4f, -1.8f, -1.3f, -0.7f };
        for (int c = 0; c < 4; ++c) {
            float ca = claw_angles[c];
            float fx = hx + std::cos(ca) * 11.0f;
            float fy = hy + std::abs(std::sin(ca)) * 7.5f;

            canvas.drawLine((int)hx, (int)hy, (int)fx, (int)fy, COLOR_VENOM_CORE);
            canvas.drawLine((int)hx + 1, (int)hy, (int)fx + 1, (int)fy, COLOR_VENOM_CORE);
            canvas.drawPixel((int)fx, (int)fy, COLOR_GLOW_CYAN);
        }
        return;
    }

    // 【2. 常规爬行射出触手与目的地掌心爪盘】
    float cx = (sx + hx) * 0.5f + grapple.ctrl_offset_x * (1.0f - grapple.pull_progress);
    float cy = (sy + hy) * 0.5f + grapple.ctrl_offset_y * (1.0f - grapple.pull_progress);

    constexpr int SEGMENTS = 10;
    float prev_x = sx, prev_y = sy;

    for (int step = 1; step <= SEGMENTS; ++step) {
        float s = (float)step / (float)SEGMENTS;
        float one_minus_s = 1.0f - s;

        float cur_x = one_minus_s * one_minus_s * sx +
                      2.0f * one_minus_s * s * cx +
                      s * s * hx;
        float cur_y = one_minus_s * one_minus_s * sy +
                      2.0f * one_minus_s * s * cy +
                      s * s * hy;

        int thickness = (int)std::round(11.0f * (1.0f - s * 0.40f));
        for (int off = -thickness / 2; off <= thickness / 2; ++off) {
            canvas.drawLine((int)prev_x + off, (int)prev_y, (int)cur_x + off, (int)cur_y, COLOR_VENOM_CORE);
            canvas.drawLine((int)prev_x, (int)prev_y + off, (int)cur_x, (int)cur_y + off, COLOR_VENOM_CORE);
        }

        prev_x = cur_x;
        prev_y = cur_y;
    }

    int palm_r = (int)std::round(6.0f + grapple.palm_spread * 2.5f);
    canvas.fillCircle((int)hx, (int)hy, palm_r, COLOR_VENOM_CORE);

    if (grapple.palm_spread > 0.05f) {
        float dx = hx - sx;
        float dy = hy - sy;
        float main_angle = std::atan2(dy, dx);
        float finger_len = 9.5f + grapple.palm_spread * 7.5f;

        for (int f = -1; f <= 1; ++f) {
            float f_angle = main_angle + (float)f * 0.45f;
            float fx = hx + std::cos(f_angle) * finger_len;
            float fy = hy + std::sin(f_angle) * finger_len;

            for (int off = -1; off <= 1; ++off) {
                canvas.drawLine((int)hx + off, (int)hy, (int)fx + off, (int)fy, COLOR_VENOM_CORE);
                canvas.drawLine((int)hx, (int)hy + off, (int)fx, (int)fy + off, COLOR_VENOM_CORE);
            }
        }
    }
}

void TentacleRenderer::draw(M5Canvas &canvas) const {
    // 1. 绘制皮肤表面微小触手蠕动足 (在身体腹面交替踏步)
    drawMicroPodia(canvas);

    // 2. 绘制爬行射出大触手与掌心爪盘
    drawGrappleTendril(canvas);

    // 3. 绘制倒置吸附微丝
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
