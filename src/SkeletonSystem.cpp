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
        // 初始顺着底部水平修长排布
        nodes[i].x = start_x - i * 13.0f;
        nodes[i].y = start_y;
        nodes[i].vx = 0.0f;
        nodes[i].vy = 0.0f;

        // 头部大、身体流线型渐细
        if (i == 0)      nodes[i].base_radius = 22.0f;
        else if (i == 1) nodes[i].base_radius = 19.0f;
        else if (i == 2) nodes[i].base_radius = 16.0f;
        else if (i == 3) nodes[i].base_radius = 12.0f;
        else             nodes[i].base_radius = 8.5f;

        nodes[i].radius_x = nodes[i].base_radius;
        nodes[i].radius_y = nodes[i].base_radius;
        nodes[i].mass = 0.8f + (float)i * 0.30f;

        nodes[i].contact_bottom = 0.0f;
        nodes[i].contact_top = 0.0f;
        nodes[i].contact_left = 0.0f;
        nodes[i].contact_right = 0.0f;
        nodes[i].bleb_offset_x = 0.0f;
        nodes[i].bleb_offset_y = 0.0f;
    }

    // 脊柱静息间距增大至 13px，保证长条形身形
    for (int i = 0; i < SKELETON_NODE_COUNT - 1; ++i) {
        rest_lengths[i] = 13.0f;
    }

    has_pull_target = false;
    pull_strength = 0.0f;
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

    // 1. 底部地面接触
    float dist_b = (SCREEN_H - 1) - n.y;
    if (dist_b < r) {
        float penetration = r - dist_b;
        n.y -= penetration * 0.88f;
        n.vy *= 0.10f;
        n.vx *= 0.80f;
        n.contact_bottom = std::min(1.0f, penetration / (r * 0.40f));
    }

    // 2. 顶部天花板接触
    float dist_t = n.y - 1;
    if (dist_t < r) {
        float penetration = r - dist_t;
        n.y += penetration * 0.88f;
        n.vy *= 0.10f;
        n.vx *= 0.80f;
        n.contact_top = std::min(1.0f, penetration / (r * 0.40f));
    }

    // 3. 左壁接触
    float dist_l = n.x - 1;
    if (dist_l < r) {
        float penetration = r - dist_l;
        n.x += penetration * 0.88f;
        n.vx *= 0.10f;
        n.vy *= 0.80f;
        n.contact_left = std::min(1.0f, penetration / (r * 0.40f));
    }

    // 4. 右壁接触
    float dist_r = (SCREEN_W - 1) - n.x;
    if (dist_r < r) {
        float penetration = r - dist_r;
        n.x -= penetration * 0.88f;
        n.vx *= 0.10f;
        n.vy *= 0.80f;
        n.contact_right = std::min(1.0f, penetration / (r * 0.40f));
    }
}

void SkeletonSystem::updateNodePhysics(int i, float dt, float gx, float gy, float cfx, float cfy, float tension, bool is_upside_down) {
    SkeletonNode &n = nodes[i];

    float g_scale = is_upside_down ? (0.25f - tension * 0.15f) : 1.0f;
    n.vx += gx * g_scale * 0.45f;
    n.vy += gy * g_scale * 0.45f;

    n.vx += cfx * (0.8f / n.mass);
    n.vy += cfy * (0.8f / n.mass);

    if (has_pull_target && i == 0) {
        float dx = pull_target_x - n.x;
        float dy = pull_target_y - n.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 1.5f) {
            float pull_mag = pull_strength * 4.5f;
            n.vx += (dx / dist) * pull_mag;
            n.vy += (dy / dist) * pull_mag;
        }
    }

    float damp = SPRING_DAMPING - tension * 0.10f;
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
            rest *= 1.40f; // 受到牵引时身段进一步拉长
        }

        if (dist > 0.01f) {
            float delta = dist - rest;
            float force = delta * (SPRING_STIFFNESS + tension * 0.20f);

            float nx = dx / dist;
            float ny = dy / dist;

            curr.vx -= (nx * force) / curr.mass;
            curr.vy -= (ny * force) / curr.mass;

            prev.vx += (nx * force * 0.35f) / prev.mass;
            prev.vy += (ny * force * 0.35f) / prev.mass;

            float max_allowed_dist = rest * 2.0f;
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
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        updateNodePhysics(i, dt, gravity_x, gravity_y, crawl_force_x, crawl_force_y, neuro_tension, is_upside_down);
    }

    solveSpringConstraints(neuro_tension);
    solveSpringConstraints(neuro_tension);

    // 强力贴壁各向异性重力压扁与横向拉长长条形形变
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &n = nodes[i];
        float r = n.base_radius * (1.0f + respiration);

        if (has_pull_target) {
            r *= 0.88f;
        }

        float contact_y = std::max(n.contact_bottom, n.contact_top);
        float contact_x = std::max(n.contact_left, n.contact_right);

        // 基础形状比例
        float flat_x = 1.0f;
        float flat_y = 1.0f;

        // 1. 底部/顶部重力压扁 -> 垂直高度压扁近 60%，水平宽度大幅横向伸展 85%（形成长条形）
        if (contact_y > 0.05f) {
            flat_y -= contact_y * 0.58f;
            flat_x += contact_y * 0.85f;
        }

        // 2. 左右侧壁重力压扁 -> 水平侧向压扁，垂直高度纵向拉长
        if (contact_x > 0.05f) {
            flat_x -= contact_x * 0.58f;
            flat_y += contact_x * 0.85f;
        }

        // 3. 速度运动拉伸（移动时呈修长水滴流线型）
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
