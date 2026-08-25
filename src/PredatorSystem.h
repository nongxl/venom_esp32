#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "PreyBugSystem.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"
#include "MetaballSystem.h"

enum HuntAction {
    HUNT_NONE = 0,
    HUNT_TONGUE,        // 动作 1：变色龙闪电长舌卷食
    HUNT_TENTACLE,      // 动作 2：暗黑触手缠绕抓取拖回
    HUNT_MUCUS          // 动作 3：喷射黏液弹定身后爬行包覆吞噬
};

enum HuntPhase {
    PHASE_IDLE = 0,
    PHASE_SHOOT,        // 弹射长舌 / 射出触手 / 飞射黏液弹
    PHASE_RETRACT,      // 卷回长舌 / 拖回触手
    PHASE_CRAWL_ENGULF, // 黏液定身后爬过去包覆吞噬
    PHASE_DIGEST        // 吞噬消化反馈
};

struct HuntContext {
    bool active = false;
    HuntAction action = HUNT_NONE;
    HuntPhase phase = PHASE_IDLE;
    int target_bug_idx = -1;

    float timer = 0.0f;
    float start_x = 120.0f;
    float start_y = 100.0f;
    float target_x = 120.0f;
    float target_y = 40.0f;

    // 舌头 / 触手 / 黏液弹 当前末端位置
    float tip_x = 120.0f;
    float tip_y = 100.0f;
    float progress = 0.0f;

    // 黏液弹飞行
    float mucus_x = 0.0f;
    float mucus_y = 0.0f;
    float mucus_vx = 0.0f;
    float mucus_vy = 0.0f;
};

class PredatorSystem {
public:
    PredatorSystem();

    void init();
    void update(float dt, PreyBugSystem &bugs, SkeletonSystem &skeleton,
                PhysiologySystem &physiology, MetaballSystem &metaballs);
    void draw(M5Canvas &canvas) const;

    // 触发自主捕食
    bool tryTriggerHunt(PreyBugSystem &bugs, const SkeletonSystem &skeleton);

    bool isHunting() const { return hunt.active; }
    HuntAction getCurrentAction() const { return hunt.action; }

private:
    HuntContext hunt;
    float hunt_decision_cooldown = 1.0f;

    void updateTongueStrike(float dt, PreyBugSystem &bugs, SkeletonSystem &skeleton,
                            PhysiologySystem &physiology, MetaballSystem &metaballs);
    void updateTentacleGrab(float dt, PreyBugSystem &bugs, SkeletonSystem &skeleton,
                            PhysiologySystem &physiology, MetaballSystem &metaballs);
    void updateMucusSnare(float dt, PreyBugSystem &bugs, SkeletonSystem &skeleton,
                           PhysiologySystem &physiology, MetaballSystem &metaballs);

    void finishDigest(PreyBugSystem &bugs, PhysiologySystem &physiology, MetaballSystem &metaballs);

    void drawTongue(M5Canvas &canvas) const;
    void drawGrabTentacle(M5Canvas &canvas) const;
    void drawMucusShot(M5Canvas &canvas) const;
};
