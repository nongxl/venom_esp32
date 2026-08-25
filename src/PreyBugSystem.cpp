#include "PreyBugSystem.h"
#include <cmath>

PreyBugSystem::PreyBugSystem() {
    for (int i = 0; i < MAX_BUGS; ++i) {
        bugs[i].active = false;
        bugs[i].state = BUG_DEAD;
    }
}

void PreyBugSystem::init() {
    for (int i = 0; i < MAX_BUGS; ++i) {
        bugs[i].active = false;
        bugs[i].state = BUG_DEAD;
    }
    spawn_cooldown = 1.2f;
}

void PreyBugSystem::spawnNewBug() {
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (!bugs[i].active) {
            PreyBug &b = bugs[i];
            b.active = true;
            b.state = BUG_FREE;
            b.type = (rand() % 2 == 0) ? BUG_CRAWLER : BUG_FLYER;

            // 随机从屏幕四周边缘诞生
            int side = rand() % 4;
            if (side == 0) { b.x = 10.0f + (rand() % (SCREEN_W - 20)); b.y = 8.0f; b.angle = 1.57f; }
            else if (side == 1) { b.x = SCREEN_W - 8.0f; b.y = 10.0f + (rand() % (SCREEN_H - 20)); b.angle = 3.14f; }
            else if (side == 2) { b.x = 10.0f + (rand() % (SCREEN_W - 20)); b.y = SCREEN_H - 8.0f; b.angle = -1.57f; }
            else { b.x = 8.0f; b.y = 10.0f + (rand() % (SCREEN_H - 20)); b.angle = 0.0f; }

            b.base_speed = (b.type == BUG_CRAWLER) ? (14.0f + (rand() % 10)) : (22.0f + (rand() % 16));
            b.current_speed = b.base_speed;
            b.leg_phase = 0.0f;
            b.wing_phase = 0.0f;
            b.speed_change_timer = 1.0f + (rand() % 15) * 0.1f;
            b.state_timer = 0.0f;
            b.glow_phase = (rand() % 100) * 0.1f;
            b.snare_timer = 0.0f;
            break;
        }
    }
}

void PreyBugSystem::updateCrawler(PreyBug &b, float dt, float v_hx, float v_hy) {
    b.state_timer += dt;
    b.glow_phase += dt * 3.0f;

    if (b.state == BUG_SNARED) {
        b.snare_timer += dt;
        b.leg_phase += dt * 25.0f; // 拼命挣扎抖腿
        return;
    }
    if (b.state == BUG_CAUGHT) {
        return;
    }

    // 1. 危险感知逃逸：如果毒液靠近 35px，立即惊慌加速暴走
    float dx = b.x - v_hx;
    float dy = b.y - v_hy;
    float dist_v = std::sqrt(dx * dx + dy * dy);
    if (dist_v < 38.0f && dist_v > 0.1f) {
        float flee_angle = std::atan2(dy, dx);
        b.angle = flee_angle + ((rand() % 40) - 20) * 0.0174f;
        b.current_speed = b.base_speed * 1.85f;
    } else {
        // 2. 正常变速爬行与偶发停顿触角微颤
        b.speed_change_timer -= dt;
        if (b.speed_change_timer <= 0.0f) {
            b.speed_change_timer = 0.8f + (rand() % 18) * 0.1f;
            int act = rand() % 100;
            if (act < 30) {
                b.current_speed = 0.0f; // 停下来观察探路
            } else if (act < 70) {
                b.current_speed = b.base_speed;
                b.angle += ((rand() % 90) - 45) * 0.0174f;
            } else {
                b.current_speed = b.base_speed * 1.5f; // 突然加速小跑
                b.angle += ((rand() % 60) - 30) * 0.0174f;
            }
        }
    }

    // 3. 步态前进
    b.leg_phase += dt * b.current_speed * 0.75f;
    b.vx = std::cos(b.angle) * b.current_speed;
    b.vy = std::sin(b.angle) * b.current_speed;

    b.x += b.vx * dt;
    b.y += b.vy * dt;

    // 4. 边界碰撞掉头
    if (b.x < 8.0f) { b.x = 8.0f; b.angle = 3.14159f - b.angle; }
    if (b.x > SCREEN_W - 8.0f) { b.x = SCREEN_W - 8.0f; b.angle = 3.14159f - b.angle; }
    if (b.y < 8.0f) { b.y = 8.0f; b.angle = -b.angle; }
    if (b.y > SCREEN_H - 8.0f) { b.y = SCREEN_H - 8.0f; b.angle = -b.angle; }
}

