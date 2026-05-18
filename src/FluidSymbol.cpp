#include "FluidSymbol.h"

FluidSymbolManager::FluidSymbolManager() : pointCount(0), lastUpdateTime(0) {}

void FluidSymbolManager::addPoint(float x, float y, float r) {
    if (pointCount < MAX_SYMBOL_POINTS) {
        // 增加 Arrival 风格的随机偏移，使路径不规则
        float ox = (float)random(-30, 31) * 0.1f;
        float oy = (float)random(-30, 31) * 0.1f;
        points[pointCount].x = x + ox;
        points[pointCount].y = y + oy;
        points[pointCount].radius = r * (0.8f + (float)random(40) * 0.01f);
        points[pointCount].life = 1.0f;
        points[pointCount].vy = 0.015f + (float)random(10) * 0.01f; // 极慢的滴落感
        points[pointCount].vx = (float)random(-10, 11) * 0.003f;
        pointCount++;
    }
}

void FluidSymbolManager::update() {
    for (int i = 0; i < pointCount; i++) {
        // 增加微小的正弦漂移，模拟流体在玻璃上的波动
        float drift = sinf(millis() * 0.001f + i) * 0.015f;
        points[i].y += points[i].vy;
        points[i].x += points[i].vx + drift;
        points[i].life -= 0.0012f; // 增加存在时间
        
        if (points[i].life < 0.3f) {
            points[i].radius *= 0.985f; // 末期变细消失
        }
        
        if (points[i].life <= 0) {
            points[i] = points[pointCount - 1];
            pointCount--;
            i--;
        }
    }
}

void FluidSymbolManager::trigger(const String& type) {
    clear();
    currentType = type;
    if (type == "no" || type == "stop") genX();
    else if (type == "yes" || type == "agree") genO();
    else if (type == "help") genEXCLAMATION();
    else if (type == "question") genQUESTION();
    else if (type == "eye") genEYE();
    else if (type == "warning") genWARNING();
    else if (type == "splash") genSPLASH();
    else if (type == "heart") genHEART();
}

void FluidSymbolManager::clear() {
    pointCount = 0;
}

void FluidSymbolManager::genX() {
    // 绘制 X
    for (float i = 0; i < 25; i += 4) {
        addPoint(100 + i, 50 + i);      // \
        addPoint(125 - i, 50 + i);      // /
    }
}

void FluidSymbolManager::genO() {
    // 绘制 O
    for (float a = 0; a < 6.28f; a += 0.4f) {
        addPoint(120 + cosf(a) * 18, 65 + sinf(a) * 18);
    }
}

void FluidSymbolManager::genQUESTION() {
    // 绘制 ?
    for (float a = -1.5f; a < 2.5f; a += 0.4f) {
        addPoint(120 + cosf(a) * 15, 55 + sinf(a) * 12);
    }
    addPoint(120, 75);
    addPoint(120, 88, 5.0f); // 墨点
}

void FluidSymbolManager::genEXCLAMATION() {
    // 绘制 !
    for (float y = 45; y < 75; y += 4) {
        addPoint(120, y);
    }
    addPoint(120, 88, 5.0f); // 墨点
}

void FluidSymbolManager::genEYE() {
    // 类似 Arrival 的圆环，但带有一点“瞳孔”特征
    for (float a = 0; a < 6.28f; a += 0.25f) {
        float r = 25 + sinf(a * 4) * 3; // 不规则圆环
        addPoint(120 + cosf(a)*r, 67 + sinf(a)*r, 3.5f);
    }
    addPoint(120, 67, 6.0f); // 瞳孔
}

void FluidSymbolManager::genHEART() {
    for (float t = 0; t < 6.28f; t += 0.3f) {
        float x = 16 * powf(sinf(t), 3);
        float y = -(13 * cosf(t) - 5 * cosf(2*t) - 2 * cosf(3*t) - cosf(4*t));
        addPoint(120 + x * 2.0f, 65 + y * 2.0f, 3.5f);
    }
}

void FluidSymbolManager::genWARNING() {
    // 放射状警告图形：中心致密，四周尖刺
    float cx = 120, cy = 65;
    // 中心核
    for (float a = 0; a < 6.28f; a += 0.4f) {
        addPoint(cx + cosf(a)*8, cy + sinf(a)*8, 5.0f);
    }
    // 放射尖刺
    for (int i = 0; i < 12; i++) {
        float angle = (float)i / 12.0f * 6.28f;
        float len = 20.0f + random(25);
        for (float d = 10; d < len; d += 5) {
            addPoint(cx + cosf(angle)*d, cy + sinf(angle)*d, 3.5f * (1.0f - d/len));
        }
    }
}

void FluidSymbolManager::genSPLASH() {
    // 溅射：非对称的大面积覆盖
    float cx = 60 + random(100), cy = 40 + random(40);
    // 核心区域 (厚重的液体)
    for (int i = 0; i < 40; i++) {
        float r = (float)random(15, 30);
        float a = (float)random(360) * 0.0174f;
        addPoint(cx + cosf(a)*r*0.6f, cy + sinf(a)*r*0.6f, 6.0f);
    }
    // 边缘流淌和飞溅
    for (int i = 0; i < 15; i++) {
        float a = (float)random(360) * 0.0174f;
        float len = 30 + random(40);
        for (float d = 20; d < len; d += 6) {
            addPoint(cx + cosf(a)*d, cy + sinf(a)*d, 4.0f * (1.0f - d/len));
        }
    }
}

