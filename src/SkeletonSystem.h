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
                float respiration, bool is_upside_down);

    void applyImpulse(float ix, float iy);
    void triggerLocalBleb(int node_index, float intensity = 1.0f);

    // 主动抓取触手牵引接口 (质心前移拉向目的地)
    void setPullTarget(float tx, float ty, float force);
    void clearPullTarget();

    const SkeletonNode& getNode(int idx) const { return nodes[idx]; }
    int getNodeCount() const { return SKELETON_NODE_COUNT; }

    void getHeadPos(float &hx, float &hy) const { hx = nodes[0].x; hy = nodes[0].y; }
    void getCenterPos(float &cx, float &cy) const { cx = nodes[2].x; cy = nodes[2].y; }
    void getTailPos(float &tx, float &ty) const { tx = nodes[4].x; ty = nodes[4].y; }

    bool isAttachedToWall() const;

private:
    SkeletonNode nodes[SKELETON_NODE_COUNT];
    float rest_lengths[SKELETON_NODE_COUNT - 1];

    // 主动牵引力目标
    bool has_pull_target = false;
    float pull_target_x = 120.0f;
    float pull_target_y = 100.0f;
    float pull_strength = 0.0f;

    void updateNodePhysics(int i, float dt, float gx, float gy, float cfx, float cfy, float tension, bool is_upside_down);
    void solveSpringConstraints(float tension);
    void applyWallAdhesion(int i);
};
