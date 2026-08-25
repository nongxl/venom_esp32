#include "MouthSystem.h"
#include <cmath>

MouthSystem::MouthSystem() {
    current_mode = FEED_NONE;
    current_stage = FEED_STAGE_IDLE;
}

void MouthSystem::init() {
    current_mode = FEED_NONE;
    current_stage = FEED_STAGE_IDLE;
    stage_timer = 0.0f;
    mouth_open_ratio = 0.0f;
    tongue_progress = 0.0f;
    slime.active = false;
    chew_count = 0;
    trigger_vibe = false;
}

void MouthSystem::updateMouthAnchor(const SkeletonSystem &skeleton) {
    const SkeletonNode &head = skeleton.getNode(0);
    const SkeletonNode &neck = skeleton.getNode(1);

    float dx = head.x - neck.x;
    float dy = head.y - neck.y;
    float len = std::sqrt(dx * dx + dy * dy);

    if (len > 0.01f) {
        dx /= len;
        dy /= len;
    } else {
        dx = 0.0f;
        dy = 1.0f;
    }

    // 嘴巴位于头部骨架稍稍靠前下侧
    mouth_x = head.x + dx * 4.5f;
    mouth_y = head.y + dy * 4.5f + 3.0f;
    mouth_angle = std::atan2(dy, dx);

    tongue_root_x = mouth_x;
    tongue_root_y = mouth_y;
}

bool MouthSystem::startPredation(FeedMode mode, float bug_x, float bug_y) {
    if (current_mode != FEED_NONE) return false;

    current_mode = mode;
    target_bug_x = bug_x;
    target_bug_y = bug_y;
    stage_timer = 0.0f;

    switch (mode) {
        case FEED_TONGUE:
            current_stage = FEED_STAGE_OPEN_MOUTH;
            tongue_progress = 0.0f;
            break;

        case FEED_TENTACLE:
            current_stage = FEED_STAGE_TENTACLE_PULL;
            break;

        case FEED_SLIME_STALK:
            current_stage = FEED_STAGE_SLIME_FLY;
            slime.active = true;
            slime.x = mouth_x;
            slime.y = mouth_y;
            slime.target_x = bug_x;
            slime.target_y = bug_y;
            slime.progress = 0.0f;
            break;

        default:
            current_mode = FEED_NONE;
            return false;
    }

    return true;
}

bool MouthSystem::checkAndConsumeChompVibration() {
    if (trigger_vibe) {
        trigger_vibe = false;
        return true;
    }
    return false;
}

