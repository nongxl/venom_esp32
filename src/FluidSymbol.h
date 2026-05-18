#ifndef FLUID_SYMBOL_H
#define FLUID_SYMBOL_H

#include <Arduino.h>
#include "config.h"

struct SymbolPoint {
    float x, y;
    float radius;
    float life;   // 1.0 (新生成) -> 0.0 (消失)
    float vy;     // 滴落速度 (模拟重力流淌)
    float vx;     // 侧向扩散速度
};

class FluidSymbolManager {
public:
    static constexpr int MAX_SYMBOL_POINTS = 120;
    
    FluidSymbolManager();
    void update();
    void trigger(const String& type);
    void clear();
    
    int getPointCount() const { return pointCount; }
    const SymbolPoint& getPoint(int i) const { return points[i]; }

private:
    SymbolPoint points[MAX_SYMBOL_POINTS];
    int pointCount;
    uint32_t lastUpdateTime;
    String currentType;
    
    void addPoint(float x, float y, float r = 4.0f);
    
    // 符号路径生成器 (基于屏幕坐标 240x135)
    void genX();
    void genO();
    void genQUESTION();
    void genEXCLAMATION();
    void genEYE();
    void genHEART();
    void genWARNING();
    void genSPLASH();
};

#endif
