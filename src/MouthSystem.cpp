#include "MouthSystem.h"
#include <cmath>

MouthSystem::MouthSystem() {
    init();
}

void MouthSystem::init() {
    state = MOUTH_IDLE;
    state_timer = 0.0f;
    open_amount = 0.25f; // 微咧坏笑
    chew_phase = 0.0f;
    chew_count = 0;
    tongue_progress = 0.0f;
    target_bug_idx = -1;
    tongue_wave_phase = 0.0f;
    lick_angle = 0.0f;
}

void MouthSystem::triggerTongueStrike(float target_x, float target_y, int bug_idx) {
    state = MOUTH_STRIKE_TONGUE;
    state_timer = 0.0f;
    tongue_progress = 0.0f;
    strike_target_x = target_x;
    strike_target_y = target_y;
    target_bug_idx = bug_idx;
    open_amount = 1.0f; // 猛张大嘴
}

void MouthSystem::triggerReceiveFeed(int bug_idx) {
    state = MOUTH_RECEIVE_FEED;
    state_timer = 0.0f;
    target_bug_idx = bug_idx;
    open_amount = 0.95f; // 大嘴张开准备接虫
}

void MouthSystem::triggerChewAndSwallow() {
    state = MOUTH_CHEW;
    state_timer = 0.0f;
    chew_phase = 0.0f;
    chew_count = 0;
    open_amount = 0.5f;
    target_bug_idx = -1;
}

void MouthSystem::triggerLickLips() {
    state = MOUTH_LICK;
    state_timer = 0.0f;
    lick_angle = 0.0f;
    open_amount = 0.35f;
}

void MouthSystem::update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, float look_x, float look_y) {
    state_timer += dt;
    tongue_wave_phase += dt * 4.5f;

    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    switch (state) {
        case MOUTH_IDLE: {
            // 常规状态：随呼吸和张力微幅开合 (0.15 ~ 0.35)
            float tension = physiology.getNeuroTension();
            float base_open = 0.20f + tension * 0.25f;
            float breath = std::sin(tongue_wave_phase * 0.8f) * 0.08f;
            open_amount = std::max(0.10f, std::min(0.60f, base_open + breath));
            break;
        }

        case MOUTH_OPEN: {
            open_amount = 0.95f;
            if (state_timer > 1.2f) {
                state = MOUTH_IDLE;
            }
            break;
        }

        case MOUTH_STRIKE_TONGUE: {
            // 变色龙闪电长鞭卷舌：全程仅需 0.30s (0.15s 弹射 + 0.15s 回拉)
            constexpr float TOTAL_TIME = 0.32f;
            tongue_progress = state_timer / TOTAL_TIME;
            open_amount = 1.0f;

            float reach_factor;
            if (tongue_progress < 0.5f) {
                // 弹射阶段 (0.0 -> 1.0 极速前进)
                reach_factor = (tongue_progress / 0.5f);
                reach_factor = std::sin(reach_factor * 1.5708f); // 缓入极速
            } else {
                // 回拉阶段 (1.0 -> 0.0 闪电收回)
                reach_factor = 1.0f - ((tongue_progress - 0.5f) / 0.5f);
                reach_factor = reach_factor * reach_factor; // 爆发力回缩
            }

            // 计算舌尖当前坐标
            tongue_tip_x = hx + (strike_target_x - hx) * reach_factor;
            tongue_tip_y = hy + 6.0f + (strike_target_y - (hy + 6.0f)) * reach_factor;

            if (tongue_progress >= 1.0f) {
                tongue_progress = 1.0f;
                // 舌头收回口中，进入咀嚼吞噬！
                triggerChewAndSwallow();
            }
            break;
        }

        case MOUTH_RECEIVE_FEED: {
            open_amount = 0.90f + std::sin(state_timer * 10.0f) * 0.10f;
            if (state_timer > 1.8f) {
                triggerChewAndSwallow();
            }
            break;
        }

        case MOUTH_CHEW: {
            // 咀嚼咬合：快速开合 3 次
            chew_phase += dt * 14.0f;
            open_amount = 0.20f + (std::sin(chew_phase) * 0.5f + 0.5f) * 0.55f;

            if (chew_phase > 6.283f * 3.0f) {
                // 咀嚼完成，转入舔嘴唇
                triggerLickLips();
            }
            break;
        }

        case MOUTH_LICK: {
            // 满足地舔嘴唇一圈
            constexpr float LICK_TIME = 0.8f;
            float t = state_timer / LICK_TIME;
            lick_angle = t * 6.28318f;
            open_amount = 0.25f + std::sin(lick_angle) * 0.15f;

            if (state_timer >= LICK_TIME) {
                state = MOUTH_IDLE;
            }
            break;
        }
    }
}

