#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"

enum MouthState {
    MOUTH_IDLE = 0,        // 常规状态 (微咧嘴角, 偶尔露出小舌尖)
    MOUTH_OPEN,            // 怒吼 / 惊吓大张嘴
    MOUTH_STRIKE_TONGUE,   // 变色龙长舌闪电弹射卷虫
    MOUTH_RECEIVE_FEED,    // 大嘴张开接纳触手送虫入腹
    MOUTH_CHEW,            // 咀嚼咬合动效
    MOUTH_LICK             // 满足舔唇一圈
};

class MouthSystem {
public:
    MouthSystem();

    void init();
    void update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, float look_x, float look_y);
    void draw(M5Canvas &canvas, const SkeletonSystem &skeleton) const;

    // 动作触发接口
    void triggerTongueStrike(float target_x, float target_y, int bug_idx = -1);
    void triggerReceiveFeed(int bug_idx = -1);
    void triggerChewAndSwallow();
    void triggerLickLips();

    MouthState getState() const { return state; }
    bool isTongueStriking() const { return state == MOUTH_STRIKE_TONGUE; }
    int getTargetBugIdx() const { return target_bug_idx; }
    void getTongueTipPos(float &tx, float &ty) const { tx = tongue_tip_x; ty = tongue_tip_y; }
    bool isTongueRetracting() const { return tongue_progress > 0.5f; }

private:
    MouthState state = MOUTH_IDLE;
    float state_timer = 0.0f;
    float open_amount = 0.0f;      // 0.0 ~ 1.0 嘴巴开合度
    float chew_phase = 0.0f;
    int chew_count = 0;

    // 变色龙长舌弹射参数
    float strike_target_x = 0.0f;
    float strike_target_y = 0.0f;
    float tongue_progress = 0.0f;  // 0.0 -> 1.0 (射出并回拉)
    float tongue_tip_x = 0.0f;
    float tongue_tip_y = 0.0f;
    int target_bug_idx = -1;

    // 舌头游动与微动作
    float tongue_wave_phase = 0.0f;
    float lick_angle = 0.0f;

    void drawMouthCavityAndTeeth(M5Canvas &canvas, float mx, float my, float face_angle, float mouth_w, float mouth_h, float open_ratio) const;
    void drawTongue(M5Canvas &canvas, float mx, float my, float face_angle, float open_ratio) const;
};
