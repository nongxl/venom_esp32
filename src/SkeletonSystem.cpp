#include "SkeletonSystem.h"
#include <cmath>
#include <algorithm>

SkeletonSystem::SkeletonSystem() {
    init();
}

void SkeletonSystem::init() {
    float start_x = 130.0f;
    float start_y = 118.0f;

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        nodes[i].x = start_x - i * 13.0f;
        nodes[i].y = start_y;
        nodes[i].vx = 0.0f;
        nodes[i].vy = 0.0f;

        if (i == 0)      nodes[i].base_radius = 22.0f;
        else if (i == 1) nodes[i].base_radius = 19.0f;
        else if (i == 2) nodes[i].base_radius = 16.0f;
        else if (i == 3) nodes[i].base_radius = 12.0f;
        else             nodes[i].base_radius = 8.5f;

        nodes[i].radius_x = nodes[i].base_radius;
        nodes[i].radius_y = nodes[i].base_radius;
        nodes[i].mass = 0.7f + (float)i * 0.25f;

        nodes[i].contact_bottom = 0.0f;
        nodes[i].contact_top = 0.0f;
        nodes[i].contact_left = 0.0f;
        nodes[i].contact_right = 0.0f;
        nodes[i].bleb_offset_x = 0.0f;
        nodes[i].bleb_offset_y = 0.0f;
    }

    for (int i = 0; i < SKELETON_NODE_COUNT - 1; ++i) {
        rest_lengths[i] = 13.0f;
    }

    has_pull_target = false;
    pull_strength = 0.0f;
    flying_timer = 0.0f;
    sticky_clog_timer = 0.0f;
    impact_occurred = false;
}

void SkeletonSystem::setCreepingTarget(float tx, float ty, float speed) {
    if (flying_timer > 0.0f || is_hanging) return;
    is_creeping_motion = true;
    creep_target_x = std::max(16.0f, std::min((float)SCREEN_W - 16.0f, tx));
    creep_target_y = std::max(16.0f, std::min((float)SCREEN_H - 16.0f, ty));
    creep_speed_mult = speed;
}

void SkeletonSystem::clearCreepingTarget() {
    is_creeping_motion = false;
}

void SkeletonSystem::setPullTarget(float tx, float ty, float force) {
    has_pull_target = true;
    // 强制将牵引目标点内收到屏幕安全区域内 (距边界至少 14px)
    pull_target_x = std::max(14.0f, std::min((float)SCREEN_W - 14.0f, tx));
    pull_target_y = std::max(14.0f, std::min((float)SCREEN_H - 14.0f, ty));
    pull_strength = force;
    pull_timeout_timer = 0.0f;
}

void SkeletonSystem::clearPullTarget() {
    has_pull_target = false;
    pull_strength = 0.0f;
    pull_timeout_timer = 0.0f;
}

void SkeletonSystem::setHangingAnchor(float ax, float ay, float rope_length) {
    flying_timer = 0.0f;
    is_hanging = true;
    anchor_x = ax;
    anchor_y = ay;
    rope_len = rope_length;
    swing_phase = 0.0f;
    swing_angle = 0.0f;
    swing_ang_vel = 0.0f;
    clearPullTarget();
    clearCreepingTarget();
    Serial.printf("[SKELETON] setHangingAnchor: (%.1f, %.1f) len=%.1f\n", ax, ay, rope_length);
}

void SkeletonSystem::clearHangingAnchor() {
    if (is_hanging) {
        Serial.println("[SKELETON] clearHangingAnchor called!");
    }
    is_hanging = false;
}

void SkeletonSystem::applyImpulse(float ix, float iy) {
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        nodes[i].vx += ix / nodes[i].mass;
        nodes[i].vy += iy / nodes[i].mass;
    }
}

// 激发干脆利落的极速甩飞抛体运动 (彻底挣脱触手抓地力，闪电甩飞横跨屏幕)
void SkeletonSystem::triggerSlingThrow(float dir_x, float dir_y, float speed) {
    flying_timer = 0.65f;        // 0.65s 高速无阻尼飞行冲刺
    launch_grace_timer = 0.18f;  // 刚甩出起飞瞬间 0.18s 内不被原起飞壁阻挡
    is_creeping_motion = false;  // 彻底剥离地表小触手抓力！
    clearCreepingTarget();
    clearPullTarget();           // 立即解除任何正在进行的触手牵引！
    clearHangingAnchor();        // 立即打断蛛丝悬挂！
    is_hanging = false;
    sticky_clog_timer = 0.0f;

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        nodes[i].x += dir_x * 4.0f;
        nodes[i].y += dir_y * 4.0f;
        float lead = (i == 0) ? 1.25f : 1.0f;
        nodes[i].vx = dir_x * speed * lead;
        nodes[i].vy = dir_y * speed * lead;
    }
}

