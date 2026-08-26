#pragma once
#include <Arduino.h>
#include "config.h"

struct SymbolPoint {
    float x;
    float y;
    float radius;
    float life;   // 1.0 -> 0.0
    float vy;     // 极慢滴落速度
    float vx;     // 侧向扩散速度
};

class FluidSymbolSystem {
public:
    static constexpr int MAX_SYMBOL_POINTS = 120;

    FluidSymbolSystem();

    void init();
    void update(float dt);
    void trigger(const String &type, float center_x = 120.0f, float center_y = 65.0f);
    void clear();
    void wipePoints(float screenX, float screenY, float radius);

    bool hasActiveSymbol() const { return pointCount > 0; }
    String getCurrentType() const { return currentType; }

    int getPointCount() const { return pointCount; }
    const SymbolPoint& getPoint(int i) const { return points[i]; }

private:
    SymbolPoint points[MAX_SYMBOL_POINTS];
    int pointCount = 0;
    String currentType = "";

    void addPoint(float x, float y, float r = 4.0f);

    // 8 大经典流体墨迹符号生成器 (基于指定开阔中心点生成)
    void genX(float cx, float cy);
    void genO(float cx, float cy);
    void genQUESTION(float cx, float cy);
    void genEXCLAMATION(float cx, float cy);
    void genEYE(float cx, float cy);
    void genHEART(float cx, float cy);
    void genWARNING(float cx, float cy);
    void genSPLASH(float cx, float cy);
};