void MouthSystem::update(float dt, const SkeletonSystem &skeleton, BugSystem &bugs,
                        TentacleRenderer &tentacles, PhysiologySystem &physiology) {
    updateMouthAnchor(skeleton);

    if (current_mode == FEED_NONE) {
        // 平时嘴巴微微闭合，伴随呼吸微弱张合 (0.0 ~ 0.08)
        mouth_open_ratio = 0.03f + 0.03f * std::sin(stage_timer * 2.0f);
        stage_timer += dt;
        return;
    }

    stage_timer += dt;

    switch (current_mode) {
        case FEED_TONGUE: {
            // 变色龙闪电卷舌
            if (current_stage == FEED_STAGE_OPEN_MOUTH) {
                // 1. 大嘴迅猛裂开 (0.08s)
                mouth_open_ratio = std::min(1.0f, stage_timer / 0.08f);
                if (stage_timer >= 0.08f) {
                    current_stage = FEED_STAGE_TONGUE_EXTEND;
                    stage_timer = 0.0f;
                }
            } else if (current_stage == FEED_STAGE_TONGUE_EXTEND) {
                // 2. 粉红长舌闪电弹射命中虫子 (0.12s)
                constexpr float DURATION = 0.12f;
                tongue_progress = std::min(1.0f, stage_timer / DURATION);
                tongue_tip_x = tongue_root_x + (target_bug_x - tongue_root_x) * tongue_progress;
                tongue_tip_y = tongue_root_y + (target_bug_y - tongue_root_y) * tongue_progress;

                // 舌头中间带有弹簧下垂弧度
                tongue_ctrl_x = (tongue_root_x + tongue_tip_x) * 0.5f;
                tongue_ctrl_y = (tongue_root_y + tongue_tip_y) * 0.5f + 12.0f * (1.0f - tongue_progress);

                if (stage_timer >= DURATION) {
                    bugs.setBugCaptured(target_bug_x, target_bug_y);
                    current_stage = FEED_STAGE_TONGUE_RETRACT;
                    stage_timer = 0.0f;
                }
            } else if (current_stage == FEED_STAGE_TONGUE_RETRACT) {
                // 3. 舌头卷回虫子拉入口中 (0.14s)
                constexpr float DURATION = 0.14f;
                float t = std::min(1.0f, stage_timer / DURATION);
                tongue_progress = 1.0f - t;
                tongue_tip_x = tongue_root_x + (target_bug_x - tongue_root_x) * tongue_progress;
                tongue_tip_y = tongue_root_y + (target_bug_y - tongue_root_y) * tongue_progress;
                tongue_ctrl_y = (tongue_root_y + tongue_tip_y) * 0.5f + 8.0f * tongue_progress;

                bugs.setBugCaptured(tongue_tip_x, tongue_tip_y);

                if (stage_timer >= DURATION) {
                    bugs.setBugEaten();
                    current_stage = FEED_STAGE_CHEWING;
                    stage_timer = 0.0f;
                    chew_count = 3;
                    chew_phase = 0.0f;
                    trigger_vibe = true;
                    physiology.feedNutrition(0.25f, 0.20f);
                }
            } else if (current_stage == FEED_STAGE_CHEWING) {
                // 4. 咔嚓咔嚓咀嚼动画 (0.55s)
                chew_phase += dt * 18.0f;
                mouth_open_ratio = 0.35f + 0.45f * std::abs(std::sin(chew_phase));

                if (stage_timer >= 0.55f) {
                    mouth_open_ratio = 0.0f;
                    current_mode = FEED_NONE;
                    current_stage = FEED_STAGE_IDLE;
                }
            }
            break;
        }

        case FEED_TENTACLE: {
            // 触手抓捕塞入口中
            if (current_stage == FEED_STAGE_TENTACLE_PULL) {
                mouth_open_ratio = std::min(0.85f, stage_timer * 1.5f);
                if (tentacles.getGrappleStage() == GRAPPLE_FUSE || stage_timer >= 0.75f) {
                    bugs.setBugEaten();
                    current_stage = FEED_STAGE_CHEWING;
                    stage_timer = 0.0f;
                    chew_count = 3;
                    chew_phase = 0.0f;
                    trigger_vibe = true;
                    physiology.feedNutrition(0.25f, 0.20f);
                }
            } else if (current_stage == FEED_STAGE_CHEWING) {
                chew_phase += dt * 18.0f;
                mouth_open_ratio = 0.35f + 0.45f * std::abs(std::sin(chew_phase));

                if (stage_timer >= 0.55f) {
                    mouth_open_ratio = 0.0f;
                    current_mode = FEED_NONE;
                    current_stage = FEED_STAGE_IDLE;
                }
            }
            break;
        }

        case FEED_SLIME_STALK: {
            // 喷射黏液黏住爬近吞噬
            if (current_stage == FEED_STAGE_SLIME_FLY) {
                mouth_open_ratio = 0.75f;
                constexpr float FLY_TIME = 0.20f;
                slime.progress = std::min(1.0f, stage_timer / FLY_TIME);
                slime.x = mouth_x + (slime.target_x - mouth_x) * slime.progress;
                slime.y = mouth_y + (slime.target_y - mouth_y) * slime.progress;

                if (stage_timer >= FLY_TIME) {
                    slime.active = false;
                    bugs.setBugSlimed();
                    mouth_open_ratio = 0.1f;
                    current_stage = FEED_STAGE_OPEN_MOUTH; // 爬到附近后张嘴吃
                    stage_timer = 0.0f;
                }
            } else if (current_stage == FEED_STAGE_OPEN_MOUTH) {
                float dx = target_bug_x - mouth_x;
                float dy = target_bug_y - mouth_y;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist <= 18.0f) {
                    mouth_open_ratio = 0.95f;
                    bugs.setBugEaten();
                    current_stage = FEED_STAGE_CHEWING;
                    stage_timer = 0.0f;
                    chew_count = 3;
                    chew_phase = 0.0f;
                    trigger_vibe = true;
                    physiology.feedNutrition(0.25f, 0.20f);
                }
            } else if (current_stage == FEED_STAGE_CHEWING) {
                chew_phase += dt * 18.0f;
                mouth_open_ratio = 0.35f + 0.45f * std::abs(std::sin(chew_phase));

                if (stage_timer >= 0.55f) {
                    mouth_open_ratio = 0.0f;
                    current_mode = FEED_NONE;
                    current_stage = FEED_STAGE_IDLE;
                }
            }
            break;
        }

        default:
            current_mode = FEED_NONE;
            break;
    }
}