void PreyBugSystem::updateFlyer(PreyBug &b, float dt, float v_hx, float v_hy) {
    b.state_timer += dt;
    b.wing_phase += dt * 45.0f; // 高频振翅
    b.glow_phase += dt * 4.0f;

    if (b.state == BUG_SNARED) {
        b.snare_timer += dt;
        return;
    }
    if (b.state == BUG_CAUGHT) {
        return;
    }

    // 1. 危险逃逸
    float dx = b.x - v_hx;
    float dy = b.y - v_hy;
    float dist_v = std::sqrt(dx * dx + dy * dy);
    if (dist_v < 42.0f && dist_v > 0.1f) {
        b.angle = std::atan2(dy, dx) + ((rand() % 60) - 30) * 0.0174f;
        b.current_speed = b.base_speed * 1.9f;
    } else {
        // 2. 空中优雅盘旋与正弦穿梭
        b.speed_change_timer -= dt;
        if (b.speed_change_timer <= 0.0f) {
            b.speed_change_timer = 0.6f + (rand() % 15) * 0.1f;
            b.angle += ((rand() % 120) - 60) * 0.0174f;
            b.current_speed = b.base_speed * (0.7f + (rand() % 60) * 0.01f);
        }
    }

    // 正弦轨迹扰动
    float sine_wobble = std::sin(b.state_timer * 6.0f) * 1.2f;
    float move_angle = b.angle + sine_wobble;

    b.vx = std::cos(move_angle) * b.current_speed;
    b.vy = std::sin(move_angle) * b.current_speed;

    b.x += b.vx * dt;
    b.y += b.vy * dt;

    if (b.x < 8.0f) { b.x = 8.0f; b.angle = 3.14159f - b.angle; }
    if (b.x > SCREEN_W - 8.0f) { b.x = SCREEN_W - 8.0f; b.angle = 3.14159f - b.angle; }
    if (b.y < 8.0f) { b.y = 8.0f; b.angle = -b.angle; }
    if (b.y > SCREEN_H - 8.0f) { b.y = SCREEN_H - 8.0f; b.angle = -b.angle; }
}

void PreyBugSystem::update(float dt, float venom_hx, float venom_hy) {
    bool has_active = false;
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state != BUG_DEAD) {
            has_active = true;
            if (bugs[i].type == BUG_CRAWLER) {
                updateCrawler(bugs[i], dt, venom_hx, venom_hy);
            } else {
                updateFlyer(bugs[i], dt, venom_hx, venom_hy);
            }
        }
    }

    // 自动刷新机制：若场上无虫，则隔 3.5~6.5 秒诞生新虫子
    if (!has_active) {
        spawn_cooldown -= dt;
        if (spawn_cooldown <= 0.0f) {
            spawnNewBug();
            spawn_cooldown = 4.0f + (rand() % 35) * 0.1f;
        }
    }
}

int PreyBugSystem::getNearestBug(float x, float y, float &bug_x, float &bug_y, BugState &state) const {
    int best_idx = -1;
    float min_dist = 99999.0f;

    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state != BUG_DEAD) {
            float dx = bugs[i].x - x;
            float dy = bugs[i].y - y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < min_dist) {
                min_dist = d;
                best_idx = i;
                bug_x = bugs[i].x;
                bug_y = bugs[i].y;
                state = bugs[i].state;
            }
        }
    }
    return best_idx;
}

void PreyBugSystem::snareBug(int idx) {
    if (idx >= 0 && idx < MAX_BUGS && bugs[idx].active) {
        bugs[idx].state = BUG_SNARED;
        bugs[idx].snare_timer = 0.0f;
    }
}

void PreyBugSystem::catchBug(int idx, float at_x, float at_y) {
    if (idx >= 0 && idx < MAX_BUGS && bugs[idx].active) {
        bugs[idx].state = BUG_CAUGHT;
        bugs[idx].x = at_x;
        bugs[idx].y = at_y;
    }
}

void PreyBugSystem::killBug(int idx) {
    if (idx >= 0 && idx < MAX_BUGS) {
        bugs[idx].active = false;
        bugs[idx].state = BUG_DEAD;
    }
}

bool PreyBugSystem::hasActiveBug() const {
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state != BUG_DEAD) return true;
    }
    return false;
}

