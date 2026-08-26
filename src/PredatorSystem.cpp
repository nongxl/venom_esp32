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
            p.is_saliva = false;
            p.x = at_x + ((rand() % 6) - 3);
            p.y = at_y + ((rand() % 6) - 3);

            float angle = (rand() % 360) * 0.017453f;
            float spd = (25.0f + (rand() % 45)) * speed_mult;
            p.vx = std::cos(angle) * spd;
            p.vy = std::sin(angle) * spd;

            p.radius = 1.4f + (rand() % 15) * 0.1f;
            p.life = 1.0f;
            p.color = COLOR_VENOM_CORE;
            spawned++;
        }
    }
}

void PredatorSystem::spawnSalivaSpray(float at_x, float at_y, float base_vx, float base_vy, int count) {
    int spawned = 0;
    for (int i = 0; i < MAX_SPLAT_PARTICLES && spawned < count; ++i) {
        if (!splats[i].active) {
            SplatParticle &p = splats[i];
            p.active = true;
            p.is_saliva = true;
            p.x = at_x + ((rand() % 6) - 3);
            p.y = at_y + ((rand() % 6) - 3);

            float spread_x = ((rand() % 80) - 40) * 1.6f;
            float spread_y = ((rand() % 80) - 40) * 1.6f;
            p.vx = base_vx * 0.30f + spread_x;
            p.vy = base_vy * 0.30f + spread_y;

            p.radius = 1.0f + (rand() % 12) * 0.1f;
            p.life = 1.0f;

            int c_roll = rand() % 100;
            if (c_roll < 55) {
                p.color = 0xFFFF; // 纯白高光口水珠
            } else if (c_roll < 80) {
                p.color = 0xF9E7; // 粘稠肉粉口水拉丝
            } else {
                p.color = COLOR_GLOW_CYAN; // 荧光微黏液
            }
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

            // 口水具有自然重力下坠与空气阻尼
            if (p.is_saliva) {
                p.vy += dt * 45.0f;
                p.vx *= 0.91f;
                p.vy *= 0.91f;
                p.life -= dt * 1.8f;
            } else {
                p.vx *= 0.88f;
                p.vy *= 0.88f;
                p.life -= dt * 1.6f;
            }

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

    // 捕食射程判断 (最大有效射程 125px，覆盖屏幕大半腹地)
    if (dist > 125.0f || dist < 6.0f) return false;

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

    // 【三大进食动作智能防重复轮换决策机制】
    // 彻底杜绝单一连续蛛网，大幅提高舌头与触手出镜率 (合占 75%+)
    int roll = rand() % 100;
    if (last_hunt_action == HUNT_MUCUS) {
        // 上次使用了黏液弹 -> 本次 100% 在舌头和触手之间交替！
        hunt.action = (roll < 50) ? HUNT_TONGUE : HUNT_TENTACLE;
    } else if (last_hunt_action == HUNT_TONGUE) {
        // 上次使用了舌头 -> 本次在触手 (60%) 与黏液弹 (40%) 中挑选
        hunt.action = (roll < 60) ? HUNT_TENTACLE : HUNT_MUCUS;
    } else if (last_hunt_action == HUNT_TENTACLE) {
        // 上次使用了触手 -> 本次在长舌 (60%) 与黏液弹 (40%) 中挑选
        hunt.action = (roll < 60) ? HUNT_TONGUE : HUNT_MUCUS;
    } else {
        if (roll < 40) {
            hunt.action = HUNT_TONGUE;      // 40% 变色龙闪电长舌
        } else if (roll < 80) {
            hunt.action = HUNT_TENTACLE;    // 40% 暗黑触手抓取
        } else {
            hunt.action = HUNT_MUCUS;       // 20% 黑色黏液定身
        }
    }
    last_hunt_action = hunt.action;

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
        constexpr float SHOOT_DUR = 0.13f; // 0.13s 闪电射出
        float t = std::min(1.0f, hunt.timer / SHOOT_DUR);
        hunt.progress = t;
        hunt.tip_x = hunt.start_x + (hunt.target_x - hunt.start_x) * t;
        hunt.tip_y = hunt.start_y + (hunt.target_y - hunt.start_y) * t;

        // 闪电弹射过程中沿途拉丝飞溅口水
        hunt.saliva_spray_timer += dt;
        if (hunt.saliva_spray_timer >= 0.035f) {
            hunt.saliva_spray_timer = 0.0f;
            float dir_x = (hunt.target_x - hunt.start_x);
            float dir_y = (hunt.target_y - hunt.start_y);
            spawnSalivaSpray(hunt.tip_x, hunt.tip_y, dir_x * 0.4f, dir_y * 0.4f, 2);
        }

        if (hunt.timer >= SHOOT_DUR) {
            // 舌尖击中猎物瞬间，向四周炸裂喷射 8 颗晶莹口水飞沫！
            spawnSalivaSpray(hunt.target_x, hunt.target_y, 0.0f, -10.0f, 8);
            bugs.catchBug(hunt.target_bug_idx, hunt.tip_x, hunt.tip_y);
            hunt.phase = PHASE_HOLD;
            hunt.timer = 0.0f;
        }
    } else if (hunt.phase == PHASE_HOLD) {
        // 吸盘死死吸牢虫子，舌尖微颤，虫子在吸盘中惊慌挣扎，给予玩家 0.46s 从容观察特写！
        bugs.catchBug(hunt.target_bug_idx, hunt.tip_x, hunt.tip_y);

        hunt.saliva_spray_timer += dt;
        if (hunt.saliva_spray_timer >= 0.12f) {
            hunt.saliva_spray_timer = 0.0f;
            // 抓持期间舌尖滴落口水
            spawnSalivaSpray(hunt.tip_x, hunt.tip_y + 2.0f, 0.0f, 15.0f, 1);
        }

        constexpr float HOLD_DUR = 0.46f; // 0.46s 沉浸观察停顿
        if (hunt.timer >= HOLD_DUR) {
            hunt.phase = PHASE_RETRACT;
            hunt.timer = 0.0f;
        }
    } else if (hunt.phase == PHASE_RETRACT) {
        constexpr float RETRACT_DUR = 0.22f; // 0.22s 强力卷回
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
            // 三指爪盘紧紧抓住虫子，进入 0.28s 抽搐收紧展示阶段！
            bugs.catchBug(hunt.target_bug_idx, hunt.tip_x, hunt.tip_y);
            hunt.phase = PHASE_HOLD;
            hunt.timer = 0.0f;
        }
    } else if (hunt.phase == PHASE_HOLD) {
        bugs.catchBug(hunt.target_bug_idx, hunt.tip_x, hunt.tip_y);
        constexpr float HOLD_DUR = 0.28f;
        if (hunt.timer >= HOLD_DUR) {
            hunt.phase = PHASE_RETRACT;
            hunt.timer = 0.0f;
        }
    } else if (hunt.phase == PHASE_RETRACT) {
        constexpr float RETRACT_DUR = 0.28f; // 0.28s 强力拖回核心
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
        // 确保进食目标点严格内收在屏幕安全舒适区 (至少距边界 20px)
        float target_pos_x = std::max(20.0f, std::min((float)SCREEN_W - 20.0f, b.x));
        float target_pos_y = std::max(20.0f, std::min((float)SCREEN_H - 20.0f, b.y));

        float dx = target_pos_x - hx;
        float dy = target_pos_y - hy;
        float dist = std::sqrt(dx * dx + dy * dy);

        skeleton.setPullTarget(target_pos_x, target_pos_y, 1.85f);

        // 头部靠近 26px (此时巨大黑色标量场肉身已完全将虫子包覆) 或 1.8s 超时看门狗触发立即吞噬完成
        if (dist < 26.0f || hunt.timer > 1.8f) {
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

    float sx = hunt.start_x;
    float sy = hunt.start_y;
    float tx = hunt.tip_x;
    float ty = hunt.tip_y;

    float dx = tx - sx;
    float dy = ty - sy;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) return;

    float ux = dx / len;
    float uy = dy / len;
    float nx = -uy;
    float ny = ux;

    // 【1. 根部粗、尖端细的长三角形肌肉舌身 (带中间贯穿深色凹条与肉粉高光脊)】
    // 根部半厚度 5.5px (全宽 11.0px 雄壮肉舌基底) -> 尖端半厚度 1.8px (全宽 3.6px 收尖锐角)
    float base_half_w = 5.5f;
    float tip_half_w  = 1.8f;

    for (float s = 0.0f; s <= len; s += 1.0f) {
        float p = s / len; // 0.0 (根部) -> 1.0 (尖端)
        float cur_half_w = base_half_w * (1.0f - p) + tip_half_w * p;
        float cx = sx + ux * s;
        float cy = sy + uy * s;

        int i_half_w = (int)std::ceil(cur_half_w);
        for (int w = -i_half_w; w <= i_half_w; ++w) {
            float abs_w = std::abs((float)w);
            if (abs_w > cur_half_w) continue;

            int px = (int)std::round(cx + nx * w);
            int py = (int)std::round(cy + ny * w);

            // 分层肌肉与中线深凹槽着色：
            if (abs_w <= 0.8f && p < 0.92f) {
                // 正中线深深贯穿的深暗红/紫黑凹槽 (Dorsal Sulcus Groove)
                canvas.drawPixel(px, py, 0x4000); 
            } else if (abs_w <= 1.8f) {
                // 凹槽两侧隆起的鲜肉粉色高光肌脊 (Muscle Ridge Highlight)
                canvas.drawPixel(px, py, 0xF9E7);
            } else if (abs_w <= cur_half_w - 0.9f) {
                // 饱满猩红肌肉主肉身 (Scarlet Flesh)
                canvas.drawPixel(px, py, 0xF800);
            } else {
                // 外缘立体暗红阴影轮廓线 (Flesh Shadow Contour)
                canvas.drawPixel(px, py, 0x8800);
            }
        }
    }

    // 【2. 舌尖强力肉质吸盘圆垫 (半径 5.5px 饱满吸盘 + 凹陷深孔)】
    int itx = (int)std::round(tx);
    int ity = (int)std::round(ty);
    canvas.fillCircle(itx, ity, 5, 0xF800);
    canvas.drawCircle(itx, ity, 6, 0xF9E7);
    canvas.fillCircle(itx, ity, 2, 0x4000); // 吸盘中心深孔
    canvas.drawPixel(itx - 1, ity - 1, 0xFFFF); // 粘液湿润反光高光点
}

void PredatorSystem::drawGrabTentacle(M5Canvas &canvas) const {
    if (!hunt.active || hunt.action != HUNT_TENTACLE) return;

    int sx = (int)hunt.start_x;
    int sy = (int)hunt.start_y;
    int tx = (int)hunt.tip_x;
    int ty = (int)hunt.tip_y;

    // 加粗一倍的粗壮黑色抓取触手 (7px 宽粗壮肉柱)
    for (int off = -3; off <= 3; ++off) {
        canvas.drawLine(sx + off, sy, tx + off, ty, COLOR_VENOM_CORE);
        canvas.drawLine(sx, sy + off, tx, ty + off, COLOR_VENOM_CORE);
    }

    // 尖端加粗加长的强力三指爪盘
    float dx = tx - sx;
    float dy = ty - sy;
    float main_angle = std::atan2(dy, dx);
    for (int f = -1; f <= 1; ++f) {
        float fa = main_angle + (float)f * 0.48f;
        float fx = hunt.tip_x + std::cos(fa) * 9.5f;
        float fy = hunt.tip_y + std::sin(fa) * 9.5f;
        for (int off = -1; off <= 1; ++off) {
            canvas.drawLine(tx + off, ty, (int)fx + off, (int)fy, COLOR_VENOM_CORE);
            canvas.drawLine(tx, ty + off, (int)fx, (int)fy + off, COLOR_VENOM_CORE);
        }
        canvas.fillCircle((int)fx, (int)fy, 1, COLOR_GLOW_CYAN);
    }
}

void PredatorSystem::drawMucusShot(M5Canvas &canvas) const {
    // 1. 绘制所有飞行中的溅射黑色液滴与晶莹口水珠
    for (int i = 0; i < MAX_SPLAT_PARTICLES; ++i) {
        if (splats[i].active) {
            int px = (int)splats[i].x;
            int py = (int)splats[i].y;
            int r = (int)std::round(splats[i].radius);
            if (splats[i].is_saliva) {
                // 晶莹口水微粒 (高亮白/肉粉/青光)
                canvas.fillCircle(px, py, std::max(1, r), splats[i].color);
                if (r >= 2) {
                    canvas.drawPixel(px - 1, py - 1, 0xFFFF); // 口水高光反光
                }
            } else {
                // 纯黑黏液滴
                canvas.fillCircle(px, py, r, COLOR_VENOM_CORE);
                canvas.drawPixel(px, py, COLOR_DITHER_GRAY);
            }
        }
    }

    // 2. 绘制飞行中的纯黑黏液母弹
    if (hunt.active && hunt.action == HUNT_MUCUS && hunt.phase == PHASE_SHOOT) {
        int mx = (int)hunt.mucus_x;
        int my = (int)hunt.mucus_y;

        canvas.fillCircle(mx, my, 4, COLOR_VENOM_CORE);
        float dir_len = std::sqrt(hunt.mucus_vx * hunt.mucus_vx + hunt.mucus_vy * hunt.mucus_vy);
        if (dir_len > 0.1f) {
            float tail_off_x = (hunt.mucus_vx / dir_len) * 3.5f;
            float tail_off_y = (hunt.mucus_vy / dir_len) * 3.5f;
            canvas.fillCircle((int)(mx - tail_off_x), (int)(my - tail_off_y), 3, COLOR_VENOM_CORE);
        }
        canvas.drawPixel(mx, my, COLOR_DITHER_GRAY);
    }
}

void PredatorSystem::draw(M5Canvas &canvas) const {
    drawTongue(canvas);
    drawGrabTentacle(canvas);
    drawMucusShot(canvas);
}
