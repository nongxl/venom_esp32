#include "PredatorSystem.h"
#include <cmath>

PredatorSystem::PredatorSystem() {
    hunt.active = false;
    hunt.action = HUNT_NONE;
    hunt.phase = PHASE_IDLE;
    for (int i = 0; i < MAX_SPLAT_PARTICLES; ++i) {
        splats[i].active = false;
    }
}

void PredatorSystem::init() {
    hunt.active = false;
    hunt.action = HUNT_NONE;
    hunt.phase = PHASE_IDLE;
    hunt_decision_cooldown = 0.5f;
    for (int i = 0; i < MAX_SPLAT_PARTICLES; ++i) {
        splats[i].active = false;
    }
}

void PredatorSystem::spawnSplatBurst(float at_x, float at_y, int count, float speed_mult) {
    int spawned = 0;
    for (int i = 0; i < MAX_SPLAT_PARTICLES && spawned < count; ++i) {
        if (!splats[i].active) {
            SplatParticle &p = splats[i];
            p.active = true;
            p.x = at_x + ((rand() % 6) - 3);
            p.y = at_y + ((rand() % 6) - 3);

            float angle = (rand() % 360) * 0.017453f;
            float spd = (25.0f + (rand() % 45)) * speed_mult;
            p.vx = std::cos(angle) * spd;
            p.vy = std::sin(angle) * spd;

            p.radius = 1.4f + (rand() % 15) * 0.1f;
            p.life = 1.0f;
            spawned++;
        }
    }
}

void PredatorSystem::updateSplatParticles(float dt) {
    for (int i = 0; i < MAX_SPLAT_PARTICLES; ++i) {
        if (splats[i].active) {
            SplatParticle &p = splats[i];
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.vx *= 0.88f;
            p.vy *= 0.88f;
            p.life -= dt * 1.6f;

            if (p.life <= 0.0f) {
                p.active = false;
            }
        }
    }
}

bool PredatorSystem::tryTriggerHunt(PreyBugSystem &bugs, const SkeletonSystem &skeleton) {
    if (hunt.active) return false;

    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    float bx, by;
    BugState state;
    int bug_idx = bugs.getNearestBug(hx, hy, bx, by, state);
    if (bug_idx < 0 || state == BUG_DEAD || state == BUG_CAUGHT) return false;

    float dx = bx - hx;
    float dy = by - hy;
    float dist = std::sqrt(dx * dx + dy * dy);

    // 捕食射程判断 (最大有效射程 95px)
    if (dist > 95.0f || dist < 6.0f) return false;

    hunt.active = true;
    hunt.target_bug_idx = bug_idx;
    hunt.timer = 0.0f;
    hunt.start_x = hx;
    hunt.start_y = hy;
    hunt.target_x = bx;
    hunt.target_y = by;
    hunt.tip_x = hx;
    hunt.tip_y = hy;
    hunt.progress = 0.0f;
    hunt.trail_spawn_timer = 0.0f;

    // 动作决策权重分布
    int roll = rand() % 100;
    if (dist < 60.0f) {
        if (roll < 45) {
            hunt.action = HUNT_TONGUE;      // 变色龙闪电弹舌
        } else if (roll < 80) {
            hunt.action = HUNT_TENTACLE;    // 暗黑触手抓取
        } else {
            hunt.action = HUNT_MUCUS;       // 黑色黏液定身爬行
        }
    } else {
        if (roll < 55) {
            hunt.action = HUNT_MUCUS;       // 远距离喷射黑色黏液弹
        } else if (roll < 80) {
            hunt.action = HUNT_TONGUE;      // 极限长舌弹射
        } else {
            hunt.action = HUNT_TENTACLE;    // 远距触手
        }
    }

    hunt.phase = PHASE_SHOOT;

    if (hunt.action == HUNT_MUCUS) {
        hunt.mucus_x = hx;
        hunt.mucus_y = hy;
        float dir_x = dx / dist;
        float dir_y = dy / dist;
        hunt.mucus_vx = dir_x * 420.0f; // 极速破空狙击弹丸 (0.08~0.15s 瞬间破空命中！)
        hunt.mucus_vy = dir_y * 420.0f;

        // 喷射瞬间枪口初速度溅射
        spawnSplatBurst(hx, hy, 4, 0.8f);
    }

    return true;
}

