#include "TentacleRenderer.h"
#include <cmath>

TentacleRenderer::TentacleRenderer() {
    for (int i = 0; i < MAX_TENTACLES; ++i) tentacles[i].active = false;
    grapple.active = false;
    initMicroTentacles();
}

void TentacleRenderer::init() {
    for (int i = 0; i < MAX_TENTACLES; ++i) tentacles[i].active = false;
    grapple.active = false;
    auto_spawn_timer = 0.0f;
    is_creeping = false;
    creep_wave_phase = 0.0f;
    initMicroTentacles();
}

void TentacleRenderer::initMicroTentacles() {
    // 精简为 6 根流体黑色附肢 (依附于 Node 1, 2, 3 两侧腹部，彻底消除密恐)
    int idx = 0;
    for (int n = 1; n <= 3 && idx < MAX_MICRO_TENTACLES; ++n) {
        for (int side = 0; side < 2 && idx < MAX_MICRO_TENTACLES; ++side) {
            micro_tentacles[idx].node_idx = n;
            micro_tentacles[idx].side_sign = (side == 0) ? 1.0f : -1.0f;
            micro_tentacles[idx].offset_angle = (side == 0) ? 0.25f : -0.25f;
            micro_tentacles[idx].current_len = 0.0f; // 平时完全隐入体内，保持浑然一体
            micro_tentacles[idx].max_len = 11.5f + (float)(rand() % 20) * 0.1f;
            micro_tentacles[idx].phase_offset = (float)idx * 0.85f;
            idx++;
        }
    }
}

void TentacleRenderer::setCreepMode(bool active, float speed_factor) {
    is_creeping = active;
    creep_speed = speed_factor;
}

void TentacleRenderer::updateMicroTentacles(float dt, const SkeletonSystem &skeleton) {
    if (is_creeping) {
        creep_wave_phase += dt * creep_speed * 11.0f;
    }

    float head_angle = skeleton.getHeadingAngle();
    float norm_angle = head_angle + 1.5708f;

    for (int i = 0; i < MAX_MICRO_TENTACLES; ++i) {
        MicroTentacle &mt = micro_tentacles[i];
        const SkeletonNode &node = skeleton.getNode(mt.node_idx);

        // 仅在 is_creeping 时优雅伸长划地，移动结束立即平滑融回体内
        float target_len = is_creeping ? mt.max_len : 0.0f;
        mt.current_len += (target_len - mt.current_len) * (dt * 10.0f);

        if (mt.current_len < 0.5f) continue;

        // 计算表皮根部锚点
        float base_ang = norm_angle * mt.side_sign + mt.offset_angle;
        float bx = node.x + std::cos(base_ang) * (node.radius_x * 0.90f);
        float by = node.y + std::sin(base_ang) * (node.radius_y * 0.90f);

        // 自然划步推地：撑地向后划动 -> 抬腿向前复位
        float leg_phase = creep_wave_phase + mt.phase_offset;
        float stroke_forward = -std::cos(head_angle) * (std::sin(leg_phase) * mt.current_len * 0.90f);
        float stroke_lateral = std::cos(base_ang) * (mt.current_len * (0.75f + 0.30f * std::cos(leg_phase)));
        float stroke_vertical = std::sin(base_ang) * (mt.current_len * (0.75f + 0.30f * std::cos(leg_phase)));

        mt.tip_x = bx + stroke_forward + stroke_lateral;
        mt.tip_y = by + stroke_vertical;
    }
}

void TentacleRenderer::drawMicroTentacles(M5Canvas &canvas, const SkeletonSystem &skeleton) const {
    for (int i = 0; i < MAX_MICRO_TENTACLES; ++i) {
        const MicroTentacle &mt = micro_tentacles[i];
        if (mt.current_len < 1.0f) continue;

        const SkeletonNode &node = skeleton.getNode(mt.node_idx);
        float head_angle = skeleton.getHeadingAngle();
        float norm_angle = head_angle + 1.5708f;
        float base_ang = norm_angle * mt.side_sign + mt.offset_angle;
        float bx = node.x + std::cos(base_ang) * (node.radius_x * 0.88f);
        float by = node.y + std::sin(base_ang) * (node.radius_y * 0.88f);

        int ibx = (int)std::round(bx);
        int iby = (int)std::round(by);
        int itx = (int)std::round(mt.tip_x);
        int ity = (int)std::round(mt.tip_y);

        // 绘制纯黑锥形流体肉足 (根部 3px 渐变收细至尖端，无任何发光圆点，完全消除密恐)
        for (int off = -1; off <= 1; ++off) {
            canvas.drawLine(ibx + off, iby, itx, ity, COLOR_VENOM_CORE);
            canvas.drawLine(ibx, iby + off, itx, ity, COLOR_VENOM_CORE);
        }
        canvas.drawPixel((ibx + itx) / 2, (iby + ity) / 2, COLOR_DITHER_GRAY); // 柔和微反光线
    }
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

void TentacleRenderer::update(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology, bool is_upside_down) {
    // 1. 爬行与秋千抓取触手更新
    updateGrappleCrawl(dt, skeleton, physiology);

    // 2. 表皮细小腹足触手波浪步态更新
    updateMicroTentacles(dt, skeleton);

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
        // 1.1 绘制紧绷单摆共生体吊索 (从天花板直通毒液球心)
        for (int off = -1; off <= 1; ++off) {
            canvas.drawLine((int)hx + off, (int)hy, (int)sx + off, (int)sy, COLOR_VENOM_CORE);
        }
        canvas.drawLine((int)hx, (int)hy, (int)sx, (int)sy, COLOR_DITHER_GRAY); // 核心高光拉丝

        // 1.2 绘制天花板强力吸附大肉掌 (牢固抓住天花板顶框)
        canvas.fillEllipse((int)hx, (int)hy + 1, 9, 5, COLOR_VENOM_CORE);
        canvas.fillCircle((int)hx, (int)hy, 5, COLOR_VENOM_CORE);

        // 1.3 绘制 4 根锋利张开抠住天花板的共生体利爪
        float claw_angles[4] = { -2.4f, -1.8f, -1.3f, -0.7f };
        for (int c = 0; c < 4; ++c) {
            float ca = claw_angles[c];
            float fx = hx + std::cos(ca) * 11.0f;
            float fy = hy + std::abs(std::sin(ca)) * 7.5f;

            canvas.drawLine((int)hx, (int)hy, (int)fx, (int)fy, COLOR_VENOM_CORE);
            canvas.drawLine((int)hx + 1, (int)hy, (int)fx + 1, (int)fy, COLOR_VENOM_CORE);
            canvas.drawPixel((int)fx, (int)fy, COLOR_GLOW_CYAN); // 爪尖共生体微光
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

    // 绘制目的地掌心肉垫
    int palm_r = (int)std::round(6.0f + grapple.palm_spread * 2.5f);
    canvas.fillCircle((int)hx, (int)hy, palm_r, COLOR_VENOM_CORE);

    // 绘制目的地 3 根张开的利爪
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

void TentacleRenderer::draw(M5Canvas &canvas, const SkeletonSystem &skeleton) const {
    // 1. 绘制表皮细小蠕动触手 (Cilia / Micro-Footpads)
    drawMicroTentacles(canvas, skeleton);

    // 2. 绘制爬行射出触手与掌心爪盘
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
