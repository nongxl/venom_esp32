#include "FluidSymbolSystem.h"
#include <cmath>

FluidSymbolSystem::FluidSymbolSystem() {
    for (int i = 0; i < MAX_INK_PARTICLES; ++i) {
        particles[i].active = false;
    }
}

void FluidSymbolSystem::init() {
    for (int i = 0; i < MAX_INK_PARTICLES; ++i) {
        particles[i].active = false;
    }
    active_particle_count = 0;
    current_type = SYMBOL_NONE;
}

void FluidSymbolSystem::addParticle(float x, float y, float vx, float vy, float r, float duration, bool splatter, bool drip) {
    for (int i = 0; i < MAX_INK_PARTICLES; ++i) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].x = x;
            particles[i].y = y;
            particles[i].vx = vx;
            particles[i].vy = vy;
            particles[i].radius = r;
            particles[i].life = duration;
            particles[i].max_life = duration;
            particles[i].is_splatter = splatter;
            particles[i].is_drip = drip;
            active_particle_count++;
            break;
        }
    }
}

void FluidSymbolSystem::addStrokeLine(float x1, float y1, float x2, float y2, float r1, float r2, int steps) {
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        float px = x1 + (x2 - x1) * t + ((rand() % 20) - 10) * 0.08f;
        float py = y1 + (y2 - y1) * t + ((rand() % 20) - 10) * 0.08f;
        float r = r1 + (r2 - r1) * t + ((rand() % 10) - 5) * 0.06f;
        if (r < 1.6f) r = 1.6f;
        addParticle(px, py, 0.0f, 0.0f, r, 4.5f, false, (t > 0.8f));
    }
}

void FluidSymbolSystem::addSplatterBurst(float cx, float cy, int count, float max_spread) {
    for (int i = 0; i < count; ++i) {
        float angle = (rand() % 360) * 0.017453f;
        float dist = 4.0f + (rand() % 100) * 0.01f * max_spread;
        float px = cx + std::cos(angle) * dist;
        float py = cy + std::sin(angle) * dist;
        float r = 1.0f + (rand() % 15) * 0.1f;
        addParticle(px, py, std::cos(angle) * 0.1f, std::sin(angle) * 0.1f, r, 3.8f, true, false);
    }
}

void FluidSymbolSystem::buildRingGlyph(float cx, float cy) {
    // 电影《Arrival》七肢桶风格泼墨圆环
    constexpr int RING_STEPS = 18;
    float prev_x = 0.0f, prev_y = 0.0f;

    for (int i = 0; i <= RING_STEPS; ++i) {
        float angle = (i % RING_STEPS) * (360.0f / RING_STEPS) * 0.017453f;
        // 有机粗细与突刺调制
        float spike = (i % 3 == 0) ? 5.5f : ((i % 2 == 0) ? -2.5f : 2.0f);
        float r_ring = 14.0f + spike;
        float px = cx + std::cos(angle) * r_ring;
        float py = cy + std::sin(angle) * r_ring;
        float stroke_w = (i % 4 == 0) ? 3.8f : 2.6f;

        addParticle(px, py, 0.0f, 0.0f, stroke_w, 4.5f, false, (i % 5 == 0));

        // 沿途向外炸出的微小飞溅毛刺
        if (i % 3 == 0) {
            float sp_ang = angle + ((rand() % 40) - 20) * 0.017453f;
            addParticle(cx + std::cos(sp_ang) * (r_ring + 6.0f), cy + std::sin(sp_ang) * (r_ring + 6.0f),
                        std::cos(sp_ang) * 0.15f, std::sin(sp_ang) * 0.15f, 1.4f, 4.0f, true, false);
        }
    }

    // 周围随机泼墨微粒
    addSplatterBurst(cx, cy, 6, 18.0f);
    // 中央微小墨核
    addParticle(cx, cy, 0.0f, 0.0f, 3.6f, 4.5f, false, false);
}

void FluidSymbolSystem::buildQuestionGlyph(float cx, float cy) {
    // 苍劲泼墨问号
    // 弧线段
    constexpr int ARC_STEPS = 9;
    for (int i = 0; i <= ARC_STEPS; ++i) {
        float t = (float)i / (float)ARC_STEPS;
        float angle = (-170.0f + t * 240.0f) * 0.017453f;
        float px = cx + std::cos(angle) * 11.0f;
        float py = cy - 7.0f + std::sin(angle) * 11.0f;
        float r = 3.4f - t * 1.2f;
        addParticle(px, py, 0.0f, 0.0f, r, 4.2f, false, false);
    }

    // 中间竖弯
    addStrokeLine(cx + 2.0f, cy + 2.0f, cx - 1.0f, cy + 8.0f, 2.6f, 2.0f, 3);
    // 底部炸裂悬浮墨花与滴落
    addParticle(cx - 1.0f, cy + 15.0f, 0.0f, 0.0f, 3.4f, 4.2f, false, true);
    addSplatterBurst(cx - 1.0f, cy + 15.0f, 4, 6.0f);
}

void FluidSymbolSystem::buildExclamationGlyph(float cx, float cy) {
    // 撕裂泼墨叹号
    addStrokeLine(cx, cy - 15.0f, cx, cy + 4.0f, 3.8f, 2.0f, 6);
    addSplatterBurst(cx, cy - 6.0f, 4, 8.0f);

    // 底部墨点
    addParticle(cx, cy + 13.0f, 0.0f, 0.0f, 3.5f, 4.0f, false, true);
    addSplatterBurst(cx, cy + 13.0f, 3, 5.0f);
}