void PredatorSystem::finishDigest(PreyBugSystem &bugs, PhysiologySystem &physiology, MetaballSystem &metaballs) {
    if (hunt.target_bug_idx >= 0) {
        bugs.killBug(hunt.target_bug_idx);
    }

    // 吞噬消化反馈：能量补充 (+0.30)
    physiology.feed(0.30f);

    // 头部飞溅 3 颗兴奋微液滴
    for (int k = 0; k < 3; ++k) {
        float vx = ((rand() % 60) - 30) * 0.08f;
        float vy = ((rand() % 60) - 30) * 0.08f;
        metaballs.spawnDroplet(hunt.start_x + vx * 2.0f, hunt.start_y + vy * 2.0f, vx, vy, 2.2f, true);
    }

    hunt.active = false;
    hunt.action = HUNT_NONE;
    hunt.phase = PHASE_IDLE;
    // 吞噬完成进入 22 ~ 40 秒饱腹期，不再连续抓虫！
    hunt_decision_cooldown = 22.0f + (rand() % 180) * 0.1f;
}

void PredatorSystem::updateTongueStrike(float dt, PreyBugSystem &bugs, SkeletonSystem &skeleton,
                                        PhysiologySystem &physiology, MetaballSystem &metaballs) {
    hunt.timer += dt;
    float hx, hy;
    skeleton.getHeadPos(hx, hy);
    hunt.start_x = hx;
    hunt.start_y = hy;

    const PreyBug &b = bugs.getBug(hunt.target_bug_idx);
    if (b.active && b.state != BUG_DEAD && hunt.phase == PHASE_SHOOT) {
        hunt.target_x = b.x;
        hunt.target_y = b.y;
    }

    if (hunt.phase == PHASE_SHOOT) {
        constexpr float SHOOT_DUR = 0.11f; // 0.11s 闪电射出
        float t = std::min(1.0f, hunt.timer / SHOOT_DUR);
        hunt.progress = t;
        hunt.tip_x = hunt.start_x + (hunt.target_x - hunt.start_x) * t;
        hunt.tip_y = hunt.start_y + (hunt.target_y - hunt.start_y) * t;

        if (hunt.timer >= SHOOT_DUR) {
            // 舌尖黏中虫子，进入卷回阶段
            bugs.catchBug(hunt.target_bug_idx, hunt.tip_x, hunt.tip_y);
            hunt.phase = PHASE_RETRACT;
            hunt.timer = 0.0f;
        }
    } else if (hunt.phase == PHASE_RETRACT) {
        constexpr float RETRACT_DUR = 0.13f; // 0.13s 极速卷回
        float t = std::min(1.0f, hunt.timer / RETRACT_DUR);
        hunt.progress = 1.0f - t;
        hunt.tip_x = hunt.target_x + (hunt.start_x - hunt.target_x) * t;
        hunt.tip_y = hunt.target_y + (hunt.start_y - hunt.target_y) * t;

        bugs.catchBug(hunt.target_bug_idx, hunt.tip_x, hunt.tip_y);

        if (hunt.timer >= RETRACT_DUR) {
            finishDigest(bugs, physiology, metaballs);
        }
    }
}

void PredatorSystem::updateTentacleGrab(float dt, PreyBugSystem &bugs, SkeletonSystem &skeleton,
                                        PhysiologySystem &physiology, MetaballSystem &metaballs) {
    hunt.timer += dt;
    float hx, hy;
    skeleton.getHeadPos(hx, hy);
    hunt.start_x = hx;
    hunt.start_y = hy;

    const PreyBug &b = bugs.getBug(hunt.target_bug_idx);
    if (b.active && b.state != BUG_DEAD && hunt.phase == PHASE_SHOOT) {
        hunt.target_x = b.x;
        hunt.target_y = b.y;
    }

    if (hunt.phase == PHASE_SHOOT) {
        constexpr float SHOOT_DUR = 0.18f;
        float t = std::min(1.0f, hunt.timer / SHOOT_DUR);
        hunt.progress = t;
        hunt.tip_x = hunt.start_x + (hunt.target_x - hunt.start_x) * t;
        hunt.tip_y = hunt.start_y + (hunt.target_y - hunt.start_y) * t;

        if (hunt.timer >= SHOOT_DUR) {
            bugs.catchBug(hunt.target_bug_idx, hunt.tip_x, hunt.tip_y);
            hunt.phase = PHASE_RETRACT;
            hunt.timer = 0.0f;
        }
    } else if (hunt.phase == PHASE_RETRACT) {
        constexpr float RETRACT_DUR = 0.20f;
        float t = std::min(1.0f, hunt.timer / RETRACT_DUR);
        hunt.progress = 1.0f - t;
        hunt.tip_x = hunt.target_x + (hunt.start_x - hunt.target_x) * t;
        hunt.tip_y = hunt.target_y + (hunt.start_y - hunt.target_y) * t;

        bugs.catchBug(hunt.target_bug_idx, hunt.tip_x, hunt.tip_y);

        if (hunt.timer >= RETRACT_DUR) {
            finishDigest(bugs, physiology, metaballs);
        }
    }
}

