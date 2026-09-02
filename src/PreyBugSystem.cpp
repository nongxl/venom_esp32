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
    spawn_cooldown = 45.0f;
}

void PreyBugSystem::spawnNewBug() {
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (!bugs[i].active) {
            PreyBug &b = bugs[i];
            b.active = true;
            b.state = BUG_FREE;
            b.type = (rand() % 2 == 0) ? BUG_CRAWLER : BUG_FLYER;

            // 随机从屏幕安全视窗边缘诞生 (内收 18px，保证毒液进食时绝不出屏)
            int side = rand() % 4;
            if (side == 0) { b.x = 20.0f + (rand() % (SCREEN_W - 40)); b.y = 18.0f; b.angle = 1.57f; }
            else if (side == 1) { b.x = SCREEN_W - 18.0f; b.y = 20.0f + (rand() % (SCREEN_H - 40)); b.angle = 3.14f; }
            else if (side == 2) { b.x = 20.0f + (rand() % (SCREEN_W - 40)); b.y = SCREEN_H - 18.0f; b.angle = -1.57f; }
            else { b.x = 18.0f; b.y = 20.0f + (rand() % (SCREEN_H - 40)); b.angle = 0.0f; }

            b.base_speed = (b.type == BUG_CRAWLER) ? (14.0f + (rand() % 10)) : (22.0f + (rand() % 16));
            b.current_speed = b.base_speed;
            b.leg_phase = 0.0f;
            b.wing_phase = 0.0f;
            b.speed_change_timer = 1.0f + (rand() % 15) * 0.1f;
            b.state_timer = 0.0f;
            b.glow_phase = (rand() % 100) * 0.1f;
            b.snare_timer = 0.0f;
            b.lifespan = 0.0f;
            b.max_lifespan = 24.0f + (rand() % 160) * 0.1f; // 24~40 秒自然逗留后逃逸离场
            b.is_escaping = false;
            b.caught_watchdog = 0.0f;
            break;
        }
    }
}

