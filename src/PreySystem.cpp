#include "PreySystem.h"
#include <cmath>

PreySystem::PreySystem() {
    init();
}

void PreySystem::init() {
    for (int i = 0; i < MAX_BUGS; ++i) {
        bugs[i].active = false;
        bugs[i].state = PREY_FREE;
    }
    slime_proj.active = false;
    spawn_timer = 2.0f; // 启动 2 秒后生成第一只小虫子
}

int PreySystem::getActiveBugCount() const {
    int count = 0;
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state != PREY_EATEN) count++;
    }
    return count;
}

void PreySystem::spawnBug() {
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (!bugs[i].active) {
            PreyBug &b = bugs[i];
            b.active = true;
            b.type = (rand() % 2 == 0) ? BUG_CRAWLER : BUG_FLYER;
            b.state = PREY_FREE;
            b.state_timer = 0.0f;
            b.life_timer = 0.0f;
            b.pause_timer = 0.0f;
            b.anim_phase = (rand() % 100) * 0.1f;
            b.leg_phase = 0.0f;
            b.wing_phase = 0.0f;

            // 从屏幕四周边沿随机出生
            int side = rand() % 4;
            if (side == 0) { // 上边缘
                b.x = 20.0f + (rand() % (SCREEN_W - 40));
                b.y = 8.0f;
            } else if (side == 1) { // 下边缘
                b.x = 20.0f + (rand() % (SCREEN_W - 40));
                b.y = SCREEN_H - 8.0f;
            } else if (side == 2) { // 左边缘
                b.x = 8.0f;
                b.y = 15.0f + (rand() % (SCREEN_H - 30));
            } else { // 右边缘
                b.x = SCREEN_W - 8.0f;
                b.y = 15.0f + (rand() % (SCREEN_H - 30));
            }

            b.target_x = 30.0f + (rand() % (SCREEN_W - 60));
            b.target_y = 20.0f + (rand() % (SCREEN_H - 40));

            float dx = b.target_x - b.x;
            float dy = b.target_y - b.y;
            b.heading_angle = std::atan2(dy, dx);
            b.speed = (b.type == BUG_CRAWLER) ? (14.0f + (rand() % 12)) : (22.0f + (rand() % 18));
            break;
        }
    }
}

int PreySystem::findClosestBug(float from_x, float from_y, float &out_dist, float max_radius) const {
    int best_idx = -1;
    float best_dist = max_radius;

    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state != PREY_EATEN) {
            float dx = bugs[i].x - from_x;
            float dy = bugs[i].y - from_y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < best_dist) {
                best_dist = d;
                best_idx = i;
            }
        }
    }

    out_dist = best_dist;
    return best_idx;
}

bool PreySystem::getBugPos(int bug_idx, float &out_x, float &out_y) const {
    if (bug_idx < 0 || bug_idx >= MAX_BUGS || !bugs[bug_idx].active) return false;
    out_x = bugs[bug_idx].x;
    out_y = bugs[bug_idx].y;
    return true;
}

PreyState PreySystem::getBugState(int bug_idx) const {
    if (bug_idx < 0 || bug_idx >= MAX_BUGS || !bugs[bug_idx].active) return PREY_EATEN;
    return bugs[bug_idx].state;
}

void PreySystem::hookBugWithTongue(int bug_idx) {
    if (bug_idx >= 0 && bug_idx < MAX_BUGS && bugs[bug_idx].active) {
        bugs[bug_idx].state = PREY_TONGUE_HOOKED;
    }
}

void PreySystem::grabBugWithTentacle(int bug_idx) {
    if (bug_idx >= 0 && bug_idx < MAX_BUGS && bugs[bug_idx].active) {
        bugs[bug_idx].state = PREY_GRABBED;
    }
}

void PreySystem::launchSlimeSnare(float from_x, float from_y, int bug_idx) {
    if (bug_idx < 0 || bug_idx >= MAX_BUGS || !bugs[bug_idx].active) return;
    slime_proj.active = true;
    slime_proj.start_x = from_x;
    slime_proj.start_y = from_y;
    slime_proj.x = from_x;
    slime_proj.y = from_y;
    slime_proj.target_x = bugs[bug_idx].x;
    slime_proj.target_y = bugs[bug_idx].y;
    slime_proj.progress = 0.0f;
    slime_proj.target_bug_idx = bug_idx;
}