void FluidSymbolSystem::buildCrossGlyph(float cx, float cy) {
    // 撕裂泼墨叉号
    addStrokeLine(cx - 11.0f, cy - 11.0f, cx + 11.0f, cy + 11.0f, 3.2f, 2.2f, 6);
    addStrokeLine(cx - 11.0f, cy + 11.0f, cx + 11.0f, cy - 11.0f, 3.2f, 2.2f, 6);
    addSplatterBurst(cx, cy, 5, 12.0f);
}

void FluidSymbolSystem::buildRippleGlyph(float cx, float cy) {
    for (int i = 0; i < 8; ++i) {
        float a = i * 45.0f * 0.017453f;
        addParticle(cx + std::cos(a) * 8.0f, cy + std::sin(a) * 8.0f, 0.0f, 0.0f, 2.4f, 3.8f, false, false);
    }
    for (int i = 0; i < 12; ++i) {
        float a = i * 30.0f * 0.017453f;
        addParticle(cx + std::cos(a) * 16.0f, cy + std::sin(a) * 16.0f, 0.0f, 0.0f, 2.0f, 3.8f, false, false);
    }
    addSplatterBurst(cx, cy, 4, 14.0f);
}

void FluidSymbolSystem::spawnSymbol(FluidSymbolType type, float origin_x, float origin_y) {
    for (int i = 0; i < MAX_INK_PARTICLES; ++i) particles[i].active = false;
    active_particle_count = 0;
    current_type = type;

    float cx = std::max(35.0f, std::min(SCREEN_W - 35.0f, origin_x));
    float cy = std::max(30.0f, std::min(SCREEN_H - 30.0f, origin_y));

    switch (type) {
        case SYMBOL_RING:        buildRingGlyph(cx, cy); break;
        case SYMBOL_QUESTION:    buildQuestionGlyph(cx, cy); break;
        case SYMBOL_EXCLAMATION: buildExclamationGlyph(cx, cy); break;
        case SYMBOL_CROSS:       buildCrossGlyph(cx, cy); break;
        case SYMBOL_RIPPLE:      buildRippleGlyph(cx, cy); break;
        default: break;
    }
}

void FluidSymbolSystem::update(float dt, const SkeletonSystem &skeleton, float gravity_x, float gravity_y) {
    float hx, hy;
    skeleton.getHeadPos(hx, hy);
    active_particle_count = 0;

    for (int i = 0; i < MAX_INK_PARTICLES; ++i) {
        if (!particles[i].active) continue;

        FluidInkParticle &p = particles[i];
        p.life -= dt;

        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }

        // 速度阻尼
        p.vx *= 0.90f;
        p.vy *= 0.90f;

        // 滴落流淌
        if (p.is_drip) {
            p.vy += (gravity_y * 0.12f + 0.06f) * dt * 30.0f;
            p.vx += gravity_x * 0.12f * dt * 30.0f;
        }

        p.x += p.vx;
        p.y += p.vy;

        p.x = std::max(2.0f, std::min((float)(SCREEN_W - 2), p.x));
        p.y = std::max(2.0f, std::min((float)(SCREEN_H - 2), p.y));

        float dx = hx - p.x;
        float dy = hy - p.y;
        if (dx * dx + dy * dy < 280.0f && p.life < p.max_life * 0.65f) {
            p.active = false;
            continue;
        }

        active_particle_count++;
    }

    if (active_particle_count == 0) {
        current_type = SYMBOL_NONE;
    }
}

void FluidSymbolSystem::draw(M5Canvas &canvas) const {
    if (active_particle_count == 0) return;

    // 1. 在主干粒子间绘制连接带（消除圆点感，形成流动泼墨笔触）
    for (int i = 0; i < MAX_INK_PARTICLES; ++i) {
        if (!particles[i].active || particles[i].is_splatter) continue;

        const FluidInkParticle &p1 = particles[i];
        for (int j = i + 1; j < MAX_INK_PARTICLES; ++j) {
            if (!particles[j].active || particles[j].is_splatter) continue;

            const FluidInkParticle &p2 = particles[j];
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float dist2 = dx * dx + dy * dy;

            // 相邻主干粒子连成平滑带
            if (dist2 < 48.0f) {
                int thickness = (int)((p1.radius + p2.radius) * 0.5f);
                for (int off = -thickness / 2; off <= thickness / 2; ++off) {
                    canvas.drawLine((int)p1.x + off, (int)p1.y, (int)p2.x + off, (int)p2.y, COLOR_INK_BLACK);
                    canvas.drawLine((int)p1.x, (int)p1.y + off, (int)p2.x, (int)p2.y + off, COLOR_INK_BLACK);
                }
            }
        }
    }

    // 2. 绘制粒子核心与飞溅反光
    for (int i = 0; i < MAX_INK_PARTICLES; ++i) {
        if (!particles[i].active) continue;

        const FluidInkParticle &p = particles[i];
        int ix = (int)std::round(p.x);
        int iy = (int)std::round(p.y);
        int ir = (int)std::round(p.radius);

        if (ix < 0 || ix >= SCREEN_W || iy < 0 || iy >= SCREEN_H) continue;

        canvas.fillCircle(ix, iy, ir, COLOR_INK_BLACK);

        if (ir >= 2 && !p.is_splatter) {
            canvas.drawPixel(ix - 1, iy - 1, COLOR_INK_GLOW);
        }
    }
}
