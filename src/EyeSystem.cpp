#include "EyeSystem.h"
#include <cmath>

EyeSystem::EyeSystem() {
    current_eye_x = SCREEN_W * 0.5f;
    current_eye_y = SCREEN_H * 0.6f;
}

void EyeSystem::init() {
    current_eye_x = SCREEN_W * 0.5f;
    current_eye_y = SCREEN_H * 0.6f;
    pupil_offset_x = 0.0f;
    pupil_offset_y = 0.0f;
    pupil_vx = 0.0f;
    pupil_vy = 0.0f;
    eyelid_close = 0.0f;
    is_blinking = false;
    blink_timer = 0.0f;
    next_blink_interval = 2.5f + (rand() % 150) * 0.01f;
    blood_pulse_phase = 0.0f;
}

void EyeSystem::triggerBlink() {
    if (!is_blinking) {
        is_blinking = true;
        blink_phase = 0.0f;
    }
}

void EyeSystem::updateBlinking(float dt, bool is_sleep, bool is_sleep_peek, EmotionState emotion) {
    if (is_sleep) {
        // 睡眠中半睁眼微眯：开合度插值到 0.45f；深睡时完全闭合 (1.0f)
        float target_close = is_sleep_peek ? 0.45f : 1.0f;
        eyelid_close = eyelid_close * 0.85f + target_close * 0.15f;
        return;
    }

    if (is_blinking) {
        float blink_speed = (emotion == EMOTION_FEAR || emotion == EMOTION_STRESS) ? 18.0f : 12.0f;
        blink_phase += dt * blink_speed;
        if (blink_phase < 1.0f) {
            eyelid_close = blink_phase;
        } else if (blink_phase < 2.0f) {
            eyelid_close = 2.0f - blink_phase;
        } else {
        eyelid_close = 0.0f;
            is_blinking = false;
            blink_timer = 0.0f;
            float base_int = (emotion == EMOTION_STRESS) ? 1.5f : ((emotion == EMOTION_CURIOSITY) ? 4.5f : 3.0f);
            next_blink_interval = base_int + (rand() % 150) * 0.01f;
        }
    } else {
        // 疲惫/困倦时眼皮自然下垂耷拉 (Droopy Eyelid, 40% 耷拉)
        float resting_eyelid = (emotion == EMOTION_EXHAUSTED) ? 0.40f : 0.0f;
        eyelid_close = eyelid_close * 0.70f + resting_eyelid * 0.30f;
        if (eyelid_close < 0.02f) eyelid_close = 0.0f;
        blink_timer += dt;
        if (blink_timer >= next_blink_interval) {
            triggerBlink();
        }
    }
}

void EyeSystem::updatePupilPhysics(float dt, float target_dx, float target_dy, const PhysiologySystem &physiology) {
    EmotionState emotion = physiology.getEmotion();
    float tension = physiology.getNeuroTension();

    float max_travel = current_eye_rx * 0.42f;
    float dist = std::sqrt(target_dx * target_dx + target_dy * target_dy);
    float desired_x = 0.0f;
    float desired_y = 0.0f;

    if (dist > 0.001f) {
        float scale = std::min(dist, max_travel);
        desired_x = (target_dx / dist) * scale;
        desired_y = (target_dy / dist) * scale;
    }

    if (emotion == EMOTION_FEAR || emotion == EMOTION_STRESS) {
        saccade_offset_x = ((rand() % 60) - 30) * 0.08f * tension;
        saccade_offset_y = ((rand() % 60) - 30) * 0.08f * tension;
    }

    desired_x += saccade_offset_x;
    desired_y += saccade_offset_y;

    float stiffness = (emotion == EMOTION_FEAR || emotion == EMOTION_ANGER) ? 0.35f : 0.14f;
    float damping = (emotion == EMOTION_FEAR) ? 0.65f : 0.82f;

    float fx = (desired_x - pupil_offset_x) * stiffness;
    float fy = (desired_y - pupil_offset_y) * stiffness;

    pupil_vx = (pupil_vx + fx) * damping;
    pupil_vy = (pupil_vy + fy) * damping;

    pupil_offset_x += pupil_vx * dt * 30.0f;
    pupil_offset_y += pupil_vy * dt * 30.0f;

    if (emotion == EMOTION_FEAR) {
        current_eye_rx = current_eye_rx * 0.8f + (base_eye_radius * 1.3f) * 0.2f;
        current_eye_ry = current_eye_ry * 0.8f + (base_eye_radius * 1.3f) * 0.2f;
        current_pupil_radius = current_pupil_radius * 0.8f + 2.0f * 0.2f;
    } else if (emotion == EMOTION_ANGER) {
        current_eye_rx = current_eye_rx * 0.8f + (base_eye_radius * 1.1f) * 0.2f;
        current_eye_ry = current_eye_ry * 0.8f + (base_eye_radius * 0.75f) * 0.2f;
        current_pupil_radius = current_pupil_radius * 0.8f + 3.2f * 0.2f;
    } else if (emotion == EMOTION_CURIOSITY) {
        current_eye_rx = current_eye_rx * 0.85f + base_eye_radius * 0.15f;
        current_eye_ry = current_eye_ry * 0.85f + base_eye_radius * 0.15f;
        current_pupil_radius = current_pupil_radius * 0.85f + 5.5f * 0.15f;
    } else {
        current_eye_rx = current_eye_rx * 0.85f + base_eye_radius * 0.15f;
        current_eye_ry = current_eye_ry * 0.85f + base_eye_radius * 0.15f;
        current_pupil_radius = current_pupil_radius * 0.85f + 4.6f * 0.15f;
    }
}

