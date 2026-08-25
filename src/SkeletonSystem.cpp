#include "SkeletonSystem.h"
#include <cmath>

SkeletonSystem::SkeletonSystem() {
    for (int i = 0; i < SKELETON_NODE_COUNT - 1; ++i) {
        rest_lengths[i] = 16.0f;
    }
}

void SkeletonSystem::init() {
    reset(SCREEN_W * 0.5f, SCREEN_H - 30.0f);
}

void SkeletonSystem::reset(float cx, float cy) {
    const float radii[SKELETON_NODE_COUNT] = { 22.0f, 24.0f, 22.0f, 18.0f, 13.0f };

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        nodes[i].x = cx - i * 14.0f;
        nodes[i].y = cy;
        nodes[i].vx = 0.0f;
        nodes[i].vy = 0.0f;
        nodes[i].base_radius = radii[i];
        nodes[i].radius_x = radii[i];
        nodes[i].radius_y = radii[i];
        nodes[i].bleb_offset_x = 0.0f;
        nodes[i].bleb_offset_y = 0.0f;
        nodes[i].spike_amount = 0.0f;
        nodes[i].contact_left = 0.0f;
        nodes[i].contact_right = 0.0f;
        nodes[i].contact_top = 0.0f;
        nodes[i].contact_bottom = 0.0f;
        nodes[i].is_head = (i == 0);
        nodes[i].is_tail = (i == SKELETON_NODE_COUNT - 1);
    }
}

void SkeletonSystem::getCenterPos(float &cx, float &cy) const {
    float sum_x = 0.0f, sum_y = 0.0f;
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        sum_x += (nodes[i].x + nodes[i].bleb_offset_x);
        sum_y += (nodes[i].y + nodes[i].bleb_offset_y);
    }
    cx = sum_x / SKELETON_NODE_COUNT;
    cy = sum_y / SKELETON_NODE_COUNT;
}

void SkeletonSystem::applyImpulse(float fx, float fy) {
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        float factor = 1.0f - (i * 0.12f);
        nodes[i].vx += fx * factor;
        nodes[i].vy += fy * factor;
    }
}

void SkeletonSystem::triggerLocalBleb(int node_idx, float intensity) {
    if (node_idx >= 0 && node_idx < SKELETON_NODE_COUNT) {
        float angle = (rand() % 360) * 0.017453f;
        nodes[node_idx].bleb_offset_x += std::cos(angle) * intensity * 5.0f;
        nodes[node_idx].bleb_offset_y += std::sin(angle) * intensity * 5.0f;
    }
}

void SkeletonSystem::applySpringForces(float tension) {
    // 刚度受情绪张力调节：Tension 高时紧绷，低时柔顺
    float current_k = SPRING_STIFFNESS * (0.7f + tension * 0.9f);

    for (int i = 0; i < SKELETON_NODE_COUNT - 1; ++i) {
        SkeletonNode &n1 = nodes[i];
        SkeletonNode &n2 = nodes[i + 1];

        float dx = n2.x - n1.x;
        float dy = n2.y - n1.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 0.001f) {
            dx = 1.0f;
            dist = 1.0f;
        }

        float target_d = rest_lengths[i] * (1.0f - tension * 0.15f); // 紧张时身体整体收紧缩小
        float delta = dist - target_d;
        float force = delta * current_k;

        float nx = dx / dist;
        float ny = dy / dist;

        n1.vx += nx * force * 0.5f;
        n1.vy += ny * force * 0.5f;
        n2.vx -= nx * force * 0.5f;
        n2.vy -= ny * force * 0.5f;
    }

    for (int i = 0; i < SKELETON_NODE_COUNT - 2; ++i) {
        SkeletonNode &n1 = nodes[i];
        SkeletonNode &n3 = nodes[i + 2];

        float dx = n3.x - n1.x;
        float dy = n3.y - n1.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float target_d = (rest_lengths[i] + rest_lengths[i + 1]) * 0.85f;

        if (dist < target_d && dist > 0.001f) {
            float force = (dist - target_d) * (current_k * 0.35f);
            float nx = dx / dist;
            float ny = dy / dist;
            n1.vx += nx * force * 0.5f;
            n1.vy += ny * force * 0.5f;
            n3.vx -= nx * force * 0.5f;
            n3.vy -= ny * force * 0.5f;
        }
    }
}

