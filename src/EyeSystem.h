#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"

class EyeSystem {
public:
    EyeSystem();

    void init();
    void update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology,
                float target_look_world_x, float target_look_world_y, bool is_sleep);
    void draw(M5Canvas &canvas, const PhysiologySystem &physiology) const;

    void triggerBlink();

    float getEyeX() const { return current_eye_x; }
    float getEyeY() const { return current_eye_y; }

private:
    float current_eye_x = 120.0f;
    float current_eye_y = 90.0f;

    // 瞳孔位置与阻尼物理
    float pupil_offset_x = 0.0f;
    float pupil_offset_y = 0.0f;
    float pupil_vx = 0.0f;
    float pupil_vy = 0.0f;

    // 眼睛几何大小
    float base_eye_radius = 11.0f;
    float current_eye_rx = 11.0f;
    float current_eye_ry = 11.0f;
    float current_pupil_radius = 4.8f;

    // 眨眼与眼睑动画
    float eyelid_close = 0.0f;
    float blink_timer = 0.0f;
    float next_blink_interval = 2.5f;
    bool is_blinking = false;
    float blink_phase = 0.0f;

    // 眼神偶发微颤 (Saccade & Tremor)
    float saccade_timer = 0.0f;
    float saccade_offset_x = 0.0f;
    float saccade_offset_y = 0.0f;

    // 红色血丝脉动与充血相位
    float blood_pulse_phase = 0.0f;

    void updateBlinking(float dt, bool is_sleep, EmotionState emotion);
    void updatePupilPhysics(float dt, float target_dx, float target_dy, const PhysiologySystem &physiology);
    void drawBloodVeins(M5Canvas &canvas, int ex, int ey, int rx, int ry, const PhysiologySystem &physiology) const;
};
