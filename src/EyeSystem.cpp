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
    next_blink_interval = 2.0f + (rand() % 200) * 0.01f;
}

void EyeSystem::triggerBlink() {
    if (!is_blinking) {
        is_blinking = true;
        blink_phase = 0.0f;
    }
}

void EyeSystem::updateBlinking(float dt, bool is_sleep, EmotionState emotion) {
    if (is_sleep) {
        eyelid_close = eyelid_close * 0.85f + 1.0f * 0.15f;
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
        eyelid_close = eyelid_close * 0.65f;
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

    // 情绪微颤抖动
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

    // 情绪与声音驱动眼睛大小与瞳孔孔径
    if (emotion == EMOTION_FEAR) {
        // 恐惧：眼白暴睁，瞳孔极度收缩
        current_eye_rx = current_eye_rx * 0.8f + (base_eye_radius * 1.3f) * 0.2f;
        current_eye_ry = current_eye_ry * 0.8f + (base_eye_radius * 1.3f) * 0.2f;
        current_pupil_radius = current_pupil_radius * 0.8f + 2.0f * 0.2f;
    } else if (emotion == EMOTION_ANGER) {
        // 愤怒：眼睛狭缝收缩，瞳孔锐利
        current_eye_rx = current_eye_rx * 0.8f + (base_eye_radius * 1.1f) * 0.2f;
        current_eye_ry = current_eye_ry * 0.8f + (base_eye_radius * 0.75f) * 0.2f;
        current_pupil_radius = current_pupil_radius * 0.8f + 3.2f * 0.2f;
    } else if (emotion == EMOTION_CURIOSITY) {
        // 好奇：瞳孔适度放大，眼神聚焦
        current_eye_rx = current_eye_rx * 0.85f + base_eye_radius * 0.15f;
        current_eye_ry = current_eye_ry * 0.85f + base_eye_radius * 0.15f;
        current_pupil_radius = current_pupil_radius * 0.85f + 5.5f * 0.15f;
    } else {
        // 平静/正常
        current_eye_rx = current_eye_rx * 0.85f + base_eye_radius * 0.15f;
        current_eye_ry = current_eye_ry * 0.85f + base_eye_radius * 0.15f;
        current_pupil_radius = current_pupil_radius * 0.85f + 4.6f * 0.15f;
    }
}

void EyeSystem::update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology,
                       float target_look_world_x, float target_look_world_y, bool is_sleep) {
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

    updateBlinking(dt, is_sleep, emotion);
    updatePupilPhysics(dt, target_dx, target_dy, physiology);
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

    canvas.fillEllipse(ex, ey, rx, ry, COLOR_EYE_WHITE);

    if (eyelid_close < 0.7f) {
        int px = (int)std::round(current_eye_x + pupil_offset_x);
        int py = (int)std::round(current_eye_y + pupil_offset_y);
        int pr = (int)std::round(current_pupil_radius * (1.0f - eyelid_close * 0.5f));
        if (pr < 1) pr = 1;

        canvas.fillCircle(px, py, pr, COLOR_EYE_PUPIL);

        if (pr >= 3) {
            canvas.drawPixel(px - 1, py - 1, COLOR_EYE_WHITE);
        }
    }
}
