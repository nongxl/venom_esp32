#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"
#include "FluidSymbolSystem.h"

constexpr int MAX_SPIKE_ERUPTIONS = 24;

struct Droplet {
    bool active = false;
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float radius = 3.0f;
    float life = 1.0f;
    bool is_jolt_spurt = false;
};

// 物理突发尖刺 (突然刺出 -> 软化变圆融回)
struct SpikeEruption {
    bool active = false;
    int node_idx = 0;
    float angle = 0.0f;
    float max_len = 8.0f;
    float age = 0.0f;
    float duration = 0.35f;
    float attack_time = 0.06f;
};

class MetaballSystem {
public:
    static constexpr uint8_t THRESHOLD = 52;

    MetaballSystem();

    void init();
    void update(float dt, const SkeletonSystem &skeleton, float gravity_x, float gravity_y, const PhysiologySystem &physiology, bool is_sleeping = false);

    void computeField(const SkeletonSystem &skeleton, const PhysiologySystem &physiology,
                      const FluidSymbolSystem &fluid_symbols, float gx, float gy,
                      float ball_x = -1.0f, float ball_y = -1.0f, float ball_r = 0.0f);

    const uint8_t* getFieldBuffer() const { return field_buffer; }

    uint8_t getFieldValue(int gx, int gy) const {
        if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) return 0;
        return field_buffer[gy * GRID_W + gx];
    }

    void spawnDroplet(float x, float y, float vx, float vy, float r = 3.0f, bool is_jolt = false);
    void triggerJoltSpurt(const SkeletonSystem &skeleton, float intensity = 1.0f);
    void triggerSpikeBurst(int count, float max_len_boost = 1.0f);

private:
    uint8_t field_buffer[GRID_W * GRID_H];
    Droplet droplets[MAX_DROPLETS];
    SpikeEruption spikes[MAX_SPIKE_ERUPTIONS];

    float auto_droplet_timer = 0.0f;
    float spike_spawn_timer = 0.0f;

    void updateDroplets(float dt, const SkeletonSystem &skeleton, float gx, float gy, const PhysiologySystem &physiology, bool is_sleeping);
    void updateSpikes(float dt, const PhysiologySystem &physiology, bool is_sleeping);
    void spawnRandomSpike(const PhysiologySystem &physiology);

    void addMetaballToField(float cx, float cy, float rx, float ry, uint8_t intensity,
                           float contact_b, float contact_t, float contact_l, float contact_r);
};