void EyeSystem::update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology,
                       float target_look_world_x, float target_look_world_y, bool is_sleep, bool is_sleep_peek) {
    const SkeletonNode &head = skeleton.getNode(0);
    const SkeletonNode &neck = skeleton.getNode(1);

    float dir_x = head.x - neck.x;
    float dir_y = head.y - neck.y;
    float dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
    if (dir_len > 0.001f) {
        dir_x /= dir_len;
        dir_y /= dir_len;
    } else {
        dir_x = 1.0f;
        dir_y = 0.0f;
    }

    float eye_target_x = head.x + head.bleb_offset_x + dir_x * (head.radius_x * 0.25f);
    float eye_target_y = head.y + head.bleb_offset_y + dir_y * (head.radius_y * 0.25f) - 1.5f;

    current_eye_x = current_eye_x * 0.70f + eye_target_x * 0.30f;
    current_eye_y = current_eye_y * 0.70f + eye_target_y * 0.30f;

    EmotionState emotion = physiology.getEmotion();
    float tension = physiology.getNeuroTension();

    // 推进血丝呼吸脉动
    blood_pulse_phase += dt * (2.2f + tension * 4.0f);

    saccade_timer += dt;
    if (saccade_timer > 1.2f) {
        saccade_timer = 0.0f;
        if ((rand() % 100) < 60) {
            saccade_offset_x = ((rand() % 60) - 30) * 0.05f;
            saccade_offset_y = ((rand() % 60) - 30) * 0.05f;
        } else {
            saccade_offset_x = 0.0f;
            saccade_offset_y = 0.0f;
        }
    }

    float target_dx = target_look_world_x - current_eye_x;
    float target_dy = target_look_world_y - current_eye_y;

    updateBlinking(dt, is_sleep, is_sleep_peek, emotion);
    updatePupilPhysics(dt, target_dx, target_dy, physiology);
}