void PreySystem::eatBug(int bug_idx) {
    if (bug_idx >= 0 && bug_idx < MAX_BUGS && bugs[bug_idx].active) {
        bugs[bug_idx].state = PREY_EATEN;
        bugs[bug_idx].active = false;
    }
}

void PreySystem::updateSnaredBugPos(int bug_idx, float new_x, float new_y) {
    if (bug_idx >= 0 && bug_idx < MAX_BUGS && bugs[bug_idx].active) {
        bugs[bug_idx].x = new_x;
        bugs[bug_idx].y = new_y;
    }
}

void PreySystem::updateCrawler(PreyBug &bug, float dt) {
    bug.leg_phase += dt * (bug.speed * 0.8f);

    if (bug.pause_timer > 0.0f) {
        bug.pause_timer -= dt;
        return;
    }

    float dx = bug.target_x - bug.x;
    float dy = bug.target_y - bug.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 8.0f || bug.life_timer > 18.0f) {
        // 偶尔停顿探索
        if ((rand() % 100) < 35) {
            bug.pause_timer = 0.4f + (rand() % 8) * 0.1f;
        }
        // 选取下一个巡游点
        bug.target_x = 15.0f + (rand() % (SCREEN_W - 30));
        bug.target_y = 12.0f + (rand() % (SCREEN_H - 24));
        dx = bug.target_x - bug.x;
        dy = bug.target_y - bug.y;
        dist = std::sqrt(dx * dx + dy * dy);
    }

    if (dist > 0.01f) {
        float target_angle = std::atan2(dy, dx);
        // 平滑转向
        float diff = target_angle - bug.heading_angle;
        while (diff > 3.14159f) diff -= 6.28318f;
        while (diff < -3.14159f) diff += 6.28318f;
        bug.heading_angle += diff * dt * 4.0f;

        bug.x += std::cos(bug.heading_angle) * bug.speed * dt;
        bug.y += std::sin(bug.heading_angle) * bug.speed * dt;
    }

    // 屏幕边缘软边界约束
    bug.x = std::max(6.0f, std::min(SCREEN_W - 6.0f, bug.x));
    bug.y = std::max(6.0f, std::min(SCREEN_H - 6.0f, bug.y));
}

void PreySystem::updateFlyer(PreyBug &bug, float dt) {
    bug.wing_phase += dt * 38.0f; // 38Hz 高频振翅
    bug.anim_phase += dt * 2.5f;

    float dx = bug.target_x - bug.x;
    float dy = bug.target_y - bug.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 10.0f || bug.life_timer > 22.0f) {
        bug.target_x = 25.0f + (rand() % (SCREEN_W - 50));
        bug.target_y = 18.0f + (rand() % (SCREEN_H - 36));
        dx = bug.target_x - bug.x;
        dy = bug.target_y - bug.y;
        dist = std::sqrt(dx * dx + dy * dy);
    }

    if (dist > 0.01f) {
        float target_angle = std::atan2(dy, dx);
        float diff = target_angle - bug.heading_angle;
        while (diff > 3.14159f) diff -= 6.28318f;
        while (diff < -3.14159f) diff += 6.28318f;
        bug.heading_angle += diff * dt * 3.2f;

        // 加入正弦轻盈微悬停漂移
        float sway = std::sin(bug.anim_phase) * 6.0f;
        float fx = std::cos(bug.heading_angle) * bug.speed + std::sin(bug.heading_angle + 1.57f) * sway;
        float fy = std::sin(bug.heading_angle) * bug.speed - std::cos(bug.heading_angle + 1.57f) * sway;

        bug.x += fx * dt;
        bug.y += fy * dt;
    }

    bug.x = std::max(8.0f, std::min(SCREEN_W - 8.0f, bug.x));
    bug.y = std::max(8.0f, std::min(SCREEN_H - 8.0f, bug.y));
}

