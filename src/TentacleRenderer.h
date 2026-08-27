#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"

enum GrappleStage {
    GRAPPLE_INACTIVE = 0,
    GRAPPLE_SHOOT,       // 触手高速射出 (0.2s)
    GRAPPLE_ANCHOR,      // 掌心爪盘拍击目的地并抓牢展开 (0.15s)
    GRAPPLE_PULL,        // 强力收缩触手拉动身体质心 (0.5s)
    GRAPPLE_HOLD,        // 头部抵达后手部继续死死吸附 1.0~2.5s 克服重力挂住！
    GRAPPLE_FUSE,        // 融回重吸收 (0.2s)
    GRAPPLE_SWING_SHOOT, // 荡秋千前奏1：向上高速射出触手吸附天花板 (0.22s)
    GRAPPLE_SWING_HOIST, // 荡秋千前奏2：强力收紧触手将身体平滑向上拉升至秋千高度 (0.45s)
    GRAPPLE_SWING        // 高空悬挂荡秋千阶段 (持续单摆摇荡)
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

struct MicroTentacle {
    int node_idx = 0;       // 依附的骨架节点 (0..4)
    float side_sign = 1.0f; // +1.0 (下腹/右侧), -1.0 (下腹/左侧)
    float offset_angle = 0.0f;
    float current_len = 0.0f;
    float max_len = 7.5f;
    float phase_offset = 0.0f;
    float tip_x = 0.0f;
    float tip_y = 0.0f;
};

class TentacleRenderer {
public:
    static constexpr int MAX_MICRO_TENTACLES = 6; // 精简为 3 对流体黑色肉足，彻底消除密恐

    TentacleRenderer();

    void init();
    void update(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology, bool is_upside_down, bool is_sleeping = false);

    void clearAllTentacles() {
        for (int i = 0; i < MAX_TENTACLES; ++i) tentacles[i].active = false;
        for (int i = 0; i < MAX_MICRO_TENTACLES; ++i) micro_tentacles[i].current_len = 0.0f;
        grapple.active = false;
        is_creeping = false;
    }
    void draw(M5Canvas &canvas, const SkeletonSystem &skeleton) const;

    // 蠕动爬行模式控制
    void setCreepMode(bool active, float speed_factor = 1.0f);
    bool isCreepActive() const { return is_creeping; }

    // 启动触手射出爬行抓取
    void startGrappleCrawl(float from_x, float from_y, float to_x, float to_y);

    // 启动天花板高空悬挂荡秋千
    void startCeilingSwing(float from_x, float from_y, float anchor_x, float anchor_y, float rope_length = SWING_ROPE_LENGTH);
    void endCeilingSwing();

    // 凌空抽射/击球专用爆发触手
    void triggerVolleyTentacle(float from_x, float from_y, float target_x, float target_y);

    bool isGrappling() const { return grapple.active; }
    GrappleStage getGrappleStage() const { return grapple.stage; }

    void reset() {
        grapple.active = false;
        grapple.stage = GRAPPLE_INACTIVE;
        is_creeping = false;
        for (int i = 0; i < MAX_TENTACLES; ++i) {
            tentacles[i].active = false;
        }
    }

private:
    Tentacle tentacles[MAX_TENTACLES];
    MicroTentacle micro_tentacles[MAX_MICRO_TENTACLES];
    GrappleTendril grapple;
    float auto_spawn_timer = 0.0f;
    bool is_creeping = false;
    float creep_speed = 1.0f;
    float creep_wave_phase = 0.0f;

    void initMicroTentacles();
    void updateTentacle(int idx, float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology);
    void updateMicroTentacles(float dt, const SkeletonSystem &skeleton);
    void updateGrappleCrawl(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology);
    void spawnTentacle(const SkeletonSystem &skeleton, bool cling_edge);

    void drawGrappleTendril(M5Canvas &canvas) const;
    void drawMicroTentacles(M5Canvas &canvas, const SkeletonSystem &skeleton) const;
};