// 闪电猎食猛扑 (Explosive Predatory Pounce) - 紧绷流线型高速暴冲
void SkeletonSystem::triggerPounce(float dir_x, float dir_y, float speed) {
    flying_timer = 0.26f; // 0.26s 高速无阻尼直线破空突进
    clearPullTarget();
    clearHangingAnchor();
    sticky_clog_timer = 0.0f;
    is_creeping_motion = false;

    // 全骨架全节点瞬间获得超高速前向破空速度 (140~175 px/s)
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        float lead = (i == 0) ? 1.22f : ((i == 1) ? 1.10f : 1.0f);
        nodes[i].vx = dir_x * speed * lead;
        nodes[i].vy = dir_y * speed * lead;
    }
}

void SkeletonSystem::triggerLocalBleb(int node_index, float intensity) {
    if (node_index < 0 || node_index >= SKELETON_NODE_COUNT) return;
    float angle = (rand() % 360) * 0.017453f;
    float dist = (4.0f + (rand() % 20) * 0.1f) * intensity;
    nodes[node_index].bleb_offset_x = std::cos(angle) * dist;
    nodes[node_index].bleb_offset_y = std::sin(angle) * dist;
}

bool SkeletonSystem::isAttachedToWall() const {
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        if (nodes[i].contact_bottom > 0.25f || nodes[i].contact_top > 0.25f ||
            nodes[i].contact_left > 0.25f || nodes[i].contact_right > 0.25f) {
            return true;
        }
    }
    return false;
}

void SkeletonSystem::applyWallAdhesion(int i) {
    SkeletonNode &n = nodes[i];
    float r = n.base_radius;

    n.contact_bottom = 0.0f;
    n.contact_top = 0.0f;
    n.contact_left = 0.0f;
    n.contact_right = 0.0f;

    // 荡秋千/倒挂悬挂状态由单摆物理约束接管，不触发贴壁撞击事件
    if (is_hanging) return;

    // 刚甩出起飞的 0.18 秒保护期内，忽略原壁面约束，允许身体全速冲向开阔屏幕！
    if (launch_grace_timer > 0.0f) return;

    // 1. 底部地面撞击
    float dist_b = (SCREEN_H - 1) - n.y;
    if (dist_b < r) {
        float penetration = r - dist_b;
        n.y -= penetration * 0.90f;

        // 仅在真实高速抛掷撞击 (flying_timer > 0 或 猛烈摔打冲击 > 25.0px/s) 时才触发拍扁撞击事件
        if ((flying_timer > 0.0f && std::abs(n.vy) > 8.0f) || n.vy > 25.0f) {
            impact_occurred = true;
            last_impact_speed = std::max(last_impact_speed, n.vy);
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 1.0f; // 激发 1.0s 黏性玩具吸附
            flying_timer = 0.0f;      // 撞墙瞬间立即终止飞行
        }

        n.vy = 0.0f;
        if (flying_timer <= 0.0f) {
            n.vx *= (sticky_clog_timer > 0.0f) ? 0.25f : 0.82f;
        }
        n.contact_bottom = std::min(1.0f, penetration / (r * 0.30f));
    }

    // 2. 顶部天花板撞击
    float dist_t = n.y - 1;
    if (dist_t < r) {
        float penetration = r - dist_t;
        n.y += penetration * 0.90f;

        if ((flying_timer > 0.0f && std::abs(n.vy) > 8.0f) || n.vy < -25.0f) {
            impact_occurred = true;
            last_impact_speed = std::max(last_impact_speed, -n.vy);
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 1.0f;
            flying_timer = 0.0f;
        }

        n.vy = 0.0f;
        if (flying_timer <= 0.0f) {
            n.vx *= (sticky_clog_timer > 0.0f) ? 0.25f : 0.82f;
        }
        n.contact_top = std::min(1.0f, penetration / (r * 0.30f));
    }

    // 3. 左壁撞击
    float dist_l = n.x - 1;
    if (dist_l < r) {
        float penetration = r - dist_l;
        n.x += penetration * 0.90f;

        if ((flying_timer > 0.0f && std::abs(n.vx) > 8.0f) || n.vx < -25.0f) {
            impact_occurred = true;
            last_impact_speed = std::max(last_impact_speed, -n.vx);
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 1.0f;
            flying_timer = 0.0f;
        }

        if (is_rolling) {
            n.vx = -n.vx * 0.85f;
        } else {
            n.vx = 0.0f;
        }
        n.vy *= (sticky_clog_timer > 0.0f) ? 0.25f : 0.82f;
        n.contact_left = std::min(1.0f, penetration / (r * 0.30f));
    }

    // 4. 右壁撞击
    float dist_r = (SCREEN_W - 1) - n.x;
    if (dist_r < r) {
        float penetration = r - dist_r;
        n.x -= penetration * 0.90f;

        if ((flying_timer > 0.0f && std::abs(n.vx) > 8.0f) || n.vx > 25.0f) {
            impact_occurred = true;
            last_impact_speed = std::max(last_impact_speed, n.vx);
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 1.0f;
            flying_timer = 0.0f;
        }

        if (is_rolling) {
            n.vx = -n.vx * 0.85f;
        } else {
            n.vx = 0.0f;
        }
        n.vy *= (sticky_clog_timer > 0.0f) ? 0.25f : 0.82f;
        n.contact_right = std::min(1.0f, penetration / (r * 0.30f));
    }
}

