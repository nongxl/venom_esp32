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
    sticky_clog_timer = 0.0f;
    impact_occurred = false;
}

void SkeletonSystem::setPullTarget(float tx, float ty, float force) {
    has_pull_target = true;
    pull_target_x = tx;
    pull_target_y = ty;
    pull_strength = force;
}

void SkeletonSystem::clearPullTarget() {
    has_pull_target = false;
    pull_strength = 0.0f;
}

void SkeletonSystem::applyImpulse(float ix, float iy) {
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        nodes[i].vx += ix / nodes[i].mass;
        nodes[i].vy += iy / nodes[i].mass;
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

    // 1. 底部地面高速撞击与贴紧形变
    float dist_b = (SCREEN_H - 1) - n.y;
    if (dist_b < r) {
        float penetration = r - dist_b;
        n.y -= penetration * 0.90f;

        if (n.vy > 1.8f) {
            impact_occurred = true;
            last_impact_speed = n.vy;
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 0.90f; // 激发黏性玩具吸附
        }

        n.vy = 0.0f;
        n.vx *= (sticky_clog_timer > 0.0f) ? 0.35f : 0.82f;
        n.contact_bottom = std::min(1.0f, penetration / (r * 0.30f));
    }

    // 2. 顶部天花板高速撞击与贴紧形变
    float dist_t = n.y - 1;
    if (dist_t < r) {
        float penetration = r - dist_t;
        n.y += penetration * 0.90f;

        if (n.vy < -1.8f) {
            impact_occurred = true;
            last_impact_speed = -n.vy;
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 0.90f;
        }

        n.vy = 0.0f;
        n.vx *= (sticky_clog_timer > 0.0f) ? 0.35f : 0.82f;
        n.contact_top = std::min(1.0f, penetration / (r * 0.30f));
    }

    // 3. 左壁高速撞击与贴紧形变
    float dist_l = n.x - 1;
    if (dist_l < r) {
        float penetration = r - dist_l;
        n.x += penetration * 0.90f;

        if (n.vx < -1.8f) {
            impact_occurred = true;
            last_impact_speed = -n.vx;
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 0.90f;
        }

        n.vx = 0.0f;
        n.vy *= (sticky_clog_timer > 0.0f) ? 0.35f : 0.82f;
        n.contact_left = std::min(1.0f, penetration / (r * 0.30f));
    }

    // 4. 右壁高速撞击与贴紧形变
    float dist_r = (SCREEN_W - 1) - n.x;
    if (dist_r < r) {
        float penetration = r - dist_r;
        n.x -= penetration * 0.90f;

        if (n.vx > 1.8f) {
            impact_occurred = true;
            last_impact_speed = n.vx;
            impact_hit_x = n.x;
            impact_hit_y = n.y;
            sticky_clog_timer = 0.90f;
        }

        n.vx = 0.0f;
        n.vy *= (sticky_clog_timer > 0.0f) ? 0.35f : 0.82f;
        n.contact_right = std::min(1.0f, penetration / (r * 0.30f));
    }
}

