#pragma once
#include <Arduino.h>
#include "config.h"

struct SkeletonNode {
    float x = 120.0f;
    float y = 100.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float base_radius = 22.0f;
    float radius_x = 22.0f;
    float radius_y = 22.0f;
    float mass = 1.0f;

    // 贴壁接触形变程度 (0.0~1.0)
    float contact_bottom = 0.0f;
    float contact_top = 0.0f;
    float contact_left = 0.0f;
    float contact_right = 0.0f;

    // 局部神经鼓包偏移
    float bleb_offset_x = 0.0f;
    float bleb_offset_y = 0.0f;
};

class SkeletonSystem {
public:
    SkeletonSystem();

    void init();
    void update(float dt, float gravity_x, float gravity_y,
                float crawl_force_x, float crawl_force_y,
                float neuro_tension, float spike_intensity,
                float respiration, bool is_upside_down,
                const float *spectrum_bands = nullptr);

    void applyImpulse(float ix, float iy);
    void triggerSlingThrow(float dir_x, float dir_y, float speed);
    void triggerLocalBleb(int node_index, float intensity = 1.0f);

    // 主动抓取触手牵引接口
    void setPullTarget(float tx, float ty, float force);
    void clearPullTarget();

    // 蛛丝悬挂荡秋千接口
    void setHangingAnchor(float ax, float ay, float rope_length = SWING_ROPE_LENGTH);
    void clearHangingAnchor();
    bool isHanging() const { return is_hanging; }
    void getHangingAnchor(float &ax, float &ay) const { ax = anchor_x; ay = anchor_y; }
    float getHangingRopeLength() const { return rope_len; }

    void setRollingMode(bool active) { is_rolling = active; }
    bool isRolling() const { return is_rolling; }

    void setBouncingMode(bool active) { is_bouncing_ball = active; }
    bool isBouncingMode() const { return is_bouncing_ball; }
    void setBounceDeform(float squash_y, float stretch_x) {
        bounce_squash_y = squash_y;
        bounce_stretch_x = stretch_x;
    }

    void setBatHangMode(bool active) { is_bat_hang = active; }
    bool isBatHang() const { return is_bat_hang; }

    void setWakeStretchMode(bool active) { is_wake_stretching = active; }
    bool isWakeStretching() const { return is_wake_stretching; }

    void setSlouchLevel(float slouch) { slouch_level = slouch; }
    float getSlouchLevel() const { return slouch_level; }

    void setSleepMode(bool active) { is_sleeping_mode = active; }
    bool isSleepMode() const { return is_sleeping_mode; }

    void clearVelocities() {
        for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
            nodes[i].vx = 0.0f;
            nodes[i].vy = 0.0f;
        }
    }

    const SkeletonNode& getNode(int idx) const { return nodes[idx]; }
    int getNodeCount() const { return SKELETON_NODE_COUNT; }

    void getHeadPos(float &hx, float &hy) const { hx = nodes[0].x; hy = nodes[0].y; }
    void getCenterPos(float &cx, float &cy) const { cx = nodes[2].x; cy = nodes[2].y; }
    void getTailPos(float &tx, float &ty) const { tx = nodes[4].x; ty = nodes[4].y; }

    // 全节点表皮小触手蠕动动力学接口 (彻底克服弹簧阻尼整体前移)
    void setCreepingTarget(float tx, float ty, float speed = 1.0f);
    void clearCreepingTarget();
    bool isCreepingMotion() const { return is_creeping_motion; }

    // 防倒退掉头与表皮小触手蠕动动力学
    void alignHeadingToTarget(float target_x, float target_y);
    void applyCreepingMotion(float dir_x, float dir_y, float speed, float dt);
    float getHeadingAngle() const;

    bool isAttachedToWall() const;
    bool isStickyToyAdhered() const { return sticky_clog_timer > 0.0f; }
    bool isFlying() const { return flying_timer > 0.0f; }

    // 撞击事件查询与消费（供外部触发马达触觉与液滴飞溅）
    bool checkAndConsumeImpactEvent(float &impact_speed, float &impact_x, float &impact_y) {
        if (impact_occurred) {
            impact_occurred = false;
            impact_speed = last_impact_speed;
            impact_x = impact_hit_x;
            impact_y = impact_hit_y;
            return true;
        }
        return false;
    }

private:
    SkeletonNode nodes[SKELETON_NODE_COUNT];
    float rest_lengths[SKELETON_NODE_COUNT - 1];

    // 主动牵引力目标
    bool has_pull_target = false;
    float pull_target_x = 120.0f;
    float pull_target_y = 100.0f;
    float pull_strength = 0.0f;
    float pull_timeout_timer = 0.0f;

    // 全骨架全节点表皮小触手持续蠕动推力系统
    bool is_creeping_motion = false;
    float creep_target_x = 120.0f;
    float creep_target_y = 100.0f;
    float creep_speed_mult = 1.0f;

    // 蛛丝高空悬挂荡秋千参数
    bool is_hanging = false;
    float anchor_x = 120.0f;
    float anchor_y = 8.0f;
    float rope_len = SWING_ROPE_LENGTH;
    float swing_phase = 0.0f;
    float swing_angle = 0.0f;
    float swing_ang_vel = 0.0f;
    bool is_rolling = false;
    bool is_bouncing_ball = false;
    float bounce_squash_y = 1.0f;
    float bounce_stretch_x = 1.0f;
    float roll_angle = 0.0f;
    bool is_bat_hang = false;

    // 高速飞行抛体与黏性吸附状态
    float flying_timer = 0.0f;
    float sticky_clog_timer = 0.0f;
    float flip_cooldown = 0.0f;             // 原地调头翻身冷却，杜绝每帧高频震荡
    float creep_locomotion_phase = 0.0f;    // 尺蠖蠕动波浪相位
    bool is_wake_stretching = false;        // 睡醒伸懒腰拉伸
    bool is_sleeping_mode = false;          // 深度睡眠静止模式
    float slouch_level = 0.0f;              // 疲惫/无聊松弛度 [0.0, 1.0]
    bool impact_occurred = false;
    float last_impact_speed = 0.0f;
    float impact_hit_x = 120.0f;
    float impact_hit_y = 100.0f;

    void applyWallAdhesion(int i);
    void updateNodePhysics(int i, float dt, float gx, float gy, float cfx, float cfy, float tension, bool is_upside_down);
    void solveSpringConstraints(float tension);
    void solveHangingConstraint(float dt, float gravity_x, float gravity_y);
};