void SkeletonSystem::updateNodePhysics(int i, float dt, float gx, float gy, float cfx, float cfy, float tension, bool is_upside_down) {
    SkeletonNode &n = nodes[i];

    // 头部（i=0）具有主动支撑抗力，中尾部（i>=2）自然感受真实重力拖拽下坠
    float muscle_resistance = 0.20f + tension * 0.35f;
    if (has_pull_target) {
        // 触手攀爬中：全骨架壁面高附着抗重力支持（92% 重力消除，畅游蓝色背景）！
        muscle_resistance = 0.92f;
    } else if (is_creeping_motion) {
        // 表皮小触手蠕动漫步中：壁面吸附力抵抗 85% 重力，支持全屏 2D 攀爬！
        muscle_resistance = 0.85f;
    } else if (sticky_clog_timer > 0.0f) {
        muscle_resistance = 0.90f;
    } else if (is_hanging) {
        muscle_resistance = 0.95f; // 荡秋千时整体强力聚拢为牛顿摆球
    } else if (is_upside_down && i == 0) {
        muscle_resistance = std::max(muscle_resistance, 0.65f);
    } else if (i >= 2) {
        // 尾部肌肉抗力大幅降低，让尾巴沉实下垂拖拽！
        muscle_resistance = std::min(muscle_resistance, 0.18f);
    }

    float eff_gx = (flying_timer > 0.0f) ? 0.0f : (gx * (1.0f - muscle_resistance));
    float eff_gy = (flying_timer > 0.0f) ? 0.0f : (gy * (1.0f - muscle_resistance));

    // 恢复沉实的重力加速度传导 (尾部重力充足，下摆自然流畅)
    float g_scale = (i >= 2) ? 0.95f : 0.65f;
    n.vx += eff_gx * (g_scale / n.mass);
    n.vy += eff_gy * (g_scale / n.mass);

    if (flying_timer <= 0.0f) {
        float c_lead = (i == 0) ? 1.25f : ((i == 1) ? 0.70f : ((i == 2) ? 0.35f : 0.10f));
        n.vx += cfx * (c_lead * 0.8f / n.mass);
        n.vy += cfy * (c_lead * 0.8f / n.mass);
    }

    // 【表皮小触手物理驱动力】头部 Node 0 作为前导牵引核心，中尾部 Node 1..4 自然尾随
    if (is_creeping_motion && flying_timer <= 0.0f && !is_hanging) {
        float dx = creep_target_x - nodes[0].x;
        float dy = creep_target_y - nodes[0].y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 2.0f) {
            float nx = dx / dist;
            float ny = dy / dist;
            float creep_force = 16.0f * creep_speed_mult; // 充沛的 2D 蠕动力
            
            // 头部获得 1.25x 牵引力，尾部依次跟随，驱动全身体在蓝色背景上爬行
            float lead_factor = (i == 0) ? 1.25f : ((i == 1) ? 0.85f : ((i == 2) ? 0.60f : 0.35f));
            float wave = std::sin(millis() * 0.006f - (float)i * 1.0f);
            float final_force = creep_force * lead_factor * (1.0f + 0.20f * wave);
            
            n.vx += nx * final_force * (1.0f / n.mass) * dt;
            n.vy += ny * final_force * (1.0f / n.mass) * dt;
        }
    }

    if (has_pull_target && flying_timer <= 0.0f) {
        float dx = pull_target_x - n.x;
        float dy = pull_target_y - n.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 0.2f) {
            // 前后梯度的 PD 牵引拉力，拉动整个肉身躯干向上攀爬！
            float node_factor = 1.0f - (float)i * 0.16f;
            float k_p = pull_strength * 3.2f * node_factor;
            n.vx += dx * k_p * dt * 30.0f;
            n.vy += dy * k_p * dt * 30.0f;
        }
        if (dist < 6.0f) {
            float brake = 0.50f + (dist / 6.0f) * 0.40f;
            n.vx *= brake;
            n.vy *= brake;
        }
    }

    float damp = SPRING_DAMPING - tension * 0.10f;
    if (flying_timer > 0.0f) {
        damp = 0.985f; // 飞行态无损极速冲刺！
    } else if (sticky_clog_timer > 0.0f) {
        damp *= 0.80f; // 黏性吸附高阻尼
    }

    n.vx *= damp;
    n.vy *= damp;

    n.x += n.vx * dt * 30.0f;
    n.y += n.vy * dt * 30.0f;

    n.bleb_offset_x *= 0.88f;
    n.bleb_offset_y *= 0.88f;

    applyWallAdhesion(i);
}