void MouthSystem::drawMouthCavityAndTeeth(M5Canvas &canvas, float mx, float my, float face_angle, float mouth_w, float mouth_h, float open_ratio) const {
    if (open_ratio < 0.08f) return;

    float ca = std::cos(face_angle);
    float sa = std::sin(face_angle);
    float perp_x = -sa;
    float perp_y = ca;

    float half_w = mouth_w * 0.5f;
    float half_h = mouth_h * 0.5f * open_ratio;

    // 1. 绘制暗红深邃口腔底腔 (Deep Crimson Cavity)
    uint16_t cavity_color = canvas.color565(130, 22, 35);
    int p_count = 10;
    int poly_x[10];
    int poly_y[10];

    for (int p = 0; p < p_count; ++p) {
        float angle = (float)p * (6.28318f / (float)p_count);
        float px = std::cos(angle) * half_w;
        float py = std::sin(angle) * half_h;
        poly_x[p] = (int)std::round(mx + ca * px + perp_x * py);
        poly_y[p] = (int)std::round(my + sa * px + perp_y * py);
    }

    // 填充口腔
    for (int p = 1; p < p_count - 1; ++p) {
        canvas.fillTriangle(poly_x[0], poly_y[0], poly_x[p], poly_y[p], poly_x[p+1], poly_y[p+1], cavity_color);
    }

    // 2. 绘制上排锐利白三角锯齿尖牙 (5 颗尖牙)
    constexpr int TOP_TEETH = 5;
    uint16_t tooth_color = TFT_WHITE;
    uint16_t tooth_shadow = canvas.color565(180, 190, 205);

    for (int i = 0; i < TOP_TEETH; ++i) {
        float t_pos = ((float)i / (float)(TOP_TEETH - 1)) * 2.0f - 1.0f; // -1.0 ~ 1.0
        float tx_base = t_pos * (half_w * 0.78f);
        float ty_base = -half_h * 0.72f;

        float tooth_len = (4.0f + (1.0f - std::abs(t_pos)) * 2.5f) * open_ratio;
        float tooth_w = 2.4f;

        // 尖端朝口腔内部下方
        float b1_x = mx + ca * (tx_base - tooth_w) + perp_x * ty_base;
        float b1_y = my + sa * (tx_base - tooth_w) + perp_y * ty_base;
        float b2_x = mx + ca * (tx_base + tooth_w) + perp_x * ty_base;
        float b2_y = my + sa * (tx_base + tooth_w) + perp_y * ty_base;
        float tip_x = mx + ca * tx_base + perp_x * (ty_base + tooth_len);
        float tip_y = my + sa * tx_base + perp_y * (ty_base + tooth_len);

        canvas.fillTriangle((int)b1_x, (int)b1_y, (int)b2_x, (int)b2_y, (int)tip_x, (int)tip_y, tooth_color);
        canvas.drawLine((int)b1_x, (int)b1_y, (int)tip_x, (int)tip_y, tooth_shadow);
    }

    // 3. 绘制下排锐利白三角锯齿尖牙 (4 颗交错尖牙)
    constexpr int BOT_TEETH = 4;
    for (int i = 0; i < BOT_TEETH; ++i) {
        float t_pos = ((float)(i + 0.5f) / (float)BOT_TEETH) * 2.0f - 1.0f;
        float tx_base = t_pos * (half_w * 0.70f);
        float ty_base = half_h * 0.72f;

        float tooth_len = (3.5f + (1.0f - std::abs(t_pos)) * 2.0f) * open_ratio;
        float tooth_w = 2.2f;

        // 尖端朝上
        float b1_x = mx + ca * (tx_base - tooth_w) + perp_x * ty_base;
        float b1_y = my + sa * (tx_base - tooth_w) + perp_y * ty_base;
        float b2_x = mx + ca * (tx_base + tooth_w) + perp_x * ty_base;
        float b2_y = my + sa * (tx_base + tooth_w) + perp_y * ty_base;
        float tip_x = mx + ca * tx_base + perp_x * (ty_base - tooth_len);
        float tip_y = my + sa * tx_base + perp_y * (ty_base - tooth_len);

        canvas.fillTriangle((int)b1_x, (int)b1_y, (int)b2_x, (int)b2_y, (int)tip_x, (int)tip_y, tooth_color);
        canvas.drawLine((int)b2_x, (int)b2_y, (int)tip_x, (int)tip_y, tooth_shadow);
    }

    // 4. 勾勒厚实黑嘴唇轮廓
    for (int p = 0; p < p_count; ++p) {
        int next_p = (p + 1) % p_count;
        canvas.drawLine(poly_x[p], poly_y[p], poly_x[next_p], poly_y[next_p], COLOR_VENOM_CORE);
        canvas.drawLine(poly_x[p]+1, poly_y[p], poly_x[next_p]+1, poly_y[next_p], COLOR_VENOM_CORE);
    }
}