void PreyBugSystem::updateCrawler(PreyBug &b, float dt, float v_hx, float v_hy) {
    b.state_timer += dt;
    b.glow_phase += dt * 3.0f;
    b.lifespan += dt;

    if (b.lifespan >= b.max_lifespan) {
        b.is_escaping = true;
    }

    if (b.state == BUG_SNARED) {
        b.snare_timer += dt;
        b.leg_phase += dt * 25.0f; // 拼命挣扎抖腿
        // 蛛网时效到期：虫子挣脱蛛网定身并加速逃离屏幕！
        if (b.snare_timer >= b.snare_duration) {
            b.state = BUG_FREE;
            b.snare_timer = 0.0f;
            b.is_escaping = true;
            b.current_speed = b.base_speed * 2.0f;
        }
        return;
    }
    if (b.state == BUG_CAUGHT) {
        b.caught_watchdog += dt;
        if (b.caught_watchdog > 2.2f) {
            // 抓取超时防死锁看门狗：超过 2.2 秒未消化完成，自动销毁
            b.active = false;
            b.state = BUG_DEAD;
        }
        return;
    }
    b.caught_watchdog = 0.0f;

    // 1. 敏捷危险感知与极速逃逸闪避：如果毒液靠近或飞扑，立即惊慌蛇形逃逸！
    float dx = b.x - v_hx;
    float dy = b.y - v_hy;
    float dist_v = std::sqrt(dx * dx + dy * dy);
    if (dist_v < 55.0f && dist_v > 0.1f) {
        float flee_angle = std::atan2(dy, dx);
        b.angle = flee_angle + std::sin(b.state_timer * 16.0f) * 0.40f;
        if (dist_v < 38.0f) {
            // 毒液极近距离逼近或身体飞扑：极速蛇形暴走闪避 (2.6x 速度)
            b.current_speed = b.base_speed * 2.6f;
            b.leg_phase += dt * 38.0f;
        } else {
            b.current_speed = b.base_speed * 1.85f;
            b.leg_phase += dt * 25.0f;
        }
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

    // 4. 边界碰撞掉头 (逃逸模式下直接离开屏幕销毁)
    if (b.is_escaping) {
        if (b.x < -10.0f || b.x > (float)SCREEN_W + 10.0f || b.y < -10.0f || b.y > (float)SCREEN_H + 10.0f) {
            b.active = false;
            b.state = BUG_DEAD;
            return;
        }
    } else {
        constexpr float MARGIN = 18.0f;
        if (b.x < MARGIN) { b.x = MARGIN; b.angle = 3.14159f - b.angle; }
        if (b.x > SCREEN_W - MARGIN) { b.x = SCREEN_W - MARGIN; b.angle = 3.14159f - b.angle; }
        if (b.y < MARGIN) { b.y = MARGIN; b.angle = -b.angle; }
        if (b.y > SCREEN_H - MARGIN) { b.y = SCREEN_H - MARGIN; b.angle = -b.angle; }
    }
}

void PreyBugSystem::updateFlyer(PreyBug &b, float dt, float v_hx, float v_hy) {
    b.state_timer += dt;
    b.wing_phase += dt * 45.0f; // 高频振翅
    b.glow_phase += dt * 4.0f;
    b.lifespan += dt;

    if (b.lifespan >= b.max_lifespan) {
        b.is_escaping = true;
    }

    if (b.state == BUG_SNARED) {
        b.snare_timer += dt;
        // 蛛网时效到期：飞虫挣脱蛛网并加速飞出屏幕
        if (b.snare_timer >= b.snare_duration) {
            b.state = BUG_FREE;
            b.snare_timer = 0.0f;
            b.is_escaping = true;
            b.current_speed = b.base_speed * 2.0f;
        }
        return;
    }
    if (b.state == BUG_CAUGHT) {
        b.caught_watchdog += dt;
        if (b.caught_watchdog > 2.2f) {
            b.active = false;
            b.state = BUG_DEAD;
        }
        return;
    }
    b.caught_watchdog = 0.0f;

    // 1. 飞行敏捷预警逃逸闪避
    float dx = b.x - v_hx;
    float dy = b.y - v_hy;
    float dist_v = std::sqrt(dx * dx + dy * dy);
    if (dist_v < 62.0f && dist_v > 0.1f) {
        float flee_angle = std::atan2(dy, dx);
        b.angle = flee_angle + std::sin(b.state_timer * 20.0f) * 0.45f;
        if (dist_v < 42.0f) {
            b.current_speed = b.base_speed * 2.5f; // 极速急转弯闪避
            b.wing_phase += dt * 70.0f;
        } else {
            b.current_speed = b.base_speed * 1.8f;
            b.wing_phase += dt * 50.0f;
        }
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

    if (b.is_escaping) {
        if (b.x < -10.0f || b.x > (float)SCREEN_W + 10.0f || b.y < -10.0f || b.y > (float)SCREEN_H + 10.0f) {
            b.active = false;
            b.state = BUG_DEAD;
            return;
        }
    } else {
        constexpr float MARGIN = 18.0f;
        if (b.x < MARGIN) { b.x = MARGIN; b.angle = 3.14159f - b.angle; }
        if (b.x > SCREEN_W - MARGIN) { b.x = SCREEN_W - MARGIN; b.angle = 3.14159f - b.angle; }
        if (b.y < MARGIN) { b.y = MARGIN; b.angle = -b.angle; }
        if (b.y > SCREEN_H - MARGIN) { b.y = SCREEN_H - MARGIN; b.angle = -b.angle; }
    }
}

void PreyBugSystem::update(float dt, float venom_hx, float venom_hy) {
    int active_count = 0;
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state != BUG_DEAD) {
            active_count++;
            if (bugs[i].type == BUG_CRAWLER) {
                updateCrawler(bugs[i], dt, venom_hx, venom_hy);
            } else {
                updateFlyer(bugs[i], dt, venom_hx, venom_hy);
            }
        }
    }

    // 多虫动态生态刷新：显著降低飞虫出现频率 (60~100s 稀有偶尔刷出一只)
    if (active_count < 2) {
        spawn_cooldown -= dt;
        if (spawn_cooldown <= 0.0f) {
            spawnNewBug();
            spawn_cooldown = (active_count == 0) ? (60.0f + (rand() % 400) * 0.1f) : (120.0f + (rand() % 600) * 0.1f);
        }
    }
}

int PreyBugSystem::getActiveBugCount() const {
    int count = 0;
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state != BUG_DEAD) count++;
    }
    return count;
}

void PreyBugSystem::spawnBugImmediate() {
    spawnNewBug();
}