void SkeletonSystem::solveSpringConstraints(float tension) {
    if (is_bouncing_ball) {
        // 紧绷超弹球体模式 (Tight Superball Spheroid with Elastic Deformation)
        // 整个身体 5 个节点以 Node 2 为核心紧密聚合成一个弹性球，绝不拖尾或分散！
        float cx = nodes[2].x;
        float cy = nodes[2].y;
        float cvx = nodes[2].vx;
        float cvy = nodes[2].vy;

        float t_x[5] = {
            cx,
            cx - 7.5f * bounce_stretch_x,
            cx,
            cx + 7.5f * bounce_stretch_x,
            cx
        };
        float t_y[5] = {
            cy - 6.5f * bounce_squash_y,
            cy + 1.0f * bounce_squash_y,
            cy,
            cy + 1.0f * bounce_squash_y,
            cy + 6.5f * bounce_squash_y
        };

        for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
            if (i == 2) continue;
            nodes[i].x += (t_x[i] - nodes[i].x) * 0.85f;
            nodes[i].y += (t_y[i] - nodes[i].y) * 0.85f;
            nodes[i].vx = cvx;
            nodes[i].vy = cvy;
        }
        return;
    }

    if (is_rolling) {
        // 软体球形滚动模式
        float cx = nodes[2].x;
        float cy = nodes[2].y;
        float cvx = nodes[2].vx;
        float cvy = nodes[2].vy;
        roll_angle += cvx * 0.012f;

        for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
            if (i == 2) continue;
            float ang = roll_angle + (float)i * 1.57079f;
            float tx = cx + std::cos(ang) * 8.5f;
            float ty = cy + std::sin(ang) * 8.5f;
            nodes[i].x += (tx - nodes[i].x) * 0.85f;
            nodes[i].y += (ty - nodes[i].y) * 0.85f;
            nodes[i].vx = cvx;
            nodes[i].vy = cvy;
        }
        return;
    }

    if (is_peeking) {
        // 【边缘暗中观察/窥视模式 (4-Edge Peeking)】
        float hx = nodes[0].x;
        float hy = nodes[0].y;
        float target_head_x = hx;
        float target_head_y = hy;
        float body_base_x = hx;
        float body_base_y = hy;

        if (peeking_edge == 0) { // Top
            target_head_y = 5.0f - peeking_offset;
            body_base_y = -10.0f;
            nodes[0].contact_top = 0.85f;
        } else if (peeking_edge == 1) { // Right
            target_head_x = (float)SCREEN_W - 5.0f + peeking_offset;
            body_base_x = (float)SCREEN_W + 10.0f;
            nodes[0].contact_right = 0.85f;
        } else if (peeking_edge == 2) { // Bottom
            target_head_y = (float)SCREEN_H - 5.0f + peeking_offset;
            body_base_y = (float)SCREEN_H + 10.0f;
            nodes[0].contact_bottom = 0.85f;
        } else if (peeking_edge == 3) { // Left
            target_head_x = 5.0f - peeking_offset;
            body_base_x = -10.0f;
            nodes[0].contact_left = 0.85f;
        }

        if (peeking_edge == 0 || peeking_edge == 2) {
            nodes[0].y += (target_head_y - nodes[0].y) * 0.45f;
        } else {
            nodes[0].x += (target_head_x - nodes[0].x) * 0.45f;
        }

        for (int i = 1; i < SKELETON_NODE_COUNT; ++i) {
            float lag = (float)i * 11.0f;
            if (peeking_edge == 0 || peeking_edge == 2) {
                float tx = (nodes[0].vx >= 0.0f) ? (hx - lag) : (hx + lag);
                nodes[i].x += (tx - nodes[i].x) * 0.35f;
                nodes[i].y += (body_base_y - nodes[i].y) * 0.50f;
                nodes[i].vx = nodes[0].vx * 0.75f;
                nodes[i].vy = 0.0f;
                if (peeking_edge == 0) nodes[i].contact_top = 1.0f;
                else nodes[i].contact_bottom = 1.0f;
            } else {
                float ty = (nodes[0].vy >= 0.0f) ? (hy - lag) : (hy + lag);
                nodes[i].x += (body_base_x - nodes[i].x) * 0.50f;
                nodes[i].y += (ty - nodes[i].y) * 0.35f;
                nodes[i].vx = 0.0f;
                nodes[i].vy = nodes[0].vy * 0.75f;
                if (peeking_edge == 1) nodes[i].contact_right = 1.0f;
                else nodes[i].contact_left = 1.0f;
            }
        }
        return;
    }

    float creep_wave_phase = millis() * 0.0075f;

    for (int i = 1; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &prev = nodes[i - 1];
        SkeletonNode &curr = nodes[i];

        float dx = curr.x - prev.x;
        float dy = curr.y - prev.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        float rest = rest_lengths[i - 1];
        if (has_pull_target || flying_timer > 0.0f) {
            rest *= 1.45f; // 高速飞行时受离心力拉长身躯
        } else if (is_creeping_motion && !is_hanging) {
            // 蠕行模式：沿身体传导的尺蠖波动 (Peristaltic Accordion Wave)
            float seg_phase = creep_wave_phase - (float)(i - 1) * 1.35f;
            float accordion = std::sin(seg_phase);
            // 保持整体圆润聚拢，波动幅度减小以消除水母感，基准长度缩短保持圆润不散开
            rest = 7.0f + 2.5f * accordion;
        } else if (is_creeping_motion) {
            rest = 7.0f; // 贴地迅游时保持紧凑圆形
        }
        if (dist > 0.01f) {
            float delta = dist - rest;
            float force = delta * (SPRING_STIFFNESS + tension * 0.20f);

            float nx = dx / dist;
            float ny = dy / dist;

            curr.vx -= (nx * force) / curr.mass;
            curr.vy -= (ny * force) / curr.mass;

            float back_pull = (has_pull_target || sticky_clog_timer > 0.0f || flying_timer > 0.0f) ? 0.02f : 0.05f;
            prev.vx += (nx * force * back_pull) / prev.mass;
            prev.vy += (ny * force * back_pull) / prev.mass;

            // 允许骨架拉伸为细长液态丝线 (拉丝行为表现)
            float max_allowed_dist = rest * 2.2f;
            if (dist > max_allowed_dist) {
                curr.x = prev.x + nx * max_allowed_dist;
                curr.y = prev.y + ny * max_allowed_dist;
            }
        }
    }
}

