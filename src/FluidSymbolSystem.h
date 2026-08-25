#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"

struct FluidInkParticle {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float radius = 2.4f;
    float life = 0.0f;
    float max_life = 5.0f;
    bool active = false;
    bool is_splatter = false; // 是否为侧向飞溅小墨渣
    bool is_drip = false;     // 是否在缓慢向下滴落拉丝
};

class FluidSymbolSystem {
public:
    FluidSymbolSystem();

    void init();
    void update(float dt, const SkeletonSystem &skeleton, float gravity_x, float gravity_y);
    void draw(M5Canvas &canvas) const;

    void spawnSymbol(FluidSymbolType type, float origin_x, float origin_y);

    bool hasActiveSymbol() const { return active_particle_count > 0; }
    FluidSymbolType getCurrentSymbolType() const { return current_type; }

private:
    FluidInkParticle particles[MAX_INK_PARTICLES];
    int active_particle_count = 0;
    FluidSymbolType current_type = SYMBOL_NONE;

    void addParticle(float x, float y, float vx, float vy, float r, float duration, bool splatter, bool drip);
    void addStrokeLine(float x1, float y1, float x2, float y2, float r1, float r2, int steps);
    void addSplatterBurst(float cx, float cy, int count, float max_spread);

    void buildRingGlyph(float cx, float cy);        // 七肢桶泼墨圆环 (Arrival Splatter Glyph)
    void buildQuestionGlyph(float cx, float cy);    // 泼墨问号 "?"
    void buildExclamationGlyph(float cx, float cy); // 撕裂叹号 "!"
    void buildCrossGlyph(float cx, float cy);       // 泼墨叉号 "X"
    void buildRippleGlyph(float cx, float cy);      // 同心涟漪
};
