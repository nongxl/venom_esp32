#pragma once
#include <Arduino.h>
#include "config.h"

// 骨架质点定义
struct SkeletonNode {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    
    float base_radius = 20.0f;
    float radius_x = 20.0f; // 动态水平半轴（受挤压变形）
    float radius_y = 20.0f; // 动态垂直半轴（受挤压变形）
    
    // 微行为与神经局部鼓包
    float bleb_offset_x = 0.0f;
    float bleb_offset_y = 0.0f;
    float spike_amount = 0.0f; // 尖刺扩张强度

    // 贴墙接触程度 [0.0, 1.0]
    float contact_left   = 0.0f;
    float contact_right  = 0.0f;
    float contact_top    = 0.0f;
    float contact_bottom = 0.0f;

    bool is_head = false;
    bool is_tail = false;
};

class SkeletonSystem {
public:
    SkeletonSystem();
    
    void init();
    void reset(float cx, float cy);
    void update(float dt, float gravity_x, float gravity_y, float crawl_bias_x, float crawl_bias_y,
                float tension, float spike_intensity, float respiration, bool is_upside_down);
    
    // 获取骨架节点
    const SkeletonNode& getNode(int index) const { return nodes[index]; }
    SkeletonNode& getNodeRef(int index) { return nodes[index]; }
    int getNodeCount() const { return SKELETON_NODE_COUNT; }

    // 获取头部与身体中心位置
    void getHeadPos(float &hx, float &hy) const { hx = nodes[0].x; hy = nodes[0].y; }
    void getCenterPos(float &cx, float &cy) const;

    // 施加外力冲量（如惊吓、摇晃）
    void applyImpulse(float fx, float fy);

    // 触发微行为：局部鼓包抽搐
    void triggerLocalBleb(int node_idx, float intensity);

    // 检查整体贴壁状态
    bool isTouchingWall() const { return total_wall_contact > 0.3f; }
    float getBottomContact() const { return nodes[0].contact_bottom * 0.5f + nodes[1].contact_bottom * 0.5f; }
    float getTopContact() const { return nodes[0].contact_top * 0.5f + nodes[1].contact_top * 0.5f; }

private:
    SkeletonNode nodes[SKELETON_NODE_COUNT];
    float rest_lengths[SKELETON_NODE_COUNT - 1];
    float total_wall_contact = 0.0f;

    void applySpringForces(float tension);
    void applyBoundaryAndAdhesion(float gravity_x, float gravity_y, bool is_upside_down);
    void updateDeformations(float respiration, float tension, float spike_intensity, float dt);
};