void SkeletonSystem::solveHangingConstraint(float dt, float gravity_x, float gravity_y) {
    if (!is_hanging) return;

    // 1. 计算设备倾斜的真实物理重力方向角 (rad)
    // 倾斜向右: gravity_x > 0 -> target_tilt_angle > 0 (向右摆)
    // 倾斜向左: gravity_x < 0 -> target_tilt_angle < 0 (向左摆)
    float eff_gy = std::max(1.2f, gravity_y);
    float target_tilt_angle = std::atan2(gravity_x, eff_gy);

    if (!is_bat_hang) {
        // 荡秋千玩耍模式：
        // 毒液自身轻快自主蹬动节奏 (auto pump) + 用户倾斜重力力矩 (user gravity torque)
        swing_phase += dt * 2.8f;
        float auto_pump = std::sin(swing_phase) * 4.2f; // 自主摆动蹬动加速度
        float torque_user_tilt = (target_tilt_angle - swing_angle) * 18.0f; // 重力倾斜驱动力矩

        float angular_accel = torque_user_tilt + auto_pump;
        swing_ang_vel += angular_accel * dt;
        swing_ang_vel *= (1.0f - dt * 1.6f); // 自然空气阻尼
        swing_angle += swing_ang_vel * dt;

        // 摆角物理安全限制 ±75度 (±1.30 rad)
        swing_angle = std::max(-1.30f, std::min(1.30f, swing_angle));
    } else {
        // 倒挂金钩模式：无自主摆动，纯粹随用户倾斜重力垂悬
        float torque_user_tilt = (target_tilt_angle - swing_angle) * 14.0f;
        swing_ang_vel += torque_user_tilt * dt;
        swing_ang_vel *= (1.0f - dt * 3.2f); // 临界阻尼平稳垂直下垂
        swing_angle += swing_ang_vel * dt;
        swing_angle = std::max(-1.30f, std::min(1.30f, swing_angle));
    }

    float theta = swing_angle;

    // 计算单摆球心目标位置
    float bob_x = anchor_x + std::sin(theta) * rope_len;
    float bob_y = anchor_y + std::cos(theta) * rope_len;

    // 头部平滑精准跟随单摆轨迹
    SkeletonNode &head = nodes[0];
    float h_dx = bob_x - head.x;
    float h_dy = bob_y - head.y;
    head.x += h_dx * 0.88f;
    head.y += h_dy * 0.88f;
    head.vx = swing_ang_vel * rope_len * std::cos(theta);
    head.vy = -swing_ang_vel * rope_len * std::sin(theta);

    // 【单摆悬挂姿态（倒挂金钟）：上部圆润饱满，下部有自然松弛垂落的流体尾部】
    if (!is_bat_hang) {
        // 荡秋千单摆模式：
        // 头部与上身（0, 1, 2）紧密聚拢形成圆润饱满的钟身主体
        // 中下部与尾部（3, 4）沿重力/单摆轴向向下延伸，并带有轻微惯性摆尾延迟
        for (int k = 1; k < SKELETON_NODE_COUNT; ++k) {
            float dist_from_head = 0.0f;
            float lag_phase = 0.0f;
            float follow_rate = 0.85f;

            if (k == 1) {
                dist_from_head = 4.0f; // 紧贴头部形成圆润球顶
                lag_phase = 0.06f;
                follow_rate = 0.85f;
            } else if (k == 2) {
                dist_from_head = 8.5f; // 构成饱满钟身核心
                lag_phase = 0.14f;
                follow_rate = 0.80f;
            } else if (k == 3) {
                dist_from_head = 14.5f; // 向下松弛收窄
                lag_phase = 0.26f;
                follow_rate = 0.72f;
            } else { // k == 4 (尾尖)
                dist_from_head = 21.0f; // 优雅下垂的松弛尾梢
                lag_phase = 0.40f;
                follow_rate = 0.65f;
            }

            float k_theta = theta - swing_ang_vel * lag_phase * 0.22f;
            float target_k_x = head.x + std::sin(k_theta) * dist_from_head;
            float target_k_y = head.y + std::cos(k_theta) * dist_from_head;

            nodes[k].x += (target_k_x - nodes[k].x) * follow_rate;
            nodes[k].y += (target_k_y - nodes[k].y) * follow_rate;
            nodes[k].vx = head.vx * (1.0f - (float)k * 0.10f);
            nodes[k].vy = head.vy * (1.0f - (float)k * 0.10f);
        }
    } else {
        // 【蝙蝠倒挂模式】：身体笔直垂直向下悬垂成深沉水滴状
        for (int k = 1; k < SKELETON_NODE_COUNT; ++k) {
            float target_k_x = head.x - std::sin(theta) * (float)k * 3.5f;
            float target_k_y = head.y + std::cos(theta) * (float)k * 7.5f;

            nodes[k].x += (target_k_x - nodes[k].x) * 0.75f;
            nodes[k].y += (target_k_y - nodes[k].y) * 0.75f;
            nodes[k].vx = head.vx * 0.5f;
            nodes[k].vy = head.vy * 0.5f;
        }
    }
}