void EyeSystem::drawBloodVeins(M5Canvas &canvas, int ex, int ey, int rx, int ry, const PhysiologySystem &physiology) const {
    EmotionState emotion = physiology.getEmotion();
    float tension = physiology.getNeuroTension();
    float stress = physiology.getStress();

    // 计算若隐若现的血丝充血可见度与颜色
    float pulse = 0.45f + 0.40f * std::sin(blood_pulse_phase);
    float intensity = pulse * (0.35f + tension * 0.40f + stress * 0.35f);

    if (emotion == EMOTION_FEAR || emotion == EMOTION_ANGER) {
        intensity = std::min(1.0f, intensity + 0.30f);
    }

    // 根据充血程度选择颜色：暗红 (0x7800) -> 鲜红 (0xC800) -> 亮红 (0xF800)
    uint16_t vein_color_main = 0x8800;
    uint16_t vein_color_sub  = 0x6000;

    if (intensity > 0.65f) {
        vein_color_main = 0xE800; // 鲜红
        vein_color_sub  = 0x9800; // 次级分支暗红
    } else if (intensity > 0.40f) {
        vein_color_main = 0xB000;
        vein_color_sub  = 0x7800;
    }

    auto drawVeinPixel = [&](int px, int py, uint16_t col) {
        float dx = (float)(px - ex);
        float dy = (float)(py - ey);
        float norm = (dx * dx) / (float)(rx * rx) + (dy * dy) / (float)(ry * ry);
        if (norm <= 0.86f) {
            canvas.drawPixel(px, py, col);
        }
    };

    // 1. 左眼角向心分支血丝
    int lx = ex - rx + 2;
    int ly = ey;
    drawVeinPixel(lx, ly, vein_color_main);
    drawVeinPixel(lx + 1, ly - 1, vein_color_main);
    drawVeinPixel(lx + 2, ly - 1, vein_color_main);
    drawVeinPixel(lx + 3, ly, vein_color_main);
    drawVeinPixel(lx + 4, ly, vein_color_main);
    drawVeinPixel(lx + 5, ly + 1, vein_color_main);

    // 左眼角向上分支小毛细
    drawVeinPixel(lx + 2, ly - 2, vein_color_sub);
    drawVeinPixel(lx + 3, ly - 3, vein_color_sub);

    // 2. 右眼角向心分支血丝
    int rx_pos = ex + rx - 2;
    int ry_pos = ey;
    drawVeinPixel(rx_pos, ry_pos, vein_color_main);
    drawVeinPixel(rx_pos - 1, ry_pos, vein_color_main);
    drawVeinPixel(rx_pos - 2, ry_pos + 1, vein_color_main);
    drawVeinPixel(rx_pos - 3, ry_pos + 1, vein_color_main);
    drawVeinPixel(rx_pos - 4, ry_pos, vein_color_main);
    drawVeinPixel(rx_pos - 5, ry_pos, vein_color_main);

    // 右眼角向下分支小毛细
    drawVeinPixel(rx_pos - 2, ry_pos + 2, vein_color_sub);
    drawVeinPixel(rx_pos - 3, ry_pos + 3, vein_color_sub);

    // 3. 左上方斜向下分支血丝
    int tx1 = ex - rx / 2;
    int ty1 = ey - ry + 2;
    drawVeinPixel(tx1, ty1, vein_color_main);
    drawVeinPixel(tx1 + 1, ty1 + 1, vein_color_main);
    drawVeinPixel(tx1 + 1, ty1 + 2, vein_color_main);
    drawVeinPixel(tx1 + 2, ty1 + 3, vein_color_main);

    // 4. 右上方斜向下分支血丝
    int tx2 = ex + rx / 2;
    int ty2 = ey - ry + 2;
    drawVeinPixel(tx2, ty2, vein_color_main);
    drawVeinPixel(tx2 - 1, ty2 + 1, vein_color_main);
    drawVeinPixel(tx2 - 1, ty2 + 2, vein_color_main);
    drawVeinPixel(tx2 - 2, ty2 + 3, vein_color_main);

    // 5. 下方微血丝 (更细弱)
    if (intensity > 0.35f) {
        int bx = ex;
        int by = ey + ry - 2;
        drawVeinPixel(bx, by, vein_color_sub);
        drawVeinPixel(bx - 1, by - 1, vein_color_sub);
        drawVeinPixel(bx - 1, by - 2, vein_color_sub);
    }
}

void EyeSystem::draw(M5Canvas &canvas, const PhysiologySystem &physiology) const {
    int ex = (int)std::round(current_eye_x);
    int ey = (int)std::round(current_eye_y);

    if (eyelid_close >= 0.98f) {
        int w = (int)current_eye_rx;
        canvas.drawFastHLine(ex - w, ey, w * 2, COLOR_VENOM_BLACK);
        canvas.drawFastHLine(ex - w + 2, ey + 1, (w - 2) * 2, COLOR_VENOM_BLACK);
        return;
    }

    int rx = (int)std::round(current_eye_rx);
    int ry = (int)std::round(current_eye_ry * (1.0f - eyelid_close * 0.85f));
    if (ry < 1) ry = 1;

    // 1. 绘制白色椭圆眼白
    canvas.fillEllipse(ex, ey, rx, ry, COLOR_EYE_WHITE);

    // 2. 绘制若隐若现的红色血丝微血管与眼角充血
    if (eyelid_close < 0.85f) {
        drawBloodVeins(canvas, ex, ey, rx, ry, physiology);
    }

    // 3. 绘制深色瞳孔（覆盖在血丝上，具有清晰的边缘）
    if (eyelid_close < 0.7f) {
        int px = (int)std::round(current_eye_x + pupil_offset_x);
        int py = (int)std::round(current_eye_y + pupil_offset_y);
        int pr = (int)std::round(current_pupil_radius * (1.0f - eyelid_close * 0.5f));
        if (pr < 1) pr = 1;

        canvas.fillCircle(px, py, pr, COLOR_EYE_PUPIL);

        // 4. 瞳孔高光反射亮斑
        if (pr >= 3) {
            canvas.drawPixel(px - 1, py - 1, COLOR_EYE_WHITE);
        }
    }
}