void MouthSystem::drawTongue(M5Canvas &canvas, float mx, float my, float face_angle, float open_ratio) const {
    uint16_t tongue_core = canvas.color565(255, 65, 105);
    uint16_t tongue_light = canvas.color565(255, 135, 165);

    if (state == MOUTH_STRIKE_TONGUE && tongue_progress > 0.01f) {
        // 【变色龙闪电长舌弹射捕食形态】
        constexpr int SEGMENTS = 10;
        float sx = mx;
        float sy = my;
        float ex = tongue_tip_x;
        float ey = tongue_tip_y;

        // 弧线弯曲控制点
        float mid_x = (sx + ex) * 0.5f + std::sin(tongue_wave_phase) * 6.0f;
        float mid_y = (sy + ey) * 0.5f - 8.0f;

        float prev_x = sx;
        float prev_y = sy;

        for (int step = 1; step <= SEGMENTS; ++step) {
            float t = (float)step / (float)SEGMENTS;
            float omt = 1.0f - t;
            float cur_x = omt * omt * sx + 2.0f * omt * t * mid_x + t * t * ex;
            float cur_y = omt * omt * sy + 2.0f * omt * t * mid_y + t * t * ey;

            int thick = (int)std::round(4.2f * (1.0f - t * 0.35f));
            for (int off = -thick / 2; off <= thick / 2; ++off) {
                canvas.drawLine((int)prev_x + off, (int)prev_y, (int)cur_x + off, (int)cur_y, tongue_core);
                canvas.drawLine((int)prev_x, (int)prev_y + off, (int)cur_x, (int)cur_y + off, tongue_core);
            }

            prev_x = cur_x;
            prev_y = cur_y;
        }

        // 舌尖变色龙大肉垫吸盘 (末端卷住虫子)
        canvas.fillCircle((int)ex, (int)ey, 3, tongue_core);
        canvas.drawCircle((int)ex, (int)ey, 3, tongue_light);
        canvas.drawPixel((int)ex, (int)ey, TFT_WHITE);
    }
    else if (state == MOUTH_LICK) {
        // 【满足舔唇一圈形态】
        float lx = mx + std::cos(lick_angle) * 11.0f;
        float ly = my + std::sin(lick_angle) * 6.0f;

        canvas.fillCircle((int)lx, (int)ly, 3, tongue_core);
        canvas.drawLine((int)mx, (int)my, (int)lx, (int)ly, tongue_core);
        canvas.drawPixel((int)lx, (int)ly, tongue_light);
    }
    else if (open_ratio > 0.15f) {
        // 【常规活体游动小舌尖】
        float tip_len = 8.0f + open_ratio * 7.0f;
        float sway = std::sin(tongue_wave_phase) * 3.5f;

        float ca = std::cos(face_angle);
        float sa = std::sin(face_angle);
        float perp_x = -sa;
        float perp_y = ca;

        float t1_x = mx + ca * (tip_len * 0.5f) + perp_x * sway;
        float t1_y = my + sa * (tip_len * 0.5f) + perp_y * sway + 2.0f;
        float t2_x = mx + ca * tip_len + perp_x * (sway * 1.5f);
        float t2_y = my + sa * tip_len + perp_y * (sway * 1.5f) + 4.0f;

        canvas.drawLine((int)mx, (int)my, (int)t1_x, (int)t1_y, tongue_core);
        canvas.drawLine((int)t1_x, (int)t1_y, (int)t2_x, (int)t2_y, tongue_core);
        canvas.fillCircle((int)t2_x, (int)t2_y, 2, tongue_core);
        canvas.drawPixel((int)t2_x, (int)t2_y, tongue_light);
    }
}

void MouthSystem::draw(M5Canvas &canvas, const SkeletonSystem &skeleton) const {
    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    // 计算头部朝向角
    const SkeletonNode &head = skeleton.getNode(0);
    const SkeletonNode &neck = skeleton.getNode(1);
    float dx = head.x - neck.x;
    float dy = head.y - neck.y;
    float face_angle = (std::abs(dx) > 0.1f || std::abs(dy) > 0.1f) ? std::atan2(dy, dx) : 1.57f;

    // 嘴巴基准中心：位于头部中心前下方
    float mx = hx + std::cos(face_angle) * (head.radius_x * 0.18f);
    float my = hy + std::sin(face_angle) * (head.radius_y * 0.18f) + 4.5f;

    float mouth_w = head.radius_x * 1.08f;
    float mouth_h = head.radius_y * 0.70f;

    // 1. 绘制暗红口腔与白色锯齿尖牙
    drawMouthCavityAndTeeth(canvas, mx, my, face_angle, mouth_w, mouth_h, open_amount);

    // 2. 绘制游动/弹射长舌头
    drawTongue(canvas, mx, my, face_angle, open_amount);
}