void PreySystem::updateSlimeProjectile(float dt) {
    if (!slime_proj.active) return;

    float dx = slime_proj.target_x - slime_proj.start_x;
    float dy = slime_proj.target_y - slime_proj.start_y;
    float total_dist = std::sqrt(dx * dx + dy * dy);

    if (total_dist < 1.0f) {
        slime_proj.active = false;
        return;
    }

    slime_proj.progress += (slime_proj.speed * dt) / total_dist;

    if (slime_proj.progress >= 1.0f) {
        slime_proj.progress = 1.0f;
        slime_proj.active = false;

        // 黏液命中虫子，触发定身！
        int idx = slime_proj.target_bug_idx;
        if (idx >= 0 && idx < MAX_BUGS && bugs[idx].active) {
            bugs[idx].state = PREY_SNARED;
            bugs[idx].snare_timer = 0.0f;
            bugs[idx].snare_x = bugs[idx].x;
            bugs[idx].snare_y = bugs[idx].y;
        }
    }

    slime_proj.x = slime_proj.start_x + dx * slime_proj.progress;
    slime_proj.y = slime_proj.start_y + dy * slime_proj.progress;
}

void PreySystem::update(float dt) {
    // 1. 定时自动生成虫子（若场上少于 1 只，3~6s 后生成新虫子）
    if (getActiveBugCount() < 1) {
        spawn_timer += dt;
        if (spawn_timer > 3.8f) {
            spawn_timer = 0.0f;
            spawnBug();
        }
    } else {
        spawn_timer = 0.0f;
    }

    // 2. 更新黏液射弹
    updateSlimeProjectile(dt);

    // 3. 更新虫子运动与状态
    for (int i = 0; i < MAX_BUGS; ++i) {
        PreyBug &b = bugs[i];
        if (!b.active) continue;

        b.state_timer += dt;
        b.life_timer += dt;

        switch (b.state) {
            case PREY_FREE:
                if (b.type == BUG_CRAWLER) updateCrawler(b, dt);
                else updateFlyer(b, dt);
                break;

            case PREY_SNARED:
                // 定身状态：被黏在原地微弱蹬腿挣扎
                b.snare_timer += dt;
                b.leg_phase += dt * 25.0f; // 极速乱蹬
                b.x = b.snare_x + (std::sin(b.snare_timer * 30.0f) * 0.8f);
                b.y = b.snare_y + (std::cos(b.snare_timer * 30.0f) * 0.8f);
                // 12秒内未被吃掉则挣脱
                if (b.snare_timer > 12.0f) {
                    b.state = PREY_FREE;
                }
                break;

            case PREY_GRABBED:
            case PREY_TONGUE_HOOKED:
                // 随外力牵引运动，自身快速摆腿
                b.leg_phase += dt * 30.0f;
                b.wing_phase += dt * 45.0f;
                break;

            case PREY_EATEN:
                b.active = false;
                break;
        }
    }
}

void PreySystem::drawCrawler(M5Canvas &canvas, const PreyBug &bug) const {
    int cx = (int)std::round(bug.x);
    int cy = (int)std::round(bug.y);
    float ca = std::cos(bug.heading_angle);
    float sa = std::sin(bug.heading_angle);
    float perp_x = -sa;
    float perp_y = ca;

    // 1. 绘制 6 条微小细足（带步态前后摆动）
    uint16_t leg_color = canvas.color565(40, 60, 45);
    for (int side = -1; side <= 1; side += 2) {
        for (int p = -1; p <= 1; ++p) {
            float leg_swing = std::sin(bug.leg_phase + (float)p * 1.6f + (float)side * 1.57f) * 2.2f;
            float root_x = bug.x + ca * (float)p * 2.2f + perp_x * (float)side * 2.0f;
            float root_y = bug.y + sa * (float)p * 2.2f + perp_y * (float)side * 2.0f;
            float tip_x = root_x + perp_x * (float)side * 3.5f + ca * leg_swing;
            float tip_y = root_y + perp_y * (float)side * 3.5f + sa * leg_swing;
            canvas.drawLine((int)root_x, (int)root_y, (int)tip_x, (int)tip_y, leg_color);
        }
    }

    // 2. 绘制甲壳躯干 (金绿/甲虫色)
    uint16_t shell_color = canvas.color565(30, 160, 80);
    uint16_t highlight_color = canvas.color565(120, 240, 150);
    canvas.fillCircle(cx, cy, 3, shell_color);
    canvas.fillCircle((int)(cx + ca * 2.0f), (int)(cy + sa * 2.0f), 2, shell_color);
    canvas.drawPixel(cx, cy, highlight_color);

    // 3. 绘制 2 根小触须
    float ant_len = 4.0f;
    for (int a = -1; a <= 1; a += 2) {
        float a_angle = bug.heading_angle + (float)a * 0.45f;
        float ax = bug.x + ca * 3.0f + std::cos(a_angle) * ant_len;
        float ay = bug.y + sa * 3.0f + std::sin(a_angle) * ant_len;
        canvas.drawLine((int)(bug.x + ca * 2.0f), (int)(bug.y + sa * 2.0f), (int)ax, (int)ay, leg_color);
    }
}

