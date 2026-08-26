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

void FluidSymbolSystem::trigger(const String &type, float center_x, float center_y) {
    clear();
    currentType = type;
    if (type == "no" || type == "stop" || type == "x") genX(center_x, center_y);
    else if (type == "yes" || type == "agree" || type == "o") genO(center_x, center_y);
    else if (type == "help" || type == "!" || type == "exclamation") genEXCLAMATION(center_x, center_y);
    else if (type == "question" || type == "?") genQUESTION(center_x, center_y);
    else if (type == "eye" || type == "watch") genEYE(center_x, center_y);
    else if (type == "warning") genWARNING(center_x, center_y);
    else if (type == "splash" || type == "doodle") genSPLASH(center_x, center_y);
    else if (type == "heart" || type == "love") genHEART(center_x, center_y);
    else genQUESTION(center_x, center_y);
}

void FluidSymbolSystem::clear() {
    pointCount = 0;
    currentType = "";
}

void FluidSymbolSystem::genX(float cx, float cy) {
    // 饱满对称工整的双向交叉 X 符号 (绝不再残缺为单斜杠 \)
    for (float i = -12.0f; i <= 12.0f; i += 3.0f) {
        addPoint(cx + i, cy + i, 4.2f); // \ 笔画
        addPoint(cx + i, cy - i, 4.2f); // / 笔画
    }
}

void FluidSymbolSystem::genO(float cx, float cy) {
    // Arrival 风格圆润流畅的七肢桶水墨流体圆环
    for (float a = 0; a < 6.28f; a += 0.28f) {
        float r = 16.0f + std::sin(a * 3.0f) * 1.5f;
        addPoint(cx + std::cos(a) * r, cy + std::sin(a) * r, 4.2f);
    }
}

void FluidSymbolSystem::genQUESTION(float cx, float cy) {
    // 优美的弧形问号与下方墨点
    for (float a = -1.6f; a < 2.3f; a += 0.35f) {
        addPoint(cx + std::cos(a) * 13.0f, cy - 8.0f + std::sin(a) * 10.0f, 4.0f);
    }
    addPoint(cx, cy + 6.0f, 3.8f);
    addPoint(cx, cy + 16.0f, 5.2f); // 下方独立大墨点
}

void FluidSymbolSystem::genEXCLAMATION(float cx, float cy) {
    // 竖直挺拔感叹号与下方独立大墨点
    for (float y = -16.0f; y <= 6.0f; y += 3.5f) {
        addPoint(cx, cy + y, 4.2f);
    }
    addPoint(cx, cy + 16.0f, 5.5f); // 独立墨点
}

void FluidSymbolSystem::genEYE(float cx, float cy) {
    // 七肢桶环形观察之眼 (外圈多重有机涟漪圆环 + 中心凝视瞳孔)
    for (float a = 0; a < 6.28f; a += 0.20f) {
        float r = 18.0f + std::sin(a * 4.0f) * 2.5f;
        addPoint(cx + std::cos(a) * r, cy + std::sin(a) * r, 3.8f);
    }
    addPoint(cx, cy, 6.5f); // 中心大瞳孔
}

void FluidSymbolSystem::genHEART(float cx, float cy) {
    // 七肢桶液态爱心图腾
    for (float t = 0; t < 6.28f; t += 0.22f) {
        float s_t = std::sin(t);
        float x = 12.0f * s_t * s_t * s_t;
        float y = -(10.0f * std::cos(t) - 4.0f * std::cos(2.0f * t) - 1.5f * std::cos(3.0f * t) - std::cos(4.0f * t));
        addPoint(cx + x * 1.35f, cy + y * 1.35f - 2.0f, 3.8f);
    }
}

void FluidSymbolSystem::genWARNING(float cx, float cy) {
    // 放射状警示刺印
    for (float a = 0; a < 6.28f; a += 0.35f) {
        addPoint(cx + std::cos(a) * 7.0f, cy + std::sin(a) * 7.0f, 4.5f);
    }
    for (int i = 0; i < 8; i++) {
        float angle = (float)i / 8.0f * 6.28f;
        for (float d = 8.0f; d < 18.0f; d += 3.5f) {
            addPoint(cx + std::cos(angle) * d, cy + std::sin(angle) * d, 3.2f);
        }
    }
}

void FluidSymbolSystem::genSPLASH(float cx, float cy) {
    // 液态共生体泼墨图腾
    for (int i = 0; i < 20; i++) {
        float r = (float)(6 + (rand() % 14));
        float a = (float)(rand() % 360) * 0.017453f;
        addPoint(cx + std::cos(a) * r, cy + std::sin(a) * r, 4.5f);
    }
    for (int i = 0; i < 8; i++) {
        float a = (float)(rand() % 360) * 0.017453f;
        float len = 14 + (rand() % 18);
        for (float d = 8; d < len; d += 4.0f) {
            addPoint(cx + std::cos(a) * d, cy + std::sin(a) * d, 3.2f);
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