void SkeletonSystem::update(float dt, float gravity_x, float gravity_y,
                            float crawl_force_x, float crawl_force_y,
                            float neuro_tension, float spike_intensity,
                            float respiration, bool is_upside_down,
                            const float *spectrum_bands) {
    if (flying_timer > 0.0f) {
        flying_timer -= dt;
        if (flying_timer < 0.0f) flying_timer = 0.0f;
    }

    if (launch_grace_timer > 0.0f) {
        launch_grace_timer -= dt;
        if (launch_grace_timer < 0.0f) launch_grace_timer = 0.0f;
    }

    if (sticky_clog_timer > 0.0f) {
        sticky_clog_timer -= dt;
        if (sticky_clog_timer < 0.0f) sticky_clog_timer = 0.0f;
    }

    if (flip_cooldown > 0.0f) {
        flip_cooldown -= dt;
        if (flip_cooldown < 0.0f) flip_cooldown = 0.0f;
    }

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        updateNodePhysics(i, dt, gravity_x, gravity_y, crawl_force_x, crawl_force_y, neuro_tension, is_upside_down);
    }

    solveSpringConstraints(neuro_tension);
    solveSpringConstraints(neuro_tension);

    // 单摆悬挂绳索约束与自主蹬秋千 (接收真实重力 X 与 Y 向量驱动倾斜互动)
    solveHangingConstraint(dt, gravity_x, gravity_y);

    // 增加 pull_target 超时保护 (1.6秒看门狗自动释放，防止死锁)
    if (has_pull_target) {
        pull_timeout_timer += dt;
        if (pull_timeout_timer > 1.6f) {
            clearPullTarget();
        }
    }

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &n = nodes[i];
        // 鼓包平滑指数衰减回位 (平稳柔和，杜绝瞬移抽搐)
        n.bleb_offset_x *= 0.85f;
        n.bleb_offset_y *= 0.85f;

        // 极其平缓自然的深沉生命呼吸 (7.0秒悠长呼吸周期，起伏柔和平顺)
        float peristalsis_volume_scale = 1.0f;
        if (is_creeping_motion && !is_hanging && flying_timer <= 0.0f) {
            float creep_wave_phase = millis() * 0.0075f;
            float seg_phase = creep_wave_phase - (float)i * 1.35f;
            float accordion = std::sin(seg_phase);
            // 节段缩短 (accordion < 0) 时肉体鼓胀变粗 (1.35x)，伸长 (accordion > 0) 时肉体收缩变细 (0.72x)
            float node_weight = (i >= 3) ? 0.38f : ((i >= 1) ? 0.28f : 0.18f);
            peristalsis_volume_scale = 1.0f - node_weight * accordion;
        }

        // 音乐播放器 5 频段频谱均衡器律动 (5-Band Spectrum EQ Visualization)
        // Node 0: Sub-Bass, Node 1: Bass, Node 2: Mid, Node 3: Presence, Node 4: Treble
        float eq_band_energy = (spectrum_bands != nullptr) ? spectrum_bands[i] : 0.0f;
        float node_eq_pulse = eq_band_energy * 6.5f; // 各节点沿身体随各自频段独立起伏脉动

        if (eq_band_energy > 0.38f && (rand() % 100) < 25) {
            triggerLocalBleb(i, 1.2f * eq_band_energy);
        }

        // 呼吸起伏胸腔仿生形变（非对称背腹起伏，告别水球式对称膨胀）
        // Node 0 (Head): 轻微抬头/吸气
        // Node 1 (Chest): 主要胸腔起伏，吸气时向上提拔舒展，呼气时平缓下沉
        // Node 2 (Abdomen): 腹部跟随呼吸微颤
        // Node 3, 4 (Tail): 尾部贴地自然松弛
        float node_resp_gain = (i == 1) ? 1.0f : ((i == 2) ? 0.70f : ((i == 0) ? 0.45f : 0.15f));
        float resp_rise_y = respiration * node_resp_gain * 0.35f;
        float resp_narrow_x = -respiration * node_resp_gain * 0.15f;

        float r = (n.base_radius + node_eq_pulse) * peristalsis_volume_scale;

        if (has_pull_target || flying_timer > 0.0f) {
            r *= 0.90f;
        }

        float contact_y = std::max(n.contact_bottom, n.contact_top);
        float contact_x = std::max(n.contact_left, n.contact_right);

        float flat_x = 1.0f + resp_narrow_x;
        float flat_y = 1.0f + resp_rise_y;

        // 黏性撞击拍扁增强因子 (撞墙瞬间增强形变)
        float stick_boost = (sticky_clog_timer > 0.0f) ? (1.0f + (sticky_clog_timer / 1.0f) * 1.5f) : 1.0f;

        // 贴边拍扁温和形变与尾部瘫软 (尾部节点越来越扁)
        if (contact_y > 0.05f) {
            float eff = std::min(1.0f, contact_y * 0.6f * stick_boost);
            float tail_slouch = (i >= 1) ? (i * 0.10f) : 0.0f; // 0.1, 0.2, 0.3, 0.4
            flat_y -= eff * (0.22f + tail_slouch);
            flat_x += eff * (0.25f + tail_slouch * 0.6f);
        }
        if (contact_x > 0.05f) {
            float eff = std::min(1.0f, contact_x * 0.6f * stick_boost);
            flat_x -= eff * 0.35f;
            flat_y += eff * 0.45f;
        }

        // 速度形变：基于速度主方向自然拉长成细线 (液态共生体拉丝行为)
        // 注意：荡秋千与倒挂时采用“倒挂金钟”形态（上部饱满圆润，尾部自然收窄垂坠）
        if (is_hanging) {
            if (!is_bat_hang) {
                // 倒挂金钟：头部与上身保持圆润(flat ≈ 1.0)，尾部自然松弛微垂(flat_x ≈ 0.88, flat_y ≈ 1.12)
                float tail_relax = (i >= 3) ? ((float)(i - 2) * 0.08f) : 0.0f;
                flat_x = 1.0f - tail_relax;
                flat_y = 1.02f + tail_relax * 1.5f;
            } else {
                // 倒挂：纵向水滴垂感
                float tail_relax = (i >= 2) ? ((float)(i - 1) * 0.06f) : 0.0f;
                flat_x = 0.94f - tail_relax;
                flat_y = 1.08f + tail_relax * 1.5f;
            }
        } else {
            float v_speed = std::sqrt(n.vx * n.vx + n.vy * n.vy);
            if (v_speed > 0.6f) {
                float max_stretch = is_creeping_motion ? 1.05f : 2.0f; // 限制迅游时的拉丝形变，保持大致圆形
                float stretch = std::min(max_stretch, 1.0f + (v_speed - 0.6f) * 0.12f);
                if (std::abs(n.vx) > std::abs(n.vy)) {
                    flat_x *= stretch;
                    flat_y /= std::sqrt(stretch);
                } else {
                    flat_y *= stretch;
                    flat_x /= std::sqrt(stretch);
                }
            }
        }

        if (is_peeking) {
            if (i == 0) {
                flat_x = 1.05f;
                flat_y = 0.88f;
            } else {
                flat_x = 1.42f;
                flat_y = 0.42f;
            }
        }

        // 放宽形变范围，允许细丝形态
        flat_x = std::max(0.35f, std::min(2.2f, flat_x));
        flat_y = std::max(0.35f, std::min(2.2f, flat_y));

        n.radius_x = std::max(2.5f, r * flat_x);
        n.radius_y = std::max(2.5f, r * flat_y);

        // 强制全骨架节点 100% 屏幕内绝对硬安全钳位与防丢失救生圈
        if (std::isnan(n.x) || std::isnan(n.y) || std::isnan(n.vx) || std::isnan(n.vy)) {
            n.x = 120.0f + (float)i * 3.0f;
            n.y = 70.0f;
            n.vx = 0.0f;
            n.vy = 0.0f;
        }

        float margin_left = 6.0f;
        float margin_right = 6.0f;
        float margin_top = 6.0f;
        float margin_bottom = 6.0f;

        if (is_peeking) {
            float peek_margin = -35.0f; // 允许深度隐藏到屏幕外
            if (peeking_edge == 0) margin_top = peek_margin;
            else if (peeking_edge == 1) margin_right = peek_margin;
            else if (peeking_edge == 2) margin_bottom = peek_margin;
            else if (peeking_edge == 3) margin_left = peek_margin;
        }

        if (n.x < margin_left) { n.x = margin_left; n.vx = std::max(0.0f, n.vx); }
        if (n.x > (float)SCREEN_W - margin_right) { n.x = (float)SCREEN_W - margin_right; n.vx = std::min(0.0f, n.vx); }
        if (n.y < margin_top) { n.y = margin_top; n.vy = std::max(0.0f, n.vy); }
        if (n.y > (float)SCREEN_H - margin_bottom) { n.y = (float)SCREEN_H - margin_bottom; n.vy = std::min(0.0f, n.vy); }
    }
}


