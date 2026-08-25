#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"

struct Tentacle {
    bool active = false;
    int attach_node_idx = 2;
    float start_x = 0.0f, start_y = 0.0f;
    float ctrl_x = 0.0f,  ctrl_y = 0.0f;
    float end_x = 0.0f,   end_y = 0.0f;
    float target_x = 0.0f, target_y = 0.0f;

    float length_progress = 0.0f;
    float max_length = 40.0f;
    float wave_phase = 0.0f;
    float life_timer = 0.0f;
    float duration = 3.0f;
    float base_thickness = 4.0f;
    bool is_clinging = false; // 是否强力抓紧边缘
};

class TentacleRenderer {
public:
    TentacleRenderer();

    void init();
    void update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, bool is_upside_down);
    void draw(M5Canvas &canvas) const;

    void spawnTentacle(const SkeletonSystem &skeleton, bool cling_edge = false);

private:
    Tentacle tentacles[MAX_TENTACLES];
    float auto_spawn_timer = 0.0f;

    void updateTentacle(int idx, float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology);
};
