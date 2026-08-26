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
    if (flying_timer > 0.0f) return; // 飞行期间拒绝爬行拉力干扰
    has_pull_target = true;
    pull_target_x = tx;
    pull_target_y = ty;
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

    // 1. 底部地面撞击与软体流体下沉贴地 (有效支撑高度降为 0.58r，使身体深度下沉贴紧底框)
    float dist_b = (SCREEN_H - 1) - n.y;
    float soft_r_b = r * 0.58f;
    if (dist_b < soft_r_b) {
        float penetration = soft_r_b - dist_b;
        n.y -= penetration * 0.90f;

        if ((flying_timer > 0.0f && std::abs(n.vy) > 10.0f) || n.vy > 28.0f) {
            impact_occurred = true;
            last_impact_speed = std::max(last_impact_speed, n.vy);
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 1.0f;
            flying_timer = 0.0f;
        }

        n.vy = 0.0f;
        n.vx *= (sticky_clog_timer > 0.0f) ? 0.25f : 0.82f;
        n.contact_bottom = 1.0f;
    } else if (dist_b < r) {
        n.contact_bottom = (r - dist_b) / (r - soft_r_b);
    }

    // 2. 顶部天花板撞击与贴附
    float dist_t = n.y - 1;
    float soft_r_t = r * 0.65f;
    if (dist_t < soft_r_t) {
        float penetration = soft_r_t - dist_t;
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
        n.contact_top = 1.0f;
    } else if (dist_t < r) {
        n.contact_top = (r - dist_t) / (r - soft_r_t);
    }

    // 3. 左壁撞击与贴附
    float dist_l = n.x - 1;
    float soft_r_l = r * 0.65f;
    if (dist_l < soft_r_l) {
        float penetration = soft_r_l - dist_l;
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
        n.contact_left = 1.0f;
    } else if (dist_l < r) {
        n.contact_left = (r - dist_l) / (r - soft_r_l);
    }

    // 4. 右壁撞击与贴附
    float dist_r = (SCREEN_W - 1) - n.x;
    float soft_r_r = r * 0.65f;
    if (dist_r < soft_r_r) {
        float penetration = soft_r_r - dist_r;
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
        n.contact_right = 1.0f;
    } else if (dist_r < r) {
        n.contact_right = (r - dist_r) / (r - soft_r_r);
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
        } else if (prev.contact_bottom > 0.35f && curr.contact_bottom > 0.35f && !is_hanging) {
            rest *= 1.35f; // 贴地静止时受重力压迫横向流体自然舒展铺展
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

        // 贴边流体滩地形变 (底边/壁面流体滩开：纵向高度压扁，横向向两侧自然流淌铺展)
        if (n.contact_bottom > 0.05f) {
            float eff = std::min(1.0f, n.contact_bottom);
            flat_y -= eff * 0.48f; // 高度明显压缩压扁 48% (压至 0.52r 扁平流体)
            flat_x += eff * 0.85f; // 宽度向两侧大幅流淌扩展 85% (扩展至 1.85r 宽滩)
        } else if (n.contact_top > 0.05f) {
            float eff = std::min(1.0f, n.contact_top);
            flat_y -= eff * 0.40f;
            flat_x += eff * 0.70f;
        }
        if (contact_x > 0.05f) {
            float eff = std::min(1.0f, contact_x);
            flat_x -= eff * 0.40f; // 贴侧壁横向压缩
            flat_y += eff * 0.70f; // 纵向顺着重力和墙面流淌拉长
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
    }
}
