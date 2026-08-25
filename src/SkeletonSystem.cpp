#include "SkeletonSystem.h"
#include <cmath>
#include <algorithm>

SkeletonSystem::SkeletonSystem() {
    init();
}

void SkeletonSystem::init() {
    float start_x = 120.0f;
    float start_y = 100.0f;

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        nodes[i].x = start_x;
        nodes[i].y = start_y + i * 4.5f;
        nodes[i].vx = 0.0f;
        nodes[i].vy = 0.0f;

        // 头部大、身体圆润、尾部渐细
        if (i == 0)      nodes[i].base_radius = 24.0f;
        else if (i == 1) nodes[i].base_radius = 21.0f;
        else if (i == 2) nodes[i].base_radius = 18.0f;
        else if (i == 3) nodes[i].base_radius = 14.0f;
        else             nodes[i].base_radius = 9.0f;

        nodes[i].radius_x = nodes[i].base_radius;
        nodes[i].radius_y = nodes[i].base_radius;
        nodes[i].mass = 0.8f + (float)i * 0.35f; // 头部轻、尾部重，拖尾感更强

        nodes[i].contact_bottom = 0.0f;
        nodes[i].contact_top = 0.0f;
        nodes[i].contact_left = 0.0f;
        nodes[i].contact_right = 0.0f;
        nodes[i].bleb_offset_x = 0.0f;
        nodes[i].bleb_offset_y = 0.0f;
    }

    for (int i = 0; i < SKELETON_NODE_COUNT - 1; ++i) {
        rest_lengths[i] = 4.0f;
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
    float dist = (5.0f + (rand() % 25) * 0.1f) * intensity;
    nodes[node_index].bleb_offset_x = std::cos(angle) * dist;
    nodes[node_index].bleb_offset_y = std::sin(angle) * dist;
}

bool SkeletonSystem::isAttachedToWall() const {
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        if (nodes[i].contact_bottom > 0.3f || nodes[i].contact_top > 0.3f ||
            nodes[i].contact_left > 0.3f || nodes[i].contact_right > 0.3f) {
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
    float dist_b = (SCREEN_H - 2) - n.y;
    if (dist_b < r) {
        float penetration = r - dist_b;
        n.y -= penetration * 0.85f;
        n.vy *= 0.15f;
        n.vx *= 0.70f;
        n.contact_bottom = std::min(1.0f, penetration / (r * 0.45f));
    }

    // 2. 顶部天花板接触
    float dist_t = n.y - 2;
    if (dist_t < r) {
        float penetration = r - dist_t;
        n.y += penetration * 0.85f;
        n.vy *= 0.15f;
        n.vx *= 0.70f;
        n.contact_top = std::min(1.0f, penetration / (r * 0.45f));
    }

    // 3. 左壁接触
    float dist_l = n.x - 2;
    if (dist_l < r) {
        float penetration = r - dist_l;
        n.x += penetration * 0.85f;
        n.vx *= 0.15f;
        n.vy *= 0.70f;
        n.contact_left = std::min(1.0f, penetration / (r * 0.45f));
    }

    // 4. 右壁接触
    float dist_r = (SCREEN_W - 2) - n.x;
    if (dist_r < r) {
        float penetration = r - dist_r;
        n.x -= penetration * 0.85f;
        n.vx *= 0.15f;
        n.vy *= 0.70f;
        n.contact_right = std::min(1.0f, penetration / (r * 0.45f));
    }
}

void SkeletonSystem::updateNodePhysics(int i, float dt, float gx, float gy, float cfx, float cfy, float tension, bool is_upside_down) {
    SkeletonNode &n = nodes[i];

    // 1. 重力与外界加速度
    float g_scale = is_upside_down ? (0.25f - tension * 0.15f) : 1.0f;
    n.vx += gx * g_scale * 0.45f;
    n.vy += gy * g_scale * 0.45f;

    // 2. 爬行爬进驱动力
    n.vx += cfx * (0.8f / n.mass);
    n.vy += cfy * (0.8f / n.mass);

    // 3. 主动抓取触手牵引力 (强力将头部拉向掌心，产生质心前移效果)
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

    // 阻尼
    float damp = SPRING_DAMPING - tension * 0.12f;
    n.vx *= damp;
    n.vy *= damp;

    n.x += n.vx * dt * 30.0f;
    n.y += n.vy * dt * 30.0f;

    // 神经鼓包衰减
    n.bleb_offset_x *= 0.88f;
    n.bleb_offset_y *= 0.88f;

    // 贴壁接触与防穿透
    applyWallAdhesion(i);
}

void SkeletonSystem::solveSpringConstraints(float tension) {
    // 弹簧链约束（弹簧-阻尼系统 + 悬链线下坠）
    for (int i = 1; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &prev = nodes[i - 1];
        SkeletonNode &curr = nodes[i];

        float dx = curr.x - prev.x;
        float dy = curr.y - prev.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        float rest = rest_lengths[i - 1];
        if (has_pull_target) {
            // 受拉伸时静息距离适当延展，产生拖尾变细与拉伸感
            rest *= 1.35f;
        }

        if (dist > 0.01f) {
            float delta = dist - rest;
            float force = delta * (SPRING_STIFFNESS + tension * 0.25f);

            float nx = dx / dist;
            float ny = dy / dist;

            // 头部质量轻、容易拉动身体
            curr.vx -= (nx * force) / curr.mass;
            curr.vy -= (ny * force) / curr.mass;

            prev.vx += (nx * force * 0.4f) / prev.mass;
            prev.vy += (ny * force * 0.4f) / prev.mass;

            // 刚性位置限制 (PBD 刚性截断，拒绝面条状无界拉长)
            float max_allowed_dist = rest * 2.2f;
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

    // 迭代求解两次弹簧约束，保证质心传递稳固
    solveSpringConstraints(neuro_tension);
    solveSpringConstraints(neuro_tension);

    // 动态计算每个节点的压扁形变与呼吸微缩放
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &n = nodes[i];
        float r = n.base_radius * (1.0f + respiration);

        // 受到抓取拉伸时，身体产生体积守恒性拉长变细
        if (has_pull_target) {
            r *= 0.90f;
        }

        float flat_y = 1.0f - (n.contact_bottom + n.contact_top) * 0.45f;
        float flat_x = 1.0f - (n.contact_left + n.contact_right) * 0.45f;

        // 体积守恒横向膨胀
        if (flat_y < 1.0f) flat_x += (1.0f - flat_y) * 0.55f;
        if (flat_x < 1.0f) flat_y += (1.0f - flat_x) * 0.55f;

        n.radius_x = r * flat_x;
        n.radius_y = r * flat_y;
    }
}
