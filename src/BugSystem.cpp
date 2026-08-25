#include "BugSystem.h"
#include <cmath>

BugSystem::BugSystem() {
    current_bug.active = false;
}

void BugSystem::init() {
    current_bug.active = false;
    current_bug.state = BUG_STATE_INACTIVE;
    spawn_cooldown = 3.0f + (rand() % 40) * 0.1f;
}

void BugSystem::spawnRandomBug() {
    current_bug.active = true;
    current_bug.type = (rand() % 2 == 0) ? BUG_CRAWLER : BUG_FLYER;
    current_bug.state = BUG_STATE_ROAMING;
    current_bug.life_timer = 0.0f;
    current_bug.max_life = 12.0f + (rand() % 40) * 0.1f;

    if (current_bug.type == BUG_CRAWLER) {
        current_bug.wall_side = rand() % 4;
        current_bug.base_speed = 14.0f + (rand() % 16);
        current_bug.move_timer = 1.0f + (rand() % 15) * 0.1f;
        current_bug.pause_timer = 0.0f;

        switch (current_bug.wall_side) {
            case 0: current_bug.x = 20.0f + (rand() % (SCREEN_W - 40)); current_bug.y = 5.0f; break;
            case 1: current_bug.x = SCREEN_W - 5.0f; current_bug.y = 20.0f + (rand() % (SCREEN_H - 40)); break;
            case 2: current_bug.x = 20.0f + (rand() % (SCREEN_W - 40)); current_bug.y = SCREEN_H - 5.0f; break;
            case 3: current_bug.x = 5.0f; current_bug.y = 20.0f + (rand() % (SCREEN_H - 40)); break;
        }
    } else {
        // 飞虫从屏幕边缘随机飞入
        current_bug.x = (rand() % 2 == 0) ? 10.0f : (SCREEN_W - 10.0f);
        current_bug.y = 25.0f + (rand() % (SCREEN_H - 50));
        current_bug.vx = ((rand() % 40) - 20) * 0.8f;
        current_bug.vy = ((rand() % 40) - 20) * 0.8f;
        current_bug.base_speed = 22.0f + (rand() % 18);
        current_bug.fly_phase = (rand() % 100) * 0.1f;
        current_bug.wing_phase = 0.0f;
        current_bug.glow_intensity = 1.0f;
    }
}

void BugSystem::spawnBugManual(BugType type) {
    current_bug.active = true;
    current_bug.type = type;
    current_bug.state = BUG_STATE_ROAMING;
    current_bug.life_timer = 0.0f;
    current_bug.max_life = 15.0f;
    current_bug.x = SCREEN_W * 0.5f;
    current_bug.y = 35.0f;
    current_bug.base_speed = 20.0f;
}

void BugSystem::setBugSlimed() {
    if (current_bug.active) {
        current_bug.state = BUG_STATE_SLIMED;
        current_bug.vx = 0.0f;
        current_bug.vy = 0.0f;
    }
}

void BugSystem::setBugCaptured(float to_x, float to_y) {
    if (current_bug.active) {
        current_bug.state = BUG_STATE_CAPTURED;
        current_bug.x = to_x;
        current_bug.y = to_y;
    }
}

void BugSystem::setBugEaten() {
    current_bug.active = false;
    current_bug.state = BUG_STATE_EATEN;
    spawn_cooldown = 4.0f + (rand() % 35) * 0.1f; // 4~7.5 秒后刷新下一只虫子
}

void BugSystem::updateCrawlerPhysics(float dt) {
    if (current_bug.pause_timer > 0.0f) {
        current_bug.pause_timer -= dt;
        return;
    }

    current_bug.move_timer -= dt;
    if (current_bug.move_timer <= 0.0f) {
        // 走走停停
        current_bug.pause_timer = 0.3f + (rand() % 6) * 0.1f;
        current_bug.move_timer = 1.2f + (rand() % 12) * 0.1f;
        // 随机变速
        current_bug.base_speed = 12.0f + (rand() % 22);
    }

    float speed = current_bug.base_speed * dt;
    switch (current_bug.wall_side) {
        case 0: // 顶边向右爬
            current_bug.x += speed;
            if (current_bug.x >= SCREEN_W - 6.0f) { current_bug.x = SCREEN_W - 6.0f; current_bug.wall_side = 1; }
            break;
        case 1: // 右边向下爬
            current_bug.y += speed;
            if (current_bug.y >= SCREEN_H - 6.0f) { current_bug.y = SCREEN_H - 6.0f; current_bug.wall_side = 2; }
            break;
        case 2: // 底边向左爬
            current_bug.x -= speed;
            if (current_bug.x <= 6.0f) { current_bug.x = 6.0f; current_bug.wall_side = 3; }
            break;
        case 3: // 左边向上爬
            current_bug.y -= speed;
            if (current_bug.y <= 6.0f) { current_bug.y = 6.0f; current_bug.wall_side = 0; }
            break;
    }
}

