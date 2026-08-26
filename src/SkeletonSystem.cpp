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
    if (flying_timer > 0.0f) return;
    is_hanging = true;
    anchor_x = ax;
    anchor_y = ay;
    rope_len = rope_length;
    swing_phase = 0.0f;
    clearPullTarget();
}

void SkeletonSystem::clearHangingAnchor() {
    is_hanging = false;
}

void SkeletonSystem::applyImpulse(float ix, float iy) {
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        nodes[i].vx += ix / nodes[i].mass;
        nodes[i].vy += iy / nodes[i].mass;
    }
}

// 激发干脆利落的极速甩飞抛体运动
void SkeletonSystem::triggerSlingThrow(float dir_x, float dir_y, float speed) {
    flying_timer = 0.50f; // 0.5s 高速无阻尼飞行冲刺
    clearPullTarget();    // 立即解除任何正在进行的触手牵引！
    clearHangingAnchor(); // 立即打断蛛丝悬挂！
    sticky_clog_timer = 0.0f;

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        nodes[i].vx = dir_x * speed;
        nodes[i].vy = dir_y * speed;
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

    // 1. 底部地面撞击
    float dist_b = (SCREEN_H - 1) - n.y;
    if (dist_b < r) {
        float penetration = r - dist_b;
        n.y -= penetration * 0.90f;

        // 仅在真实高速抛掷撞击 (flying_timer > 0 或 猛烈摔打冲击 > 25.0px/s) 时才触发拍扁撞击事件
        if ((flying_timer > 0.0f && std::abs(n.vy) > 10.0f) || n.vy > 28.0f) {
            impact_occurred = true;
            last_impact_speed = std::max(last_impact_speed, n.vy);
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 1.0f; // 激发 1.0s 黏性玩具吸附
            flying_timer = 0.0f;      // 撞墙瞬间立即终止飞行
        }

        n.vy = 0.0f;
        n.vx *= (sticky_clog_timer > 0.0f) ? 0.25f : 0.82f;
        n.contact_bottom = std::min(1.0f, penetration / (r * 0.30f));
    }

    // 2. 顶部天花板撞击
    float dist_t = n.y - 1;
    if (dist_t < r) {
        float penetration = r - dist_t;
        n.y += penetration * 0.90f;

        if ((flying_timer > 0.0f && std::abs(n.vy) > 10.0f) || n.vy < -28.0f) {
            impact_occurred = true;
            last_impact_speed = std::max(last_impact_speed, -n.vy);
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 1.0f;
            flying_timer = 0.0f;
        }

        n.vy = 0.0f;
        n.vx *= (sticky_clog_timer > 0.0f) ? 0.25f : 0.82f;
        n.contact_top = std::min(1.0f, penetration / (r * 0.30f));
    }

    // 3. 左壁撞击
    float dist_l = n.x - 1;
    if (dist_l < r) {
        float penetration = r - dist_l;
        n.x += penetration * 0.90f;

        if ((flying_timer > 0.0f && std::abs(n.vx) > 10.0f) || n.vx < -28.0f) {
            impact_occurred = true;
            last_impact_speed = std::max(last_impact_speed, -n.vx);
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 1.0f;
            flying_timer = 0.0f;
        }

        n.vx = 0.0f;
        n.vy *= (sticky_clog_timer > 0.0f) ? 0.25f : 0.82f;
        n.contact_left = std::min(1.0f, penetration / (r * 0.30f));
    }

    // 4. 右壁撞击
    float dist_r = (SCREEN_W - 1) - n.x;
    if (dist_r < r) {
        float penetration = r - dist_r;
        n.x -= penetration * 0.90f;

        if ((flying_timer > 0.0f && std::abs(n.vx) > 10.0f) || n.vx > 28.0f) {
            impact_occurred = true;
            last_impact_speed = std::max(last_impact_speed, n.vx);
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 1.0f;
            flying_timer = 0.0f;
        }

        n.vx = 0.0f;
        n.vy *= (sticky_clog_timer > 0.0f) ? 0.25f : 0.82f;
        n.contact_right = std::min(1.0f, penetration / (r * 0.30f));
    }
}