void SkeletonSystem::applyBoundaryAndAdhesion(float gravity_x, float gravity_y, bool is_upside_down) {
    total_wall_contact = 0.0f;

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &n = nodes[i];
        float r = n.base_radius;

        n.contact_left = 0.0f;
        n.contact_right = 0.0f;
        n.contact_top = 0.0f;
        n.contact_bottom = 0.0f;

        // 1. 底部吸附与碰撞 (Bottom)
        float dist_bottom = SCREEN_H - n.y;
        if (dist_bottom < r + WALL_STICK_DIST) {
            float contact = 1.0f - (dist_bottom / (r + WALL_STICK_DIST));
            if (contact < 0.0f) contact = 0.0f;
            n.contact_bottom = contact;
            n.vy += WALL_STICK_FORCE * (1.0f + gravity_y);
            n.vx *= (1.0f - contact * 0.3f);
        }
        if (n.y > SCREEN_H - r * 0.65f) {
            n.y = SCREEN_H - r * 0.65f;
            if (n.vy > 0.0f) n.vy = -n.vy * 0.1f;
        }

        // 2. 顶部吸附与抗坠落 (Top / Upside-down Adhesion)
        float dist_top = n.y;
        float top_stick_margin = is_upside_down ? (WALL_STICK_DIST * 1.5f) : WALL_STICK_DIST;
        if (dist_top < r + top_stick_margin) {
            float contact = 1.0f - (dist_top / (r + top_stick_margin));
            if (contact < 0.0f) contact = 0.0f;
            n.contact_top = contact;
            // 倒置时强力吸附在天花板上，抗坠落
            float stick_multiplier = is_upside_down ? 2.2f : 1.0f;
            n.vy -= WALL_STICK_FORCE * stick_multiplier * (1.0f - gravity_y);
            n.vx *= (1.0f - contact * 0.4f);
        }
        if (n.y < r * 0.65f) {
            n.y = r * 0.65f;
            if (n.vy < 0.0f) n.vy = -n.vy * 0.1f;
        }

        // 3. 左壁吸附与碰撞 (Left)
        float dist_left = n.x;
        if (dist_left < r + WALL_STICK_DIST) {
            float contact = 1.0f - (dist_left / (r + WALL_STICK_DIST));
            if (contact < 0.0f) contact = 0.0f;
            n.contact_left = contact;
            n.vx -= WALL_STICK_FORCE * (1.0f - gravity_x);
            n.vy *= (1.0f - contact * 0.3f);
        }
        if (n.x < r * 0.65f) {
            n.x = r * 0.65f;
            if (n.vx < 0.0f) n.vx = -n.vx * 0.1f;
        }

        // 4. 右壁吸附与碰撞 (Right)
        float dist_right = SCREEN_W - n.x;
        if (dist_right < r + WALL_STICK_DIST) {
            float contact = 1.0f - (dist_right / (r + WALL_STICK_DIST));
            if (contact < 0.0f) contact = 0.0f;
            n.contact_right = contact;
            n.vx += WALL_STICK_FORCE * (1.0f + gravity_x);
            n.vy *= (1.0f - contact * 0.3f);
        }
        if (n.x > SCREEN_W - r * 0.65f) {
            n.x = SCREEN_W - r * 0.65f;
            if (n.vx > 0.0f) n.vx = -n.vx * 0.1f;
        }

        float c_sum = n.contact_left + n.contact_right + n.contact_top + n.contact_bottom;
        total_wall_contact += (c_sum > 1.0f ? 1.0f : c_sum);
    }

    total_wall_contact /= SKELETON_NODE_COUNT;
}

void SkeletonSystem::updateDeformations(float respiration, float tension, float spike_intensity, float dt) {
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &n = nodes[i];

        // 衰减局部鼓包
        n.bleb_offset_x *= 0.85f;
        n.bleb_offset_y *= 0.85f;

        // 尖刺强度平滑
        n.spike_amount = n.spike_amount * 0.80f + spike_intensity * 0.20f;

        float r_base = n.base_radius * (1.0f + respiration - tension * 0.10f + n.spike_amount * 0.25f);

        float vertical_contact = n.contact_bottom + n.contact_top;
        if (vertical_contact > 1.0f) vertical_contact = 1.0f;

        float horizontal_contact = n.contact_left + n.contact_right;
        if (horizontal_contact > 1.0f) horizontal_contact = 1.0f;

        float corner_factor = (vertical_contact * horizontal_contact);

        float rx = r_base;
        float ry = r_base;

        if (vertical_contact > 0.01f) {
            float squash_y = 1.0f - vertical_contact * WALL_FLATTEN_RATE;
            float expand_x = 1.0f + vertical_contact * (WALL_FLATTEN_RATE * 0.85f);
            ry *= squash_y;
            rx *= expand_x;
        }

        if (horizontal_contact > 0.01f) {
            float squash_x = 1.0f - horizontal_contact * WALL_FLATTEN_RATE;
            float expand_y = 1.0f + horizontal_contact * (WALL_FLATTEN_RATE * 0.85f);
            rx *= squash_x;
            ry *= expand_y;
        }

        if (corner_factor > 0.05f) {
            rx *= (1.0f + corner_factor * 0.35f);
            ry *= (1.0f + corner_factor * 0.35f);
        }

        n.radius_x = n.radius_x * 0.7f + rx * 0.3f;
        n.radius_y = n.radius_y * 0.7f + ry * 0.3f;
    }
}

void SkeletonSystem::update(float dt, float gravity_x, float gravity_y, float crawl_bias_x, float crawl_bias_y,
                            float tension, float spike_intensity, float respiration, bool is_upside_down) {
    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        SkeletonNode &n = nodes[i];

        n.vx += gravity_x;
        n.vy += gravity_y;

        if (i == 0) {
            n.vx += crawl_bias_x * (1.6f - tension * 0.4f);
            n.vy += crawl_bias_y * (1.6f - tension * 0.4f);
        } else {
            float falloff = 1.0f - (i * 0.2f);
            n.vx += crawl_bias_x * 0.4f * falloff;
            n.vy += crawl_bias_y * 0.4f * falloff;
        }

        // 紧张张力下的高频微颤
        if (tension > 0.3f) {
            float j_amp = tension * 0.12f;
            n.vx += ((rand() % 100) - 50) * j_amp;
            n.vy += ((rand() % 100) - 50) * j_amp;
        }

        n.vx *= SPRING_DAMPING;
        n.vy *= SPRING_DAMPING;
    }

    applySpringForces(tension);
    applyBoundaryAndAdhesion(gravity_x, gravity_y, is_upside_down);

    for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
        nodes[i].x += nodes[i].vx * dt;
        nodes[i].y += nodes[i].vy * dt;
    }

    updateDeformations(respiration, tension, spike_intensity, dt);
}
