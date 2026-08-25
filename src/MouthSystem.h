#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "BugSystem.h"
#include "TentacleRenderer.h"
#include "PhysiologySystem.h"

enum FeedMode {
    FEED_NONE = 0,
    FEED_TONGUE,        // 模态1: 变色龙闪电卷舌
    FEED_TENTACLE,      // 模态2: 触手抓捕塞入口中
    FEED_SLIME_STALK    // 模态3: 喷射黏液黏住爬近吞食
};

enum FeedStage {
    FEED_STAGE_IDLE = 0,
    FEED_STAGE_OPEN_MOUTH,
    FEED_STAGE_TONGUE_EXTEND,
    FEED_STAGE_TONGUE_RETRACT,
    FEED_STAGE_TENTACLE_PULL,
    FEED_STAGE_SLIME_FLY,
    FEED_STAGE_CHEWING
};

struct SlimeProjectile {
    bool active = false;
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float progress = 0.0f;
};

class MouthSystem {
public:
    MouthSystem();

    void init();
    void update(float dt, const SkeletonSystem &skeleton, BugSystem &bugs,
                TentacleRenderer &tentacles, PhysiologySystem &physiology);
    void draw(M5Canvas &canvas, const SkeletonSystem &skeleton) const;

    // 触发捕食动作
    bool startPredation(FeedMode mode, float bug_x, float bug_y);
    bool isHunting() const { return current_mode != FEED_NONE; }
    FeedMode getCurrentMode() const { return current_mode; }

    // 咀嚼震动与音效判定
    bool checkAndConsumeChompVibration();

private:
    FeedMode current_mode = FEED_NONE;
    FeedStage current_stage = FEED_STAGE_IDLE;
    float stage_timer = 0.0f;

    // 嘴巴几何状态
    float mouth_x = 120.0f;
    float mouth_y = 95.0f;
    float mouth_open_ratio = 0.0f; // 0.0 (微闭) ~ 1.0 (大张)
    float mouth_width = 14.0f;
    float mouth_height = 8.0f;
    float mouth_angle = 0.0f;

    // 变色龙长舌头几何
    float tongue_root_x = 120.0f;
    float tongue_root_y = 95.0f;
    float tongue_tip_x = 120.0f;
    float tongue_tip_y = 95.0f;
    float tongue_ctrl_x = 120.0f;
    float tongue_ctrl_y = 95.0f;
    float tongue_progress = 0.0f;
    float target_bug_x = 0.0f;
    float target_bug_y = 0.0f;

    // 黏液飞弹
    SlimeProjectile slime;

    // 咀嚼与尖牙微动画
    int chew_count = 0;
    float chew_phase = 0.0f;
    bool trigger_vibe = false;

    void updateMouthAnchor(const SkeletonSystem &skeleton);
    void drawMouthAndTeeth(M5Canvas &canvas) const;
    void drawChameleonTongue(M5Canvas &canvas) const;
    void drawSlimeProjectile(M5Canvas &canvas) const;
};
