#include "FluidSymbolSystem.h"
#include <cmath>

FluidSymbolSystem::FluidSymbolSystem() : pointCount(0), currentType("") {}

void FluidSymbolSystem::init() {
    clear();
}

void FluidSymbolSystem::addPoint(float x, float y, float r) {
    if (pointCount < MAX_SYMBOL_POINTS) {
        // 微小抖动与自然向上漂浮速度
        float ox = (float)(rand() % 21 - 10) * 0.05f;
        float oy = (float)(rand() % 21 - 10) * 0.05f;
        points[pointCount].x = x + ox;
        points[pointCount].y = y + oy;
        points[pointCount].radius = r * (0.90f + (float)(rand() % 20) * 0.01f);
        points[pointCount].life = 1.0f;
        points[pointCount].vy = -0.042f - (float)(rand() % 10) * 0.005f; // 自然向上徐徐漂浮
        points[pointCount].vx = (float)(rand() % 11 - 5) * 0.002f;
        pointCount++;
    }
}

void FluidSymbolSystem::addPointWithVel(float x, float y, float r, float vx, float vy) {
    if (pointCount < MAX_SYMBOL_POINTS) {
        float ox = (float)(rand() % 21 - 10) * 0.05f;
        float oy = (float)(rand() % 21 - 10) * 0.05f;
        points[pointCount].x = x + ox;
        points[pointCount].y = y + oy;
        points[pointCount].radius = r * (0.90f + (float)(rand() % 20) * 0.01f);
        points[pointCount].life = 1.0f;
        points[pointCount].vx = vx;
        points[pointCount].vy = vy;
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
    else if (type == "zz" || type == "sleep" || type == "zzz") genZz(center_x, center_y);
    else if (type == "dizzy" || type == "spiral" || type == "coil" || type == "swirl" || type == "晕") genDIZZY(center_x, center_y);
    else if (type == "bite" || type == "teeth" || type == "jaw" || type == "咬" || type == "齿痕") genBITE(center_x, center_y);
    else genQUESTION(center_x, center_y);
}

void FluidSymbolSystem::clear() {
    pointCount = 0;
    currentType = "";
}

void FluidSymbolSystem::genX(float cx, float cy) {
    // 饱满对称工整的双向交叉 X 符号 (绝不再残缺为单斜杠 \)
    for (float i = -12.0f; i <= 12.0f; i += 3.0f) {
        addPoint(cx + i, cy + i, 3.8f); // \ 笔画
        addPoint(cx + i, cy - i, 3.8f); // / 笔画
    }
}

void FluidSymbolSystem::genO(float cx, float cy) {
    // Arrival 风格圆润流畅的七肢桶水墨流体圆环
    for (float a = 0; a < 6.28f; a += 0.28f) {
        float r = 16.0f + std::sin(a * 3.0f) * 1.5f;
        addPoint(cx + std::cos(a) * r, cy + std::sin(a) * r, 3.8f);
    }
}

void FluidSymbolSystem::genQUESTION(float cx, float cy) {
    // 极其清晰的问号图形 (圆弧顶部 + 竖折 + 独立下方圆点)
    for (float a = -2.6f; a <= 1.4f; a += 0.25f) {
        addPoint(cx + std::cos(a) * 9.5f, cy - 8.0f + std::sin(a) * 9.5f, 3.2f);
    }
    for (float y = 1.0f; y <= 6.0f; y += 2.2f) {
        addPoint(cx, cy + y, 3.2f);
    }
    addPoint(cx, cy + 14.0f, 4.2f); // 独立大圆点
}

void FluidSymbolSystem::genEXCLAMATION(float cx, float cy) {
    // 经典的倒梯形/楔形感叹号 (顶部宽厚饱满 -> 向下收窄成尖角 -> 明显留白空隙 -> 独立大圆点)
    
    // 1. 上半部锥形竖笔 (y: -18.0 ~ +2.0)
    // 顶部横向宽厚端帽
    addPoint(cx - 3.5f, cy - 16.0f, 4.5f);
    addPoint(cx + 3.5f, cy - 16.0f, 4.5f);
    addPoint(cx, cy - 16.0f, 5.0f);

    // 躯干渐变收窄 (从半径 4.2px 平滑锥形过渡到 1.9px)
    for (float y = -13.0f; y <= 2.0f; y += 2.5f) {
        float t = (y - (-13.0f)) / (2.0f - (-13.0f)); // 0.0 ~ 1.0
        float r = 4.2f * (1.0f - t * 0.55f); // 4.2px -> 1.9px
        float spread = 2.5f * (1.0f - t);   // 顶部双翼收窄
        if (spread > 0.8f) {
            addPoint(cx - spread, cy + y, r);
            addPoint(cx + spread, cy + y, r);
        } else {
            addPoint(cx, cy + y, r);
        }
    }

    // 2. 底部独立醒目圆点 (留出 10px 显著空隙，位于 cy + 13.0)
    addPoint(cx, cy + 13.0f, 5.5f);
    addPoint(cx - 1.2f, cy + 13.0f, 4.0f);
    addPoint(cx + 1.2f, cy + 13.0f, 4.0f);
}

void FluidSymbolSystem::genEYE(float cx, float cy) {
    // 七肢桶环形观察之眼 (外圈多重有机涟漪圆环 + 中心凝视瞳孔)
    for (float a = 0; a < 6.28f; a += 0.20f) {
        float r = 18.0f + std::sin(a * 4.0f) * 2.5f;
        addPoint(cx + std::cos(a) * r, cy + std::sin(a) * r, 3.5f);
    }
    addPoint(cx, cy, 5.5f); // 中心大瞳孔
}

void FluidSymbolSystem::genHEART(float cx, float cy) {
    // 清晰饱满的水墨爱心图腾
    for (float t = 0.0f; t < 6.28f; t += 0.20f) {
        float s = std::sin(t);
        float x = 14.0f * s * s * s;
        float y = -(12.0f * std::cos(t) - 4.5f * std::cos(2.0f * t) - 2.0f * std::cos(3.0f * t) - std::cos(4.0f * t));
        addPoint(cx + x, cy + y, 3.4f);
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
        // 刚生成的符号粒子在寿命前段受到绝对保护，绝不被头部原地误擦除
        if (points[i].life > 0.82f) continue;

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

void FluidSymbolSystem::genZz(float cx, float cy) {
    // 渐进变小、向上漂浮的经典呼噜 "Zzz" 气泡墨迹 (大Z -> 中z -> 小z)
    // 1. 大 Z (靠近头部上方)
    float z1_x = cx;
    float z1_y = cy - 6.0f;
    float w1 = 6.5f, h1 = 5.5f;
    // 顶横
    for (float x = -w1; x <= w1; x += 2.2f) addPointWithVel(z1_x + x, z1_y - h1, 3.2f, 0.008f, -0.045f);
    // 斜线
    for (float t = 0.0f; t <= 1.0f; t += 0.16f) addPointWithVel(z1_x + w1 - t * 2.0f * w1, z1_y - h1 + t * 2.0f * h1, 3.2f, 0.008f, -0.045f);
    // 底横
    for (float x = -w1; x <= w1; x += 2.2f) addPointWithVel(z1_x + x, z1_y + h1, 3.2f, 0.008f, -0.045f);

    // 2. 中 z (右上方漂浮)
    float z2_x = cx + 13.0f;
    float z2_y = cy - 20.0f;
    float w2 = 4.5f, h2 = 3.8f;
    for (float x = -w2; x <= w2; x += 1.8f) addPointWithVel(z2_x + x, z2_y - h2, 2.6f, 0.015f, -0.060f);
    for (float t = 0.0f; t <= 1.0f; t += 0.20f) addPointWithVel(z2_x + w2 - t * 2.0f * w2, z2_y - h2 + t * 2.0f * h2, 2.6f, 0.015f, -0.060f);
    for (float x = -w2; x <= w2; x += 1.8f) addPointWithVel(z2_x + x, z2_y + h2, 2.6f, 0.015f, -0.060f);

    // 3. 小 z (最高处悠扬消散)
    float z3_x = cx + 22.0f;
    float z3_y = cy - 32.0f;
    float w3 = 3.0f, h3 = 2.5f;
    for (float x = -w3; x <= w3; x += 1.6f) addPointWithVel(z3_x + x, z3_y - h3, 2.0f, 0.022f, -0.075f);
    for (float t = 0.0f; t <= 1.0f; t += 0.25f) addPointWithVel(z3_x + w3 - t * 2.0f * w3, z3_y - h3 + t * 2.0f * h3, 2.0f, 0.022f, -0.075f);
    for (float x = -w3; x <= w3; x += 1.6f) addPointWithVel(z3_x + x, z3_y + h3, 2.0f, 0.022f, -0.075f);
}

void FluidSymbolSystem::genDIZZY(float cx, float cy) {
    // 经典的双重头晕蚊香眼螺旋水墨符号 (主大蚊香圈 + 右上方小蚊香圈)
    // 1. 主大蚊香螺旋线 (约 2.6 圈，半径从 1.2px 平滑旋绕展开至 16.5px)
    float r_step = 0.96f; // 每弧度半径增加
    for (float theta = 0.75f; theta <= 16.2f; theta += 0.22f) {
        float r = 1.2f + r_step * theta;
        float x = cx + std::cos(theta) * r;
        float y = cy + std::sin(theta) * r;
        addPointWithVel(x, y, 3.2f, 0.005f, -0.040f);
    }

    // 2. 右上方小蚊香螺旋线 (约 1.8 圈，伴随晕眩感漂浮)
    float sub_cx = cx + 17.5f;
    float sub_cy = cy - 12.0f;
    for (float theta = 0.85f; theta <= 11.2f; theta += 0.28f) {
        float r = 0.8f + 0.70f * theta;
        float x = sub_cx + std::cos(theta) * r;
        float y = sub_cy + std::sin(theta) * r;
        addPointWithVel(x, y, 2.5f, 0.012f, -0.055f);
    }
}

void FluidSymbolSystem::genBITE(float cx, float cy) {
    // 经典的共生体利齿咬痕水墨符号 (弧形上下颌骨弓 + 尖锐犬齿/门齿阵列 + 撕裂水墨唾液微滴)
    
    // 1. 上颌骨弓与向下咬合的利齿阵列 (Upper Jaw & Downward Fangs)
    for (float u = -1.0f; u <= 1.0f; u += 0.12f) {
        float x = cx + u * 15.5f;
        float y = cy - 6.5f - (1.0f - u * u) * 4.5f;
        addPoint(x, y, 3.2f);
    }
    // 上颌 5 颗锋利向下尖牙 (两侧犬齿修长突刺，中间切齿均匀交错)
    static const float UPPER_TEETH_X[5] = {-11.5f, -6.0f, 0.0f, 6.0f, 11.5f};
    static const float UPPER_TEETH_LEN[5] = {6.5f, 4.2f, 3.8f, 4.2f, 6.5f};
    for (int i = 0; i < 5; ++i) {
        float tx = cx + UPPER_TEETH_X[i];
        float u = UPPER_TEETH_X[i] / 15.5f;
        float base_y = cy - 6.5f - (1.0f - u * u) * 4.5f;
        for (float l = 1.8f; l <= UPPER_TEETH_LEN[i]; l += 2.0f) {
            float r = 3.2f - (l / UPPER_TEETH_LEN[i]) * 1.5f; // 齿尖变尖锐
            addPoint(tx, base_y + l, r);
        }
    }

    // 2. 下颌骨弓与向上咬合的利齿阵列 (Lower Jaw & Upward Fangs)
    for (float u = -1.0f; u <= 1.0f; u += 0.12f) {
        float x = cx + u * 14.5f;
        float y = cy + 6.5f + (1.0f - u * u) * 4.5f;
        addPoint(x, y, 3.2f);
    }
    // 下颌 4 颗锋利向上尖牙 (与上齿自然错位咬合)
    static const float LOWER_TEETH_X[4] = {-9.5f, -3.2f, 3.2f, 9.5f};
    static const float LOWER_TEETH_LEN[4] = {6.0f, 4.0f, 4.0f, 6.0f};
    for (int i = 0; i < 4; ++i) {
        float tx = cx + LOWER_TEETH_X[i];
        float u = LOWER_TEETH_X[i] / 14.5f;
        float base_y = cy + 6.5f + (1.0f - u * u) * 4.5f;
        for (float l = 1.8f; l <= LOWER_TEETH_LEN[i]; l += 2.0f) {
            float r = 3.2f - (l / LOWER_TEETH_LEN[i]) * 1.5f;
            addPoint(tx, base_y - l, r);
        }
    }

    // 3. 撕裂咬痕水墨微滴 (嘴角飞溅与齿缝滴落)
    addPointWithVel(cx - 16.5f, cy, 2.5f, -0.015f, 0.010f);
    addPointWithVel(cx + 16.5f, cy, 2.5f, 0.015f, 0.010f);
    addPointWithVel(cx - 4.5f, cy + 15.5f, 2.2f, 0.002f, 0.035f);
    addPointWithVel(cx + 5.0f, cy + 16.5f, 2.2f, -0.002f, 0.035f);
}