int PreyBugSystem::getNearestBug(float x, float y, float &bug_x, float &bug_y, BugState &state, bool prefer_snared) const {
    int best_idx = -1;
    float min_dist = 99999.0f;

    // 优先寻找被蛛网定身的储备粮
    if (prefer_snared) {
        for (int i = 0; i < MAX_BUGS; ++i) {
            if (bugs[i].active && bugs[i].state == BUG_SNARED) {
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
        if (best_idx >= 0) return best_idx;
    }

    // 寻找最近的任意可用小虫
    min_dist = 99999.0f;
    for (int i = 0; i < MAX_BUGS; ++i) {
        if (bugs[i].active && bugs[i].state != BUG_DEAD && bugs[i].state != BUG_CAUGHT) {
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

void PreyBugSystem::releaseBug(int idx) {
    if (idx >= 0 && idx < MAX_BUGS && bugs[idx].active) {
        bugs[idx].state = BUG_FREE;
        bugs[idx].is_escaping = true; // 惊慌脱困，快速逃离屏幕
        bugs[idx].current_speed = bugs[idx].base_speed * 2.2f;
        bugs[idx].snare_timer = 0.0f;
        bugs[idx].caught_watchdog = 0.0f;
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

    // 4. 镂空不规则共生体蛛网定身特效 (Hollow Spiderweb)
    if (b.state == BUG_SNARED) {
        drawHollowSpiderWeb(canvas, b);
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

    // 4. 镂空不规则共生体蛛网定身特效 (Hollow Spiderweb)
    if (b.state == BUG_SNARED) {
        drawHollowSpiderWeb(canvas, b);
    }
}

void PreyBugSystem::drawHollowSpiderWeb(M5Canvas &canvas, const PreyBug &b) const {
    int cx = (int)b.x;
    int cy = (int)b.y;

    // 6 根不规则放射蛛丝的天然偏角与长度
    static const float BASE_ANGLES[6] = { 0.28f, 1.25f, 2.22f, 3.42f, 4.45f, 5.50f };
    static const float STRUT_LENS[6]  = { 11.5f,  9.0f, 12.0f, 10.5f, 12.5f,  9.5f };

    float strut_x[6];
    float strut_y[6];
    float mid_x[6];
    float mid_y[6];

    // 挣扎微颤
    float struggle_vib = std::sin(b.snare_timer * 36.0f) * 0.6f;

    // 1. 计算 6 个末端黏附锚点与中部内环丝节点
    for (int k = 0; k < 6; ++k) {
        float ang = BASE_ANGLES[k] + struggle_vib * 0.06f;
        float r_len = STRUT_LENS[k] + ((k % 2 == 0) ? 1.0f : -0.5f);

        strut_x[k] = b.x + std::cos(ang) * r_len;
        strut_y[k] = b.y + std::sin(ang) * r_len;

        float mid_len = 5.2f + ((k % 3 == 0) ? 0.8f : -0.5f);
        mid_x[k] = b.x + std::cos(ang) * mid_len;
        mid_y[k] = b.y + std::sin(ang) * mid_len;
    }

    // 2. 绘制 6 根放射状黑色主蛛丝 (从虫身辐射向四周)
    for (int k = 0; k < 6; ++k) {
        canvas.drawLine(cx, cy, (int)strut_x[k], (int)strut_y[k], COLOR_VENOM_CORE);
    }

    // 3. 绘制内层与外层镂空环状连接蛛丝 (Web Rings)
    for (int k = 0; k < 6; ++k) {
        int next_k = (k + 1) % 6;

        // 内环镂空连接丝
        canvas.drawLine((int)mid_x[k], (int)mid_y[k], (int)mid_x[next_k], (int)mid_y[next_k], COLOR_VENOM_CORE);

        // 外环不规则连接丝
        if (k % 2 == 0) {
            float out_x1 = b.x + (strut_x[k] - b.x) * 0.82f;
            float out_y1 = b.y + (strut_y[k] - b.y) * 0.82f;
            float out_x2 = b.x + (strut_x[next_k] - b.x) * 0.82f;
            float out_y2 = b.y + (strut_y[next_k] - b.y) * 0.82f;
            canvas.drawLine((int)out_x1, (int)out_y1, (int)out_x2, (int)out_y2, COLOR_VENOM_CORE);
        }
    }

    // 4. 绘制黏在屏幕玻璃平面上的接触吸点 (Anchor Contact Pads)
    for (int k = 0; k < 6; ++k) {
        int px = (int)strut_x[k];
        int py = (int)strut_y[k];
        canvas.drawPixel(px, py, COLOR_VENOM_CORE);
        canvas.drawPixel(px + 1, py, COLOR_VENOM_CORE);
        canvas.drawPixel(px, py + 1, COLOR_VENOM_CORE);
        canvas.drawPixel(px - 1, py - 1, COLOR_DITHER_GRAY); // 玻璃反光微点
    }

    // 5. 虫子身体上的 2 根微小缠绕黑丝 (中心留出大片镂空空间供细腿/小翅膀挣扎)
    float perp_x = -std::sin(b.angle);
    float perp_y = std::cos(b.angle);
    canvas.drawLine(cx, cy, (int)(b.x + perp_x * 2.8f), (int)(b.y + perp_y * 2.8f), COLOR_VENOM_CORE);
    canvas.drawLine(cx, cy, (int)(b.x - perp_x * 2.8f), (int)(b.y - perp_y * 2.8f), COLOR_VENOM_CORE);
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