void PreySystem::drawFlyer(M5Canvas &canvas, const PreyBug &bug) const {
    int cx = (int)std::round(bug.x);
    int cy = (int)std::round(bug.y);
    float ca = std::cos(bug.heading_angle);
    float sa = std::sin(bug.heading_angle);
    float perp_x = -sa;
    float perp_y = ca;

    // 1. 绘制高频微扑棱双翅 (亮白青色半透明)
    uint16_t wing_color = canvas.color565(160, 230, 255);
    float wing_span = 4.5f * std::abs(std::sin(bug.wing_phase));
    for (int side = -1; side <= 1; side += 2) {
        float wx = bug.x + perp_x * (float)side * (3.0f + wing_span);
        float wy = bug.y + perp_y * (float)side * (3.0f + wing_span) - 1.5f;
        canvas.drawLine(cx, cy, (int)wx, (int)wy, wing_color);
        canvas.drawPixel((int)wx, (int)wy, COLOR_GLOW_CYAN);
    }

    // 2. 绘制小黑圆飞虫躯体与微荧光腹部
    canvas.fillCircle(cx, cy, 2, canvas.color565(20, 20, 30));
    canvas.drawPixel(cx, cy, COLOR_GLOW_CYAN);
}

void PreySystem::drawSnareEffect(M5Canvas &canvas, const PreyBug &bug) const {
    int sx = (int)std::round(bug.snare_x);
    int sy = (int)std::round(bug.snare_y);

    // 黑色/深蓝液态黏液吸附大斑块
    canvas.fillCircle(sx, sy, 6, COLOR_VENOM_CORE);
    canvas.drawCircle(sx, sy, 7, COLOR_VENOM_CORE);
    canvas.drawCircle(sx, sy, 8, canvas.color565(15, 20, 35));

    // 黏液拉丝
    for (int a = 0; a < 4; ++a) {
        float ang = (float)a * 1.57f + 0.3f;
        int rx = sx + (int)(std::cos(ang) * 9.0f);
        int ry = sy + (int)(std::sin(ang) * 9.0f);
        canvas.drawLine(sx, sy, rx, ry, COLOR_VENOM_CORE);
    }
}

void PreySystem::drawSlimeProjectile(M5Canvas &canvas) const {
    if (!slime_proj.active) return;
    int px = (int)std::round(slime_proj.x);
    int py = (int)std::round(slime_proj.y);

    // 射弹主体 (高亮水滴形黑色共生体墨球)
    canvas.fillCircle(px, py, 3, COLOR_VENOM_CORE);
    canvas.drawPixel(px, py, COLOR_GLOW_CYAN);
    canvas.drawPixel(px - 1, py - 1, canvas.color565(60, 80, 110));
}

void PreySystem::draw(M5Canvas &canvas) const {
    // 1. 绘制定身黏液斑
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state == PREY_SNARED) {
            drawSnareEffect(canvas, bugs[i]);
        }
    }

    // 2. 绘制黏液飞弹
    drawSlimeProjectile(canvas);

    // 3. 绘制虫子本体
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (!bugs[i].active || bugs[i].state == PREY_EATEN) continue;
        if (bugs[i].type == BUG_CRAWLER) {
            drawCrawler(canvas, bugs[i]);
        } else {
            drawFlyer(canvas, bugs[i]);
        }
    }
}