void PredatorSystem::updateMucusSnare(float dt, PreyBugSystem &bugs, SkeletonSystem &skeleton,
                                      PhysiologySystem &physiology, MetaballSystem &metaballs) {
    hunt.timer += dt;
    float hx, hy;
    skeleton.getHeadPos(hx, hy);
    hunt.start_x = hx;
    hunt.start_y = hy;

    if (hunt.phase == PHASE_SHOOT) {
        // 极速纯黑黏液弹飞行 (420px/s)
        hunt.mucus_x += hunt.mucus_vx * dt;
        hunt.mucus_y += hunt.mucus_vy * dt;

        // 飞行后抛黑色尾迹液滴
        hunt.trail_spawn_timer += dt;
        if (hunt.trail_spawn_timer >= 0.025f) {
            hunt.trail_spawn_timer = 0.0f;
            for (int i = 0; i < MAX_SPLAT_PARTICLES; ++i) {
                if (!splats[i].active) {
                    splats[i].active = true;
                    splats[i].x = hunt.mucus_x + ((rand() % 4) - 2);
                    splats[i].y = hunt.mucus_y + ((rand() % 4) - 2);
                    splats[i].vx = -hunt.mucus_vx * 0.12f + ((rand() % 20) - 10);
                    splats[i].vy = -hunt.mucus_vy * 0.12f + ((rand() % 20) - 10);
                    splats[i].radius = 1.2f;
                    splats[i].life = 0.4f;
                    break;
                }
            }
        }

        float dx = hunt.target_x - hunt.mucus_x;
        float dy = hunt.target_y - hunt.mucus_y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 10.0f || hunt.timer > 0.28f) {
            // 命中虫子！爆浆溅射出 8 颗黑色黏液滴，将虫子死死定身！
            spawnSplatBurst(hunt.target_x, hunt.target_y, 8, 1.2f);
            bugs.snareBug(hunt.target_bug_idx);

            // 【关键重构】：不立刻过去吃，先进入 1.2~1.8s 原地戏谑观察阶段！
            hunt.phase = PHASE_STALK_OBSERVE;
            hunt.timer = 0.0f;
            hunt.observe_duration = 1.3f + (rand() % 6) * 0.1f;
            skeleton.clearPullTarget();
        }
    } else if (hunt.phase == PHASE_STALK_OBSERVE) {
        // 【观察戏谑阶段】：毒液在原地静止并微调呼吸，眼睛死死盯住挣扎的虫子
        skeleton.clearPullTarget();

        if (hunt.timer >= hunt.observe_duration) {
            hunt.phase = PHASE_CRAWL_ENGULF;
            hunt.timer = 0.0f;
        }
    } else if (hunt.phase == PHASE_CRAWL_ENGULF) {
        // 【从容爬行包覆吞噬阶段】
        const PreyBug &b = bugs.getBug(hunt.target_bug_idx);
        float target_pos_x = b.x;
        float target_pos_y = b.y;

        // 强力引导毒液头部向定身虫子爬去
        float dx = target_pos_x - hx;
        float dy = target_pos_y - hy;
        float dist = std::sqrt(dx * dx + dy * dy);

        skeleton.setPullTarget(target_pos_x, target_pos_y, 2.0f);

        if (dist < 12.0f || hunt.timer > 2.5f) {
            skeleton.clearPullTarget();
            finishDigest(bugs, physiology, metaballs);
        }
    }
}

void PredatorSystem::update(float dt, PreyBugSystem &bugs, SkeletonSystem &skeleton,
                            PhysiologySystem &physiology, MetaballSystem &metaballs) {
    // 更新所有飞溅微粒物理
    updateSplatParticles(dt);

    if (!hunt.active) {
        hunt_decision_cooldown -= dt;
        if (hunt_decision_cooldown <= 0.0f) {
            hunt_decision_cooldown = 0.4f;
            tryTriggerHunt(bugs, skeleton);
        }
        return;
    }

    switch (hunt.action) {
        case HUNT_TONGUE:
            updateTongueStrike(dt, bugs, skeleton, physiology, metaballs);
            break;
        case HUNT_TENTACLE:
            updateTentacleGrab(dt, bugs, skeleton, physiology, metaballs);
            break;
        case HUNT_MUCUS:
            updateMucusSnare(dt, bugs, skeleton, physiology, metaballs);
            break;
        default:
            hunt.active = false;
            break;
    }
}

