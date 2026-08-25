#pragma once
#include <Arduino.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"

struct Droplet {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float radius = 3.5f;
    bool active = false;
    float life = 0.0f;
    bool is_jolt_spurt = false;
};

class MetaballSystem {
public:
    MetaballSystem();

    void init();
    void update(float dt, const SkeletonSystem &skeleton, float gravity_x, float gravity_y, const PhysiologySystem &physiology);
    void computeField(const SkeletonSystem &skeleton, const PhysiologySystem &physiology);

    void spawnDroplet(float x, float y, float vx, float vy, float r, bool is_jolt = false);
    void triggerJoltSpurt(const SkeletonSystem &skeleton, float intensity);

    const uint8_t* getFieldBuffer() const { return field_buffer; }
    uint8_t getFieldValue(int gx, int gy) const {
        if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) return 0;
        return field_buffer[gy * GRID_W + gx];
    }

    static constexpr uint8_t THRESHOLD = 100;

    const Droplet* getDroplets() const { return droplets; }
    int getMaxDroplets() const { return MAX_DROPLETS; }

private:
    uint8_t field_buffer[GRID_W * GRID_H];
    Droplet droplets[MAX_DROPLETS];
    float auto_droplet_timer = 0.0f;
    float spike_time_phase = 0.0f;

    void updateDroplets(float dt, const SkeletonSystem &skeleton, float gx, float gy, const PhysiologySystem &physiology);
    void addMetaballToField(float cx, float cy, float rx, float ry, uint8_t intensity,
                            float contact_b, float contact_t, float contact_l, float contact_r,
                            float micro_spike_amp, float spike_phase);
};
