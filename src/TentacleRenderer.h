#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"

// 爬行抓取与悬挂触手阶段
enum GrappleStage {
    GRAPPLE_INACTIVE = 0,
    GRAPPLE_SHOOT,      // 触手高速射出 (0.2s)
    GRAPPLE_ANCHOR,     // 掌心爪盘拍击目的地并抓牢展开 (0.15s)
    GRAPPLE_PULL,       // 强力收缩触手拉动身体质心 (0.5s)
    GRAPPLE_HOLD,       // 头部抵达后手部继续死死吸附 1.0~2.5s 克服重力挂住！
    GRAPPLE_FUSE,       // 融回重吸收 (0.2s)
    GRAPPLE_SWING       // 高空悬挂荡秋千阶段 (持续单摆摇荡)
};

struct GrappleTendril {
    bool active = false;
    GrappleStage stage = GRAPPLE_INACTIVE;
    float timer = 0.0f;
    float hold_duration = 1.2f;

    float start_x = 120.0f;
    float start_y = 100.0f;
    float target_x = 120.0f;
    float target_y = 100.0f;

    float hand_x = 120.0f;
    float hand_y = 100.0f;

    float shoot_progress = 0.0f;
    float pull_progress = 0.0f;
    float palm_spread = 0.0f; // 掌心爪盘张开度
    float ctrl_offset_x = 0.0f;
    float ctrl_offset_y = 0.0f;
    float rope_length = SWING_ROPE_LENGTH;
};

struct Tentacle {
    bool active = false;
    int attach_node_idx = 1;
    float start_x = 0.0f;
    float start_y = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float ctrl_x = 0.0f;
    float ctrl_y = 0.0f;
    float end_x = 0.0f;
    float end_y = 0.0f;
    float length_progress = 0.0f;
    float max_length = 40.0f;
    float duration = 2.5f;
    float life_timer = 0.0f;
    float wave_phase = 0.0f;
    float base_thickness = 3.5f;
    bool is_clinging = false;
};

struct MicroPodia {
    int node_idx = 0;        // 依附骨架节点 (0~4)
    float offset_x = 0.0f;   // 沿节点横向偏移
    float offset_y = 0.0f;   // 沿节点纵向偏移
    float phase = 0.0f;      // 异相划动波相位
    float leg_len = 5.5f;    // 小触手伸展长度 (4.5~6.5px)
    float base_x = 0.0f;     // 根部世界坐标
    float base_y = 0.0f;
    float cur_tip_x = 0.0f;  // 尖端世界坐标
    float cur_tip_y = 0.0f;
    bool is_planted = false; // 是否踩在地面/墙面
};

class TentacleRenderer {
public:
    static constexpr int MAX_MICRO_PODIA = 8; // 8 根沿接触腹面生长的微小活体小触手

    TentacleRenderer();

    void init();
    void update(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology, bool is_upside_down);
    void draw(M5Canvas &canvas) const;

    // 启动触手射出爬行抓取
    void startGrappleCrawl(float from_x, float from_y, float to_x, float to_y);

    // 启动天花板高空悬挂荡秋千
    void startCeilingSwing(float from_x, float from_y, float anchor_x, float anchor_y, float rope_length = SWING_ROPE_LENGTH);
    void endCeilingSwing();

    bool isGrappling() const { return grapple.active; }
    GrappleStage getGrappleStage() const { return grapple.stage; }

    void reset() {
        grapple.active = false;
        grapple.stage = GRAPPLE_INACTIVE;
        for (int i = 0; i < MAX_TENTACLES; ++i) {
            tentacles[i].active = false;
        }
    }

private:
    Tentacle tentacles[MAX_TENTACLES];
    GrappleTendril grapple;
    float auto_spawn_timer = 0.0f;

    // 皮肤表面小触手群系统 (Micro-Tendril Podia Peristalsis)
    MicroPodia podia[MAX_MICRO_PODIA];
    float podia_wave_phase = 0.0f;
    float creep_dir = 1.0f; // 蠕动方向 (+1.0 向右 / -1.0 向左)
    float creep_switch_timer = 0.0f;

    void initMicroPodia();
    void updateMicroPodia(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology);
    void drawMicroPodia(M5Canvas &canvas) const;

    void updateTentacle(int idx, float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology);
    void updateGrappleCrawl(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology);
    void spawnTentacle(const SkeletonSystem &skeleton, bool cling_edge);

    void drawGrappleTendril(M5Canvas &canvas) const;
};