void PredatorSystem::drawTongue(M5Canvas &canvas) const {
    if (!hunt.active || hunt.action != HUNT_TONGUE) return;

    int sx = (int)hunt.start_x;
    int sy = (int)hunt.start_y;
    int tx = (int)hunt.tip_x;
    int ty = (int)hunt.tip_y;

    // 1. 绘制猩红肉质舌身 (带粗细与高光)
    canvas.drawLine(sx, sy, tx, ty, 0xF800);     // 猩红
    canvas.drawLine(sx + 1, sy + 1, tx, ty, 0xF9E7); // 肉粉色高光
    canvas.drawLine(sx, sy + 1, tx, ty + 1, 0xF800);

    // 2. 绘制舌尖肉质吸盘圆垫
    canvas.fillCircle(tx, ty, 3, 0xF800);
    canvas.drawCircle(tx, ty, 4, 0xF9E7);
    canvas.drawPixel(tx, ty, 0xFFFF);
}

void PredatorSystem::drawGrabTentacle(M5Canvas &canvas) const {
    if (!hunt.active || hunt.action != HUNT_TENTACLE) return;

    int sx = (int)hunt.start_x;
    int sy = (int)hunt.start_y;
    int tx = (int)hunt.tip_x;
    int ty = (int)hunt.tip_y;

    // 粗壮黑色抓取触手
    for (int off = -1; off <= 1; ++off) {
        canvas.drawLine(sx + off, sy, tx + off, ty, COLOR_VENOM_CORE);
        canvas.drawLine(sx, sy + off, tx, ty + off, COLOR_VENOM_CORE);
    }

    // 尖端三指爪盘抓牢
    float dx = tx - sx;
    float dy = ty - sy;
    float main_angle = std::atan2(dy, dx);
    for (int f = -1; f <= 1; ++f) {
        float fa = main_angle + (float)f * 0.5f;
        float fx = hunt.tip_x + std::cos(fa) * 5.0f;
        float fy = hunt.tip_y + std::sin(fa) * 5.0f;
        canvas.drawLine(tx, ty, (int)fx, (int)fy, COLOR_VENOM_CORE);
        canvas.drawPixel((int)fx, (int)fy, COLOR_GLOW_CYAN);
    }
}

void PredatorSystem::drawMucusShot(M5Canvas &canvas) const {
    // 1. 绘制所有飞行中的溅射黑色液滴
    for (int i = 0; i < MAX_SPLAT_PARTICLES; ++i) {
        if (splats[i].active) {
            int px = (int)splats[i].x;
            int py = (int)splats[i].y;
            int r = (int)std::round(splats[i].radius);
            canvas.fillCircle(px, py, r, COLOR_VENOM_CORE);
            canvas.drawPixel(px, py, COLOR_DITHER_GRAY); // 沥青微高光
        }
    }

    // 2. 绘制飞行中的纯黑黏液母弹
    if (hunt.active && hunt.action == HUNT_MUCUS && hunt.phase == PHASE_SHOOT) {
        int mx = (int)hunt.mucus_x;
        int my = (int)hunt.mucus_y;

        // 纯黑大液滴主体
        canvas.fillCircle(mx, my, 4, COLOR_VENOM_CORE);
        // 拉长形变拖尾黑滴
        float dir_len = std::sqrt(hunt.mucus_vx * hunt.mucus_vx + hunt.mucus_vy * hunt.mucus_vy);
        if (dir_len > 0.1f) {
            float tail_off_x = (hunt.mucus_vx / dir_len) * 3.5f;
            float tail_off_y = (hunt.mucus_vy / dir_len) * 3.5f;
            canvas.fillCircle((int)(mx - tail_off_x), (int)(my - tail_off_y), 3, COLOR_VENOM_CORE);
        }
        // 表面沥青微高光
        canvas.drawPixel(mx, my, COLOR_DITHER_GRAY);
    }
}

void PredatorSystem::draw(M5Canvas &canvas) const {
    drawTongue(canvas);
    drawGrabTentacle(canvas);
    drawMucusShot(canvas);
}
