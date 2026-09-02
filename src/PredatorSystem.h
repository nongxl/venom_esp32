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
    PHASE_SHOOT,          // 弹射长舌 / 射出触手 / 极速飞射狙击黏液弹
    PHASE_HOLD,           // 舌尖吸中/触手抓牢后的张力停顿阶段 (0.22~0.30s)
    PHASE_RETRACT,        // 卷回长舌 / 拖回触手
    PHASE_STALK_OBSERVE,  // 【黏液狙击命中后的原地戏谑观察阶段 1.2~1.8s】
    PHASE_CRAWL_ENGULF,   // 观察满足后大步爬过去包覆吞噬
    PHASE_DIGEST          // 吞噬消化反馈
};

struct SplatParticle {
    bool active = false;
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float radius = 1.8f;
    float life = 1.0f;
    bool is_saliva = false; // 晶莹口水飞溅粒子
    uint16_t color = 0xFFFF;
};

struct HuntContext {
    bool active = false;
    HuntAction action = HUNT_NONE;
    HuntPhase phase = PHASE_IDLE;
    int target_bug_idx = -1;

    float timer = 0.0f;
    float observe_duration = 1.5f;
    float start_x = 120.0f;
    float start_y = 100.0f;
    float target_x = 120.0f;
    float target_y = 40.0f;

    // 舌头 / 触手 / 黏液弹 当前末端位置
    float tip_x = 120.0f;
    float tip_y = 100.0f;
    float progress = 0.0f;

    // 纯黑黏液弹飞行与拖尾
    float mucus_x = 0.0f;
    float mucus_y = 0.0f;
    float mucus_vx = 0.0f;
    float mucus_vy = 0.0f;
    float trail_spawn_timer = 0.0f;
    float saliva_spray_timer = 0.0f;
    bool is_reserve_snare = false; // 饱腹时仅喷网定身作为储备粮，不立刻吃
};

class PredatorSystem {
public:
    static constexpr int MAX_SPLAT_PARTICLES = 36;

    PredatorSystem();

    void init();
    void update(float dt, PreyBugSystem &bugs, SkeletonSystem &skeleton,
                PhysiologySystem &physiology, MetaballSystem &metaballs,
                bool is_sleeping = false);
    void draw(M5Canvas &canvas) const;

    // 触发自主捕食 (接收生理饥饿度与储备粮判定)
    bool tryTriggerHunt(PreyBugSystem &bugs, const SkeletonSystem &skeleton, const PhysiologySystem &physiology);
    void cancelHunt(SkeletonSystem *skeleton = nullptr, PreyBugSystem *bugs = nullptr);

    bool isHunting() const { return hunt.active; }
    HuntAction getCurrentAction() const { return hunt.action; }
    HuntPhase getCurrentPhase() const { return hunt.phase; }
    void getTargetPos(float &tx, float &ty) const { tx = hunt.target_x; ty = hunt.target_y; }

private:
    HuntContext hunt;
    SplatParticle splats[MAX_SPLAT_PARTICLES];
    float hunt_decision_cooldown = 1.0f;
    HuntAction last_hunt_action = HUNT_NONE;

    void spawnSplatBurst(float at_x, float at_y, int count, float speed_mult = 1.0f);
    void spawnSalivaSpray(float at_x, float at_y, float base_vx, float base_vy, int count);
    void updateSplatParticles(float dt);

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