float SkeletonSystem::getHeadingAngle() const {
    // 身体主轴向量：从尾部 (nodes[4]) 指向头部 (nodes[0])
    float dx = nodes[0].x - nodes[4].x;
    float dy = nodes[0].y - nodes[4].y;
    return std::atan2(dy, dx);
}

void SkeletonSystem::alignHeadingToTarget(float target_x, float target_y) {
    if (is_hanging || flying_timer > 0.0f || is_bouncing_ball || is_rolling) return;

    float cx = nodes[2].x; // 身体质心
    float cy = nodes[2].y;

    float move_dx = target_x - cx;
    float move_dy = target_y - cy;
    float move_dist = std::sqrt(move_dx * move_dx + move_dy * move_dy);
    if (move_dist < 8.0f) return;

    // 【1. 智能平滑掉头（带 0.65s 防抖冷却，彻底消除高频瞬移与倒退走）】
    if (flip_cooldown <= 0.0f && std::abs(move_dx) > 12.0f) {
        bool target_on_left = (move_dx < 0.0f);
        bool head_on_right = (nodes[0].x > nodes[2].x + 3.0f);
        bool target_on_right = (move_dx > 0.0f);
        bool head_on_left = (nodes[0].x < nodes[2].x - 3.0f);

        if ((target_on_left && head_on_right) || (target_on_right && head_on_left)) {
            // 需要调头！设置防抖冷却 0.65 秒，杜绝来回抽动
            flip_cooldown = 0.65f;
            float dir_sign = target_on_left ? -1.0f : 1.0f;
            
            // 物理平滑回旋：给予头部向前上方的翻转力矩，而非硬编码瞬间传送
            nodes[0].vx = dir_sign * 16.0f;
            nodes[0].vy = -3.5f;
            nodes[1].vx = dir_sign * 9.0f;
            nodes[1].vy = -1.5f;
            
            // 顺畅调整节点顺序排列（头部移至前进方向前沿）
            float offset = 11.0f;
            nodes[0].x = nodes[1].x + dir_sign * offset;
            nodes[0].y = nodes[1].y - 2.0f;
            triggerLocalBleb(0, 1.2f);
        }
    }

    // 【2. 温和平滑前向牵引力引导头部】
    float hx = nodes[0].x;
    float hy = nodes[0].y;
    float t_dx = target_x - hx;
    float t_dy = target_y - hy;
    float t_dist = std::sqrt(t_dx * t_dx + t_dy * t_dy);
    if (t_dist > 4.0f) {
        float pull = std::min(4.5f, t_dist * 0.35f);
        nodes[0].vx += (t_dx / t_dist) * pull * 0.12f;
        nodes[0].vy += (t_dy / t_dist) * pull * 0.12f;
    }
}

