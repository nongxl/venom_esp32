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
        nodes[i].mass = 0.7f + (float)i * 0.25f; // 头部轻快、尾部顺滑跟随

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

    // 1. 底部地面接触与防穿透
    float dist_b = (SCREEN_H - 1) - n.y;
    if (dist_b < r) {
        float penetration = r - dist_b;
        n.y -= penetration * 0.88f;
        if (n.vy > 0.0f) n.vy *= 0.10f;
        n.vx *= 0.82f;
        n.contact_bottom = std::min(1.0f, penetration / (r * 0.40f));
    }

    // 2. 顶部天花板接触
    float dist_t = n.y - 1;
    if (dist_t < r) {
        float penetration = r - dist_t;
        n.y += penetration * 0.88f;
        if (n.vy < 0.0f) n.vy *= 0.10f;
        n.vx *= 0.82f;
        n.contact_top = std::min(1.0f, penetration / (r * 0.40f));
    }

    // 3. 左壁接触
    float dist_l = n.x - 1;
    if (dist_l < r) {
        float penetration = r - dist_l;
        n.x += penetration * 0.88f;
        if (n.vx < 0.0f) n.vx *= 0.10f;
        n.vy *= 0.82f;
        n.contact_left = std::min(1.0f, penetration / (r * 0.40f));
    }

    // 4. 右壁接触
    float dist_r = (SCREEN_W - 1) - n.x;
    if (dist_r < r) {
        float penetration = r - dist_r;
        n.x -= penetration * 0.88f;
        if (n.vx > 0.0f) n.vx *= 0.10f;
        n.vy *= 0.82f;
        n.contact_right = std::min(1.0f, penetration / (r * 0.40f));
    }
}

void SkeletonSystem::updateNodePhysics(int i, float dt, float gx, float gy, float cfx, float cfy, float tension, bool is_upside_down) {
    SkeletonNode &n = nodes[i];

    // 共生体主动抗重力肌张力 (Antigravity Muscle Resistance):
    // 活体肌肉纤维产生主动支撑力抵消大部分重力下坠，呈现出质心抵抗引力的生命力
    float muscle_resistance = 0.35f + tension * 0.45f;
    if (has_pull_target) {
        muscle_resistance = 0.85f; // 正在爬行攀登时几乎完全抵消下坠重力
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

    // 主动抓取牵引：头部受到直接指向目标的强力加速度
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
        SkeletonNode &prev = nodes[i - 1]; // 前方节点（朝向头部）
        SkeletonNode &curr = nodes[i];     // 后方节点（朝向尾部）

        float dx = curr.x - prev.x;        // 从 prev 指向 curr (指向后方)
        float dy = curr.y - prev.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        float rest = rest_lengths[i - 1];
        if (has_pull_target) {
            rest *= 1.35f;
        }

        if (dist > 0.01f) {
            float delta = dist - rest;
            float force = delta * (SPRING_STIFFNESS + tension * 0.20f);

            float nx = dx / dist; // 指向后方单位矢量
            float ny = dy / dist;

            // 1. 后方节点被前方节点向前强力拖动 (前拉力充足)
            curr.vx -= (nx * force) / curr.mass;
            curr.vy -= (ny * force) / curr.mass;

            // 2. 【关键物理修复】后方节点对前方节点的回拉阻力严格限制为极低阻尼 (0.02f)，
            // 彻底切断把正在探索的头部“倒吸”回角落的错误回拉！
            float back_pull = has_pull_target ? 0.02f : 0.05f;
            prev.vx += (nx * force * back_pull) / prev.mass;
            prev.vy += (ny * force * back_pull) / prev.mass;

            // 3. 刚性位置限制 (PBD 刚性约束)
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
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        updateNodePhysics(i, dt, gravity_x, gravity_y, crawl_force_x, crawl_force_y, neuro_tension, is_upside_down);
    }

    // 迭代求解弹簧约束
    solveSpringConstraints(neuro_tension);
    solveSpringConstraints(neuro_tension);

    // 动态计算各向异性贴壁与运动拉伸
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

        if (contact_y > 0.05f) {
            flat_y -= contact_y * 0.58f;
            flat_x += contact_y * 0.85f;
        }

        if (contact_x > 0.05f) {
            flat_x -= contact_x * 0.58f;
            flat_y += contact_x * 0.85f;
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
