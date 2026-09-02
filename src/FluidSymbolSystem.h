#pragma once
#include <Arduino.h>
#include "config.h"

struct SymbolPoint {
    float x;
    float y;
    float start_x;       // 喷射源点（毒液身体表皮）
    float start_y;
    float target_x;      // 目标符号坐标
    float target_y;
    float target_radius; // 展开后的完整半径
    float radius;        // 当前动态半径
    float progress;      // 0.0 -> 1.0 喷出升腾展开进度
    float delay;         // 细微喷射队列时序差
    float life;          // 1.0 -> 0.0 消散生命周期
    float vy;            // 极慢向上漂浮速度
    float vx;            // 侧向微飘移速度
};

class FluidSymbolSystem {
public:
    static constexpr int MAX_SYMBOL_POINTS = 120;

    FluidSymbolSystem();

    void init();
    void update(float dt);
    void trigger(const String &type, float center_x = 120.0f, float center_y = 65.0f, float origin_x = -999.0f, float origin_y = -999.0f);
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
    float cur_origin_x = 120.0f;
    float cur_origin_y = 90.0f;

    void addPoint(float x, float y, float r = 4.0f);
    void addPointWithVel(float x, float y, float r, float vx, float vy);

    // 经典流体墨迹符号生成器 (基于指定开阔中心点生成)
    void genX(float cx, float cy);
    void genO(float cx, float cy);
    void genQUESTION(float cx, float cy);
    void genEXCLAMATION(float cx, float cy);
    void genEYE(float cx, float cy);
    void genHEART(float cx, float cy);
    void genWARNING(float cx, float cy);
    void genSPLASH(float cx, float cy);
    void genZz(float cx, float cy);
    void genDIZZY(float cx, float cy);
    void genBITE(float cx, float cy);
    void genMUSIC_NOTE(float cx, float cy);   // 单八分音符 ♪
    void genMUSIC_DOUBLE(float cx, float cy); // 连体双八分音符 ♫
};