void BugSystem::updateFlyerPhysics(float dt) {
    current_bug.fly_phase += dt * 3.5f;
    current_bug.wing_phase += dt * 25.0f; // 翅膀极速扇动

    // 随机变速游走
    float target_vx = std::cos(current_bug.fly_phase * 0.7f) * current_bug.base_speed + std::sin(current_bug.fly_phase * 1.5f) * 10.0f;
    float target_vy = std::sin(current_bug.fly_phase * 0.9f) * current_bug.base_speed + std::cos(current_bug.fly_phase * 2.1f) * 8.0f;

    current_bug.vx = current_bug.vx * 0.85f + target_vx * 0.15f;
    current_bug.vy = current_bug.vy * 0.85f + target_vy * 0.15f;

    current_bug.x += current_bug.vx * dt;
    current_bug.y += current_bug.vy * dt;

    // 边界反弹
    if (current_bug.x < 12.0f) { current_bug.x = 12.0f; current_bug.vx = std::abs(current_bug.vx) + 8.0f; }
    if (current_bug.x > SCREEN_W - 12.0f) { current_bug.x = SCREEN_W - 12.0f; current_bug.vx = -std::abs(current_bug.vx) - 8.0f; }
    if (current_bug.y < 12.0f) { current_bug.y = 12.0f; current_bug.vy = std::abs(current_bug.vy) + 6.0f; }
    if (current_bug.y > SCREEN_H - 12.0f) { current_bug.y = SCREEN_H - 12.0f; current_bug.vy = -std::abs(current_bug.vy) - 6.0f; }

    current_bug.glow_intensity = 0.6f + 0.4f * std::sin(current_bug.fly_phase * 2.0f);
}

void BugSystem::update(float dt) {
    if (!current_bug.active) {
        spawn_cooldown -= dt;
        if (spawn_cooldown <= 0.0f) {
            spawnRandomBug();
        }
        return;
    }

    current_bug.life_timer += dt;
    if (current_bug.life_timer >= current_bug.max_life && current_bug.state == BUG_STATE_ROAMING) {
        // 自然飞离/逃逸
        current_bug.active = false;
        current_bug.state = BUG_STATE_INACTIVE;
        spawn_cooldown = 4.0f + (rand() % 35) * 0.1f;
        return;
    }

    if (current_bug.state == BUG_STATE_ROAMING) {
        if (current_bug.type == BUG_CRAWLER) {
            updateCrawlerPhysics(dt);
        } else {
            updateFlyerPhysics(dt);
        }
    }
}

void BugSystem::draw(M5Canvas &canvas) const {
    if (!current_bug.active || current_bug.state == BUG_STATE_EATEN) return;

    int bx = (int)current_bug.x;
    int by = (int)current_bug.y;

    if (current_bug.type == BUG_CRAWLER) {
        // 绘制甲虫小爬虫
        uint16_t beetle_col = 0xFD20; // 亮橙琥珀色
        canvas.fillCircle(bx, by, 3, beetle_col);
        canvas.drawCircle(bx, by, 3, 0x0000);

        // 绘制小触角与微腿
        int leg_wiggle = ((int)(current_bug.life_timer * 15.0f)) % 2;
        canvas.drawPixel(bx - 3, by - 2 + leg_wiggle, 0x0000);
        canvas.drawPixel(bx + 3, by - 2 - leg_wiggle, 0x0000);
        canvas.drawPixel(bx - 3, by + 2 - leg_wiggle, 0x0000);
        canvas.drawPixel(bx + 3, by + 2 + leg_wiggle, 0x0000);
    } else {
        // 绘制荧光飞虫
        uint16_t glow_col = 0x07E0; // 翠绿荧光
        if (current_bug.glow_intensity > 0.8f) glow_col = 0xFFE0; // 亮黄光

        // 荧光晕圈
        canvas.drawCircle(bx, by, 4, 0x03E0);
        canvas.fillCircle(bx, by, 2, glow_col);

        // 扇翅微动画
        int wing_span = (int)(std::abs(std::sin(current_bug.wing_phase)) * 4.0f) + 1;
        canvas.drawLine(bx - wing_span, by - 2, bx + wing_span, by - 2, 0xFFFF);
    }

    // 若被黏液黏住，绘制包裹的青色半透明黏液微球与挣扎触丝
    if (current_bug.state == BUG_STATE_SLIMED) {
        canvas.drawCircle(bx, by, 6, COLOR_GLOW_CYAN);
        canvas.drawCircle(bx, by, 5, 0x03E0);
        canvas.fillCircle(bx + 1, by - 1, 2, 0xFFFF); // 黏液高光
    }
}
