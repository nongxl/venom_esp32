#pragma once
#include <Arduino.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "PhysiologySystem.h"

struct VoronoiSeed {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    int attach_node = 0;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float activity = 0.0f;
};

class VoronoiSurface {
public:
    VoronoiSurface();

    void init();
    void update(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, float look_x, float look_y, bool is_sleeping = false);

    // 检查网格点 (gx, gy) 是否处于 Voronoi 细胞边界（神经元膜/裂纹）
    // 返回值：0=内部, 1=细胞膜/高光神经纤维, 2=高强度愤怒裂纹
    uint8_t evaluatePoint(int gx, int gy, float cell_dist_diff) const;

    const VoronoiSeed& getSeed(int idx) const { return seeds[idx]; }
    int getSeedCount() const { return VORONOI_SEEDS; }

    // 获取动态细胞边界阈值（受情绪驱动）
    float getMembraneThreshold() const { return current_membrane_threshold; }

private:
    VoronoiSeed seeds[VORONOI_SEEDS];
    float current_membrane_threshold = 1.2f;
    float jitter_energy = 0.0f;

    void updateSeedDynamics(float dt, const SkeletonSystem &skeleton, const PhysiologySystem &physiology, float look_x, float look_y, bool is_sleeping);
};
