#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"

// 爬行抓取触手阶段
enum GrappleStage {
    GRAPPLE_INACTIVE = 0,
    GRAPPLE_SHOOT,      // 触手高速射出 (0.2s)
    GRAPPLE_ANCHOR,     // 掌心爪盘拍击目的地并抓牢展开 (0.15s)
    GRAPPLE_PULL,       // 强力收缩触手拉动身体质心 (0.5s)
    GRAPPLE_FUSE        // 头部抵达掌心，融回重吸收 (0.2s)
};

struct GrappleTendril {
    bool active = false;
    GrappleStage stage = GRAPPLE_INACTIVE;
    float timer = 0.0f;

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

class TentacleRenderer {
public:
    TentacleRenderer();

    void init();
    void update(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology, bool is_upside_down);
    void draw(M5Canvas &canvas) const;

    // 启动触手射出爬行抓取
    void startGrappleCrawl(float from_x, float from_y, float to_x, float to_y);
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

    void updateTentacle(int idx, float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology);
    void updateGrappleCrawl(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology);
    void spawnTentacle(const SkeletonSystem &skeleton, bool cling_edge);

    void drawGrappleTendril(M5Canvas &canvas) const;
};
