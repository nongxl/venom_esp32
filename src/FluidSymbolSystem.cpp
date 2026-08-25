#include "FluidSymbolSystem.h"
#include <cmath>

FluidSymbolSystem::FluidSymbolSystem() : pointCount(0), currentType("") {}

void FluidSymbolSystem::init() {
    clear();
}

void FluidSymbolSystem::addPoint(float x, float y, float r) {
    if (pointCount < MAX_SYMBOL_POINTS) {
        // Arrival 风格的随机微偏移，使路径有机不规则
        float ox = (float)(rand() % 61 - 30) * 0.1f;
        float oy = (float)(rand() % 61 - 30) * 0.1f;
        points[pointCount].x = x + ox;
        points[pointCount].y = y + oy;
        points[pointCount].radius = r * (0.8f + (float)(rand() % 40) * 0.01f);
        points[pointCount].life = 1.0f;
        points[pointCount].vy = 0.02f + (float)(rand() % 10) * 0.01f; // 极慢的滴落感
        points[pointCount].vx = (float)(rand() % 21 - 10) * 0.003f;
        pointCount++;
    }
}

void FluidSymbolSystem::update(float dt) {
    unsigned long now = millis();
    for (int i = 0; i < pointCount; i++) {
        // 微小的正弦漂移，模拟流体在玻璃上的波动
        float drift = std::sin(now * 0.001f + i) * 0.015f;
        points[i].y += points[i].vy * (dt * 60.0f);
        points[i].x += (points[i].vx + drift) * (dt * 60.0f);
        points[i].life -= 0.0012f * (dt * 60.0f);

        if (points[i].life < 0.3f) {
            points[i].radius *= 0.985f; // 末期变细消散
        }

        if (points[i].life <= 0.0f) {
            points[i] = points[pointCount - 1];
            pointCount--;
            i--;
        }
    }

    if (pointCount == 0) {
        currentType = "";
    }
}

void FluidSymbolSystem::trigger(const String &type) {
    clear();
    currentType = type;
    if (type == "no" || type == "stop" || type == "x") genX();
    else if (type == "yes" || type == "agree" || type == "o") genO();
    else if (type == "help" || type == "!" || type == "exclamation") genEXCLAMATION();
    else if (type == "question" || type == "?") genQUESTION();
    else if (type == "eye" || type == "watch") genEYE();
    else if (type == "warning") genWARNING();
    else if (type == "splash" || type == "doodle") genSPLASH();
    else if (type == "heart") genHEART();
    else genQUESTION();
}

void FluidSymbolSystem::clear() {
    pointCount = 0;
    currentType = "";
}

void FluidSymbolSystem::genX() {
    for (float i = 0; i < 25; i += 3.5f) {
        addPoint(105 + i, 52 + i, 4.2f); // \
        addPoint(130 - i, 52 + i, 4.2f); // /
    }
}

void FluidSymbolSystem::genO() {
    for (float a = 0; a < 6.28f; a += 0.35f) {
        addPoint(120 + std::cos(a) * 18.0f, 65 + std::sin(a) * 18.0f, 4.0f);
    }
}

void FluidSymbolSystem::genQUESTION() {
    for (float a = -1.5f; a < 2.5f; a += 0.38f) {
        addPoint(120 + std::cos(a) * 15.0f, 55 + std::sin(a) * 12.0f, 4.2f);
    }
    addPoint(120, 75, 4.0f);
    addPoint(120, 88, 5.5f); // 墨点
}

void FluidSymbolSystem::genEXCLAMATION() {
    for (float y = 45; y < 75; y += 3.8f) {
        addPoint(120, y, 4.2f);
    }
    addPoint(120, 88, 5.5f); // 墨点
}

void FluidSymbolSystem::genEYE() {
    // 类似 Arrival 的圆环，带有一点瞳孔特征
    for (float a = 0; a < 6.28f; a += 0.22f) {
        float r = 24.0f + std::sin(a * 4.0f) * 3.5f;
        addPoint(120 + std::cos(a) * r, 67 + std::sin(a) * r, 3.8f);
    }
    addPoint(120, 67, 6.5f); // 瞳孔
}

void FluidSymbolSystem::genHEART() {
    for (float t = 0; t < 6.28f; t += 0.25f) {
        float s_t = std::sin(t);
        float x = 15.0f * s_t * s_t * s_t;
        float y = -(12.0f * std::cos(t) - 4.5f * std::cos(2.0f * t) - 1.8f * std::cos(3.0f * t) - std::cos(4.0f * t));
        addPoint(120 + x * 1.8f, 65 + y * 1.8f, 3.8f);
    }
}

void FluidSymbolSystem::genWARNING() {
    float cx = 120, cy = 65;
    for (float a = 0; a < 6.28f; a += 0.4f) {
        addPoint(cx + std::cos(a) * 8.0f, cy + std::sin(a) * 8.0f, 5.0f);
    }
    for (int i = 0; i < 12; i++) {
        float angle = (float)i / 12.0f * 6.28f;
        float len = 20.0f + (rand() % 25);
        for (float d = 10; d < len; d += 4.5f) {
            addPoint(cx + std::cos(angle) * d, cy + std::sin(angle) * d, 3.5f * (1.0f - d / len) + 1.2f);
        }
    }
}

void FluidSymbolSystem::genSPLASH() {
    float cx = 70 + (rand() % 100), cy = 45 + (rand() % 40);
    for (int i = 0; i < 35; i++) {
        float r = (float)(12 + (rand() % 18));
        float a = (float)(rand() % 360) * 0.017453f;
        addPoint(cx + std::cos(a) * r * 0.6f, cy + std::sin(a) * r * 0.6f, 5.5f);
    }
    for (int i = 0; i < 14; i++) {
        float a = (float)(rand() % 360) * 0.017453f;
        float len = 25 + (rand() % 35);
        for (float d = 15; d < len; d += 5.0f) {
            addPoint(cx + std::cos(a) * d, cy + std::sin(a) * d, 4.0f * (1.0f - d / len) + 1.5f);
        }
    }
}

void FluidSymbolSystem::wipePoints(float screenX, float screenY, float radius) {
    float r2 = radius * radius;
    for (int i = 0; i < pointCount; i++) {
        float dx = points[i].x - screenX;
        float dy = points[i].y - screenY;
        if (dx * dx + dy * dy < r2) {
            points[i] = points[pointCount - 1];
            pointCount--;
            i--;
        }
    }
    if (pointCount == 0) {
        currentType = "";
    }
}