void SkeletonSystem::applyCreepingMotion(float dir_x, float dir_y, float speed, float dt) {
    if (is_hanging || flying_timer > 0.0f) return;

    float len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
    if (len < 0.5f) return;

    float nx = dir_x / len;
    float ny = dir_y / len;

    creep_locomotion_phase += dt * 6.5f;

    // 尺蠖前进波浪：头部在前冲刺伸展，尾部随后跟进抽吸
    float head_wave = std::max(0.0f, std::sin(creep_locomotion_phase));

    // 头部向前拉伸突进 (12 ~ 26 px/s)
    float head_speed = (12.0f + 14.0f * head_wave) * speed;
    nodes[0].x += nx * head_speed * dt;
    nodes[0].y += ny * head_speed * dt;
    nodes[0].vx = nx * head_speed * 0.5f;
    nodes[0].vy = ny * head_speed * 0.5f;

    // 身体各节依次滞后跟进推进，形成连贯蠕动
    for (int i = 1; i < SKELETON_NODE_COUNT; ++i) {
        float node_phase = creep_locomotion_phase - (float)i * 0.9f;
        float wave_surge = std::max(0.0f, std::sin(node_phase));
        float node_speed = (10.0f + 16.0f * wave_surge) * speed;

        nodes[i].x += nx * node_speed * dt;
        nodes[i].y += ny * node_speed * dt;
        nodes[i].vx = nx * node_speed * 0.45f;
        nodes[i].vy = ny * node_speed * 0.45f;
    }
}