void SkeletonSystem::updateNodePhysics(int i, float dt, float gx, float gy, float cfx, float cfy, float tension, bool is_upside_down) {
    SkeletonNode &n = nodes[i];

    // 头部（i=0）具有主动支撑抗力，中尾部（i>=2）自然感受真实重力拖拽下坠
    float muscle_resistance = 0.20f + tension * 0.35f;
    if (i == 0 && has_pull_target) {
        muscle_resistance = 0.85f;
    }
    if (sticky_clog_timer > 0.0f) {
        muscle_resistance = 0.90f;
    }
    if (is_upside_down && i == 0) {
        muscle_resistance = std::max(muscle_resistance, 0.65f);
    }
    if (is_hanging) {
        muscle_resistance = 0.95f; // 荡秋千时整体强力聚拢为牛顿摆球
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
        n.vx += cfx * (0.8f / n.mass);
        n.vy += cfy * (0.8f / n.mass);
    }

    if (has_pull_target && i == 0 && flying_timer <= 0.0f) {
        float dx = pull_target_x - n.x;
        float dy = pull_target_y - n.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 0.2f) {
            // 平滑 PD 弹簧拉力（连续过渡，无阶跃突变）
            float k_p = pull_strength * 2.5f;
            n.vx += dx * k_p * dt * 30.0f;
            n.vy += dy * k_p * dt * 30.0f;
        }
        if (dist < 5.0f) {
            // 靠近抓取点时施加平滑临界阻尼刹车，稳稳吸附，绝不原地抖动！
            float brake = 0.55f + (dist / 5.0f) * 0.35f;
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
    for (int i = 1; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &prev = nodes[i - 1];
        SkeletonNode &curr = nodes[i];

        float dx = curr.x - prev.x;
        float dy = curr.y - prev.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        float rest = rest_lengths[i - 1];
        if (has_pull_target || flying_timer > 0.0f) {
            rest *= 1.45f; // 高速飞行时受离心力拉长身躯
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

void SkeletonSystem::solveHangingConstraint(float dt) {
    if (!is_hanging) return;

    // 【牛顿摆谐波物理单摆运动 (Newton's Cradle Pendulum Kinematics)】
    swing_phase += dt * SWING_PUMP_FREQ;
    float max_theta = 0.52f; // 最大摆角约 30 度 (大幅度优雅牛顿单摆)
    float theta = max_theta * std::sin(swing_phase);

    // 计算单摆球心目标位置
    float bob_x = anchor_x + std::sin(theta) * rope_len;
    float bob_y = anchor_y + std::cos(theta) * rope_len;

    // 头部平滑精准跟随单摆轨迹
    SkeletonNode &head = nodes[0];
    float h_dx = bob_x - head.x;
    float h_dy = bob_y - head.y;
    head.x += h_dx * 0.85f;
    head.y += h_dy * 0.85f;
    head.vx = std::cos(theta) * max_theta * SWING_PUMP_FREQ * rope_len * std::cos(swing_phase);
    head.vy = -std::sin(theta) * max_theta * SWING_PUMP_FREQ * rope_len * std::cos(swing_phase);

    // 【关键】：将后续 4 节骨架节点紧紧抱团聚拢在头部中心 (间距仅 2.5px)，形成浑圆的牛顿摆球！
    // 彻底杜绝重力下拉散开形成的倒梯形斗篷！
    for (int k = 1; k < SKELETON_NODE_COUNT; ++k) {
        float angle_k = (float)k * 1.256f + swing_phase * 0.5f;
        float radius_k = 2.5f + (float)k * 0.8f;
        float target_k_x = head.x + std::cos(angle_k) * radius_k;
        float target_k_y = head.y + std::sin(angle_k) * radius_k + 1.5f;

        nodes[k].x += (target_k_x - nodes[k].x) * 0.80f;
        nodes[k].y += (target_k_y - nodes[k].y) * 0.80f;
        nodes[k].vx = head.vx;
        nodes[k].vy = head.vy;
    }
}

void SkeletonSystem::update(float dt, float gravity_x, float gravity_y,
                            float crawl_force_x, float crawl_force_y,
                            float neuro_tension, float spike_intensity,
                            float respiration, bool is_upside_down,
                            float audio_low) {
    if (flying_timer > 0.0f) {
        flying_timer -= dt;
        if (flying_timer < 0.0f) flying_timer = 0.0f;
    }

    if (sticky_clog_timer > 0.0f) {
        sticky_clog_timer -= dt;
        if (sticky_clog_timer < 0.0f) sticky_clog_timer = 0.0f;
    }

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        updateNodePhysics(i, dt, gravity_x, gravity_y, crawl_force_x, crawl_force_y, neuro_tension, is_upside_down);
    }

    solveSpringConstraints(neuro_tension);
    solveSpringConstraints(neuro_tension);

    // 单摆悬挂绳索约束与自主蹬秋千
    solveHangingConstraint(dt);

    // 增加 pull_target 超时保护 (1.6秒看门狗自动释放，防止死锁)
    if (has_pull_target) {
        pull_timeout_timer += dt;
        if (pull_timeout_timer > 1.6f) {
            clearPullTarget();
        }
    }

    // 低音重拍大鼓点脉动 (仅在超强低音 > 0.45 时产生微弱平滑膨胀，常态下严格为 0)
    float bass_expansion = (audio_low > 0.45f) ? ((audio_low - 0.45f) * 2.2f) : 0.0f;

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &n = nodes[i];
        // 鼓包平滑指数衰减回位 (平稳柔和，杜绝瞬移抽搐)
        n.bleb_offset_x *= 0.85f;
        n.bleb_offset_y *= 0.85f;

        // 极其平缓自然的深沉生命呼吸 (7.0秒悠长呼吸周期，起伏柔和平顺)
        float r = (n.base_radius + bass_expansion) * (1.0f + respiration);

        if (has_pull_target || flying_timer > 0.0f) {
            r *= 0.90f;
        }

        float contact_y = std::max(n.contact_bottom, n.contact_top);
        float contact_x = std::max(n.contact_left, n.contact_right);

        float flat_x = 1.0f;
        float flat_y = 1.0f;

        // 贴边拍扁温和形变 (严格限制在安全区间，杜绝突变成方块横臂)
        if (contact_y > 0.05f) {
            float eff = std::min(1.0f, contact_y * 0.6f);
            flat_y -= eff * 0.22f;
            flat_x += eff * 0.25f;
        }
        if (contact_x > 0.05f) {
            float eff = std::min(1.0f, contact_x * 0.6f);
            flat_x -= eff * 0.22f;
            flat_y += eff * 0.25f;
        }

        // 速度形变：基于速度主方向自然拉长成细线 (液态共生体拉丝行为)
        float v_speed = std::sqrt(n.vx * n.vx + n.vy * n.vy);
        if (v_speed > 0.6f) {
            float stretch = std::min(2.0f, 1.0f + (v_speed - 0.6f) * 0.12f);
            if (std::abs(n.vx) > std::abs(n.vy)) {
                flat_x *= stretch;
                flat_y /= std::sqrt(stretch);
            } else {
                flat_y *= stretch;
                flat_x /= std::sqrt(stretch);
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

        float margin = 6.0f;
        if (n.x < margin) { n.x = margin; n.vx = std::max(0.0f, n.vx); }
        if (n.x > (float)SCREEN_W - margin) { n.x = (float)SCREEN_W - margin; n.vx = std::min(0.0f, n.vx); }
        if (n.y < margin) { n.y = margin; n.vy = std::max(0.0f, n.vy); }
        if (n.y > (float)SCREEN_H - margin) { n.y = (float)SCREEN_H - margin; n.vy = std::min(0.0f, n.vy); }
    }
}

float SkeletonSystem::getHeadingAngle() const {
    // 身体主轴向量：从尾部 (nodes[4]) 指向头部 (nodes[0])
    float dx = nodes[0].x - nodes[4].x;
    float dy = nodes[0].y - nodes[4].y;
    return std::atan2(dy, dx);
}

void SkeletonSystem::alignHeadingToTarget(float target_x, float target_y) {
    if (is_hanging || flying_timer > 0.0f) return;

    float hx = nodes[0].x;
    float hy = nodes[0].y;
    float tx = nodes[4].x;
    float ty = nodes[4].y;

    float move_dx = target_x - hx;
    float move_dy = target_y - hy;
    float move_dist = std::sqrt(move_dx * move_dx + move_dy * move_dy);
    if (move_dist < 4.0f) return;

    // 身体向量 (尾 -> 头)
    float body_dx = hx - tx;
    float body_dy = hy - ty;
    float body_len = std::sqrt(body_dx * body_dx + body_dy * body_dy);
    if (body_len < 1.0f) return;

    // 计算运动方向与身体朝向的点积
    float dot = (move_dx * body_dx + move_dy * body_dy) / (move_dist * body_len);

    // 如果 dot < -0.15f，说明目标在身躯后方，如果不调头就会“倒着走”！
    if (dot < -0.15f) {
        // 触发原地翻转掉头（Head Turnaround Flip）：
        // 让头部沿弧线跃起跨过身体，来到最前侧！
        float norm_move_x = move_dx / move_dist;
        float norm_move_y = move_dy / move_dist;

        // 头部施加前突跨越速度，迅速确立前进龙头
        nodes[0].vx += norm_move_x * 9.5f;
        nodes[0].vy -= 4.2f; // 向上拱起翻身

        // 中间节点配合向上弯折形成拱桥流体翻身弧线
        nodes[1].vy -= 2.8f;
        nodes[2].vy -= 1.5f;
    }
}

void SkeletonSystem::applyCreepingMotion(float dir_x, float dir_y, float speed, float dt) {
    if (is_hanging || flying_timer > 0.0f) return;

    float len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
    if (len < 0.01f) return;

    float nx = dir_x / len;
    float ny = dir_y / len;

    // 头部作为牵引龙头向前滑移
    nodes[0].vx += nx * speed * dt * 22.0f;
    nodes[0].vy += ny * speed * dt * 22.0f;

    // 后续节点依次顺向传导小触手推力
    for (int i = 1; i < SKELETON_NODE_COUNT; ++i) {
        float push = speed * (1.0f - (float)i * 0.14f);
        nodes[i].vx += nx * push * dt * 14.0f;
        nodes[i].vy += ny * push * dt * 14.0f;
    }
}