void SkeletonSystem::updateNodePhysics(int i, float dt, float gx, float gy, float cfx, float cfy, float tension, bool is_upside_down) {
    SkeletonNode &n = nodes[i];

    // 共生体主动抗重力肌张力与黏性玩具吸附
    float muscle_resistance = 0.35f + tension * 0.45f;
    if (has_pull_target) {
        muscle_resistance = 0.85f;
    }
    if (sticky_clog_timer > 0.0f) {
        muscle_resistance = 1.0f; // 黏性玩具吸附状态下完全抵抗重力下坠
    }
    if (is_upside_down) {
        muscle_resistance = std::max(muscle_resistance, 0.75f);
    }

    float eff_gx = gx * (1.0f - muscle_resistance);
    float eff_gy = gy * (1.0f - muscle_resistance);

    n.vx += eff_gx * (0.35f / n.mass);
    n.vy += eff_gy * (0.35f / n.mass);

    n.vx += cfx * (0.8f / n.mass);
    n.vy += cfy * (0.8f / n.mass);

    // 主动抓取牵引
    if (has_pull_target && i == 0) {
        float dx = pull_target_x - n.x;
        float dy = pull_target_y - n.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 1.2f) {
            float pull_mag = pull_strength * 5.2f;
            n.vx += (dx / dist) * pull_mag;
            n.vy += (dy / dist) * pull_mag;
        }
    }

    float damp = SPRING_DAMPING - tension * 0.10f;
    if (sticky_clog_timer > 0.0f) {
        damp *= 0.82f; // 黏性高阻尼
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
        if (has_pull_target) {
            rest *= 1.35f;
        }

        if (dist > 0.01f) {
            float delta = dist - rest;
            float force = delta * (SPRING_STIFFNESS + tension * 0.20f);

            float nx = dx / dist;
            float ny = dy / dist;

            curr.vx -= (nx * force) / curr.mass;
            curr.vy -= (ny * force) / curr.mass;

            float back_pull = (has_pull_target || sticky_clog_timer > 0.0f) ? 0.02f : 0.05f;
            prev.vx += (nx * force * back_pull) / prev.mass;
            prev.vy += (ny * force * back_pull) / prev.mass;

            float max_allowed_dist = rest * 2.1f;
            if (dist > max_allowed_dist) {
                curr.x = prev.x + nx * max_allowed_dist;
                curr.y = prev.y + ny * max_allowed_dist;
            }
        }
    }
}

void SkeletonSystem::update(float dt, float gravity_x, float gravity_y,
                            float crawl_force_x, float crawl_force_y,
                            float neuro_tension, float spike_intensity,
                            float respiration, bool is_upside_down) {
    if (sticky_clog_timer > 0.0f) {
        sticky_clog_timer -= dt;
        if (sticky_clog_timer < 0.0f) sticky_clog_timer = 0.0f;
    }

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        updateNodePhysics(i, dt, gravity_x, gravity_y, crawl_force_x, crawl_force_y, neuro_tension, is_upside_down);
    }

    solveSpringConstraints(neuro_tension);
    solveSpringConstraints(neuro_tension);

    // 【黏性玩具极端贴边拍扁形变算法 (Extreme Pancake Splatting)】
    float stick_boost = (sticky_clog_timer > 0.0f) ? (1.0f + (sticky_clog_timer / 0.90f) * 1.6f) : 1.0f;

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &n = nodes[i];
        float r = n.base_radius * (1.0f + respiration);

        if (has_pull_target) {
            r *= 0.88f;
        }

        float contact_y = std::max(n.contact_bottom, n.contact_top);
        float contact_x = std::max(n.contact_left, n.contact_right);

        float flat_x = 1.0f;
        float flat_y = 1.0f;

        // 贴上下边：厚度压缩至 0.28x，横向狂暴展宽至 2.65x
        if (contact_y > 0.02f) {
            float eff_cy = std::min(1.0f, contact_y * stick_boost);
            flat_y -= eff_cy * 0.72f;
            flat_x += eff_cy * 1.65f;
        }

        // 贴左右边：厚度压缩至 0.28x，纵向狂暴展宽至 2.65x
        if (contact_x > 0.02f) {
            float eff_cx = std::min(1.0f, contact_x * stick_boost);
            flat_x -= eff_cx * 0.72f;
            flat_y += eff_cx * 1.65f;
        }

        float v_speed = std::sqrt(n.vx * n.vx + n.vy * n.vy);
        if (v_speed > 0.4f) {
            float stretch = std::min(1.4f, 1.0f + v_speed * 0.08f);
            flat_x *= stretch;
            flat_y /= std::sqrt(stretch);
        }

        n.radius_x = r * flat_x;
        n.radius_y = r * flat_y;
    }
}