void PreyBugSystem::drawCrawler(M5Canvas &canvas, const PreyBug &b) const {
    int ix = (int)b.x;
    int iy = (int)b.y;

    // 1. 绘制 6 条划动的小细腿
    float cos_a = std::cos(b.angle);
    float sin_a = std::sin(b.angle);
    float perp_x = -sin_a;
    float perp_y = cos_a;

    for (int leg = -1; leg <= 1; ++leg) {
        float leg_swing = std::sin(b.leg_phase + (float)leg * 1.8f) * 2.2f;
        float leg_root_x = b.x + cos_a * (float)leg * 2.2f;
        float leg_root_y = b.y + sin_a * (float)leg * 2.2f;

        // 左腿
        float l_end_x = leg_root_x + perp_x * 4.2f + cos_a * leg_swing;
        float l_end_y = leg_root_y + perp_y * 4.2f + sin_a * leg_swing;
        canvas.drawLine((int)leg_root_x, (int)leg_root_y, (int)l_end_x, (int)l_end_y, 0xCE79);

        // 右腿
        float r_end_x = leg_root_x - perp_x * 4.2f - cos_a * leg_swing;
        float r_end_y = leg_root_y - perp_y * 4.2f - sin_a * leg_swing;
        canvas.drawLine((int)leg_root_x, (int)leg_root_y, (int)r_end_x, (int)r_end_y, 0xCE79);
    }

    // 2. 绘制金绿光泽椭圆甲壳
    uint16_t shell_col = ((int)(b.glow_phase * 10) % 2 == 0) ? 0xFFE0 : 0x07E0; // 金黄/草绿微光
    canvas.fillCircle(ix, iy, 3, shell_col);
    canvas.drawPixel(ix, iy, 0xFFFF); // 高光

    // 3. 头部前探微小触角
    float ant_x1 = b.x + cos_a * 4.5f + perp_x * 1.8f;
    float ant_y1 = b.y + sin_a * 4.5f + perp_y * 1.8f;
    float ant_x2 = b.x + cos_a * 4.5f - perp_x * 1.8f;
    float ant_y2 = b.y + sin_a * 4.5f - perp_y * 1.8f;
    canvas.drawLine(ix, iy, (int)ant_x1, (int)ant_y1, 0xFFFF);
    canvas.drawLine(ix, iy, (int)ant_x2, (int)ant_y2, 0xFFFF);

    // 4. 纯黑色黏液定身污渍与拉丝特效 (Black Slime Splat)
    if (b.state == BUG_SNARED) {
        // 中心实心纯黑黏液池
        canvas.fillCircle(ix, iy, 5, COLOR_VENOM_CORE);
        // 向外延伸的 5 根黑色黏丝拉爪
        for (int k = 0; k < 5; ++k) {
            float s_angle = (float)k * 1.256f + b.glow_phase * 0.2f;
            float leg_x = b.x + std::cos(s_angle) * 7.5f;
            float leg_y = b.y + std::sin(s_angle) * 7.5f;
            canvas.drawLine(ix, iy, (int)leg_x, (int)leg_y, COLOR_VENOM_CORE);
            canvas.drawPixel((int)leg_x, (int)leg_y, COLOR_DITHER_GRAY);
        }
        canvas.drawPixel(ix + 1, iy + 1, COLOR_DITHER_GRAY);
    }
}

void PreyBugSystem::drawFlyer(M5Canvas &canvas, const PreyBug &b) const {
    int ix = (int)b.x;
    int iy = (int)b.y;

    float cos_a = std::cos(b.angle);
    float sin_a = std::sin(b.angle);
    float perp_x = -sin_a;
    float perp_y = cos_a;

    // 1. 绘制高频振动的透明微翅膀 (随 wing_phase 展开)
    float wing_spread = 3.5f + std::abs(std::sin(b.wing_phase)) * 3.5f;

    // 左翅
    float w1_x = b.x + perp_x * wing_spread;
    float w1_y = b.y + perp_y * wing_spread;
    canvas.drawLine(ix, iy, (int)w1_x, (int)w1_y, 0xFFFF);
    canvas.drawPixel((int)w1_x, (int)w1_y, 0x07FF);

    // 右翅
    float w2_x = b.x - perp_x * wing_spread;
    float w2_y = b.y - perp_y * wing_spread;
    canvas.drawLine(ix, iy, (int)w2_x, (int)w2_y, 0xFFFF);
    canvas.drawPixel((int)w2_x, (int)w2_y, 0x07FF);

    // 2. 绘制纤细发光身体 (荧光粉/青蓝)
    canvas.fillCircle(ix, iy, 2, 0xF81F); // 荧光粉
    canvas.drawPixel(ix, iy, 0xFFFF);

    // 3. 复眼
    float eye_x = b.x + cos_a * 3.0f;
    float eye_y = b.y + sin_a * 3.0f;
    canvas.drawPixel((int)eye_x, (int)eye_y, 0x07E0);

    // 4. 纯黑色黏液定身污渍与拉丝特效 (Black Slime Splat)
    if (b.state == BUG_SNARED) {
        canvas.fillCircle(ix, iy, 5, COLOR_VENOM_CORE);
        for (int k = 0; k < 5; ++k) {
            float s_angle = (float)k * 1.256f + b.glow_phase * 0.2f;
            float leg_x = b.x + std::cos(s_angle) * 7.5f;
            float leg_y = b.y + std::sin(s_angle) * 7.5f;
            canvas.drawLine(ix, iy, (int)leg_x, (int)leg_y, COLOR_VENOM_CORE);
            canvas.drawPixel((int)leg_x, (int)leg_y, COLOR_DITHER_GRAY);
        }
        canvas.drawPixel(ix + 1, iy + 1, COLOR_DITHER_GRAY);
    }
}

void PreyBugSystem::draw(M5Canvas &canvas) const {
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state != BUG_DEAD) {
            if (bugs[i].type == BUG_CRAWLER) {
                drawCrawler(canvas, bugs[i]);
            } else {
                drawFlyer(canvas, bugs[i]);
            }
        }
    }
}