void MouthSystem::drawMouthAndTeeth(M5Canvas &canvas) const {
    int mx = (int)mouth_x;
    int my = (int)mouth_y;

    if (mouth_open_ratio < 0.12f) {
        // 【平时微闭嘴型】：一条略带俏皮弧度的深黑红裂缝，露出一颗小白尖牙 (又凶又萌)
        canvas.drawLine(mx - 5, my, mx + 5, my, 0x1800);
        canvas.drawPixel(mx, my + 1, 0x1800);
        // 微露 1 颗可爱小尖牙
        canvas.drawPixel(mx - 1, my + 1, 0xFFFF);
        canvas.drawPixel(mx - 1, my + 2, 0xFFFF);
        return;
    }

    // 【张嘴/进食/咀嚼嘴型】：深暗红内腔 + 上下两排锯齿状锐利白尖牙！
    int rx = (int)(mouth_width * 0.5f);
    int ry = (int)(mouth_height * 0.5f * mouth_open_ratio) + 2;

    // 1. 深红黑色口腔内部
    uint16_t mouth_cavity_col = 0x3000; // 暗深红
    canvas.fillEllipse(mx, my, rx, ry, mouth_cavity_col);
    canvas.drawEllipse(mx, my, rx, ry, COLOR_VENOM_CORE);

    // 2. 上排白色锯齿尖牙 (3~4 颗)
    for (int i = -2; i <= 2; ++i) {
        if (i == 0) continue;
        int tx = mx + i * 3;
        int ty = my - ry + 1;
        int tooth_h = (std::abs(i) == 1) ? 4 : 3;
        canvas.fillTriangle(tx - 1, ty, tx + 1, ty, tx, ty + tooth_h, 0xFFFF);
    }

    // 3. 下排白色锯齿尖牙 (3 颗)
    for (int i = -1; i <= 1; ++i) {
        int tx = mx + i * 4;
        int ty = my + ry - 1;
        int tooth_h = 3;
        canvas.fillTriangle(tx - 1, ty, tx + 1, ty, tx, ty - tooth_h, 0xFFFF);
    }
}

void MouthSystem::drawChameleonTongue(M5Canvas &canvas) const {
    if (current_mode != FEED_TONGUE || tongue_progress <= 0.02f) return;

    constexpr int SEGMENTS = 10;
    float prev_x = tongue_root_x;
    float prev_y = tongue_root_y;

    uint16_t tongue_color = 0xFACB; // 变色龙粉嫩亮粉红
    uint16_t tongue_core  = 0xF814; // 深粉红核心

    // 绘制弹性变色龙长卷舌
    for (int step = 1; step <= SEGMENTS; ++step) {
        float s = (float)step / (float)SEGMENTS;
        float one_minus_s = 1.0f - s;

        float cur_x = one_minus_s * one_minus_s * tongue_root_x +
                      2.0f * one_minus_s * s * tongue_ctrl_x +
                      s * s * tongue_tip_x;
        float cur_y = one_minus_s * one_minus_s * tongue_root_y +
                      2.0f * one_minus_s * s * tongue_ctrl_y +
                      s * s * tongue_tip_y;

        int thick = (s < 0.5f) ? 3 : 2;
        canvas.drawLine((int)prev_x, (int)prev_y, (int)cur_x, (int)cur_y, tongue_color);
        if (thick > 2) {
            canvas.drawLine((int)prev_x + 1, (int)prev_y, (int)cur_x + 1, (int)cur_y, tongue_core);
        }

        prev_x = cur_x;
        prev_y = cur_y;
    }

    // 舌尖带有饱满的粉红黏液吸盘球
    int tip_x = (int)tongue_tip_x;
    int tip_y = (int)tongue_tip_y;
    canvas.fillCircle(tip_x, tip_y, 3, tongue_core);
    canvas.drawCircle(tip_x, tip_y, 3, tongue_color);
    canvas.drawPixel(tip_x - 1, tip_y - 1, 0xFFFF); // 吸盘高光
}

void MouthSystem::drawSlimeProjectile(M5Canvas &canvas) const {
    if (!slime.active) return;
    int sx = (int)slime.x;
    int sy = (int)slime.y;

    // 吐出的青荧光黏液飞弹
    canvas.fillCircle(sx, sy, 4, COLOR_GLOW_CYAN);
    canvas.fillCircle(sx, sy, 2, 0xFFFF);
}

void MouthSystem::draw(M5Canvas &canvas, const SkeletonSystem &skeleton) const {
    // 1. 变色龙舌头 (在嘴唇后方伸出)
    drawChameleonTongue(canvas);

    // 2. 黏液飞弹
    drawSlimeProjectile(canvas);

    // 3. 又凶又萌的嘴巴与白尖牙
    drawMouthAndTeeth(canvas);
}
