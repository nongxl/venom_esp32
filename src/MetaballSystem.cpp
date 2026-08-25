#include "MetaballSystem.h"
#include <cstring>
#include <cmath>

MetaballSystem::MetaballSystem() {
    memset(field_buffer, 0, sizeof(field_buffer));
    for (int i = 0; i < MAX_DROPLETS; ++i) droplets[i].active = false;
}

void MetaballSystem::init() {
    memset(field_buffer, 0, sizeof(field_buffer));
    for (int i = 0; i < MAX_DROPLETS; ++i) droplets[i].active = false;
    spike_time_phase = 0.0f;
}

void MetaballSystem::spawnDroplet(float x, float y, float vx, float vy, float r, bool is_jolt) {
    for (int i = 0; i < MAX_DROPLETS; ++i) {
        if (!droplets[i].active) {
            droplets[i].active = true;
            droplets[i].x = x;
            droplets[i].y = y;
            droplets[i].vx = vx;
            droplets[i].vy = vy;
            droplets[i].radius = r;
            droplets[i].life = 1.0f;
            droplets[i].is_jolt_spurt = is_jolt;
            break;
        }
    }
}

void MetaballSystem::triggerJoltSpurt(const SkeletonSystem &skeleton, float intensity) {
    float cx, cy;
    skeleton.getCenterPos(cx, cy);

    int count = 3 + (rand() % 2);
    for (int i = 0; i < count; ++i) {
        float angle = (i * (360.0f / count) + (rand() % 30)) * 0.017453f;
        float speed = (2.5f + (rand() % 20) * 0.1f) * intensity;
        float r = 2.5f + (rand() % 12) * 0.1f;
        spawnDroplet(cx + std::cos(angle) * 10.0f, cy + std::sin(angle) * 10.0f,
                     std::cos(angle) * speed, std::sin(angle) * speed - 1.0f, r, true);
    }
}

void MetaballSystem::updateDroplets(float dt, const SkeletonSystem &skeleton, float gx, float gy, const PhysiologySystem &physiology) {
    float cx, cy;
    skeleton.getCenterPos(cx, cy);
    float tension = physiology.getNeuroTension();

    for (int i = 0; i < MAX_DROPLETS; ++i) {
        if (!droplets[i].active) continue;

        Droplet &d = droplets[i];
        d.vx += gx * 0.7f;
        d.vy += gy * 0.7f;

        float dx = cx - d.x;
        float dy = cy - d.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist > 1.0f) {
            float attract = d.is_jolt_spurt ? (0.12f + tension * 0.10f) : 0.08f;
            d.vx += (dx / dist) * attract * (dist * 0.08f);
            d.vy += (dy / dist) * attract * (dist * 0.08f);
        }

        d.vx *= 0.91f;
        d.vy *= 0.91f;

        d.x += d.vx * dt;
        d.y += d.vy * dt;

        if (d.x < 3.0f) { d.x = 3.0f; d.vx = -d.vx * 0.3f; }
        if (d.x > SCREEN_W - 3.0f) { d.x = SCREEN_W - 3.0f; d.vx = -d.vx * 0.3f; }
        if (d.y < 3.0f) { d.y = 3.0f; d.vy = -d.vy * 0.3f; }
        if (d.y > SCREEN_H - 3.0f) { d.y = SCREEN_H - 3.0f; d.vy = -d.vy * 0.3f; }

        d.life -= dt * 0.035f;
        if (dist < 14.0f || d.life <= 0.0f) {
            d.active = false;
        }
    }

    if (physiology.getAudioHigh() > 0.60f) {
        auto_droplet_timer += dt;
        if (auto_droplet_timer > 0.25f) {
            auto_droplet_timer = 0.0f;
            float hx, hy;
            skeleton.getHeadPos(hx, hy);
            float angle = (rand() % 360) * 0.017453f;
            float speed = 2.0f + (rand() % 15) * 0.1f;
            spawnDroplet(hx, hy, std::cos(angle) * speed, std::sin(angle) * speed, 2.2f);
        }
    }
}

void MetaballSystem::update(float dt, const SkeletonSystem &skeleton, float gravity_x, float gravity_y, const PhysiologySystem &physiology) {
    spike_time_phase += dt * (3.0f + physiology.getNeuroTension() * 6.0f);
    updateDroplets(dt, skeleton, gravity_x, gravity_y, physiology);
}

void MetaballSystem::addMetaballToField(float cx, float cy, float rx, float ry, uint8_t intensity,
                                       float contact_b, float contact_t, float contact_l, float contact_r,
                                       float micro_spike_amp, float spike_phase) {
    float gcx = cx / (float)GRID_SCALE;
    float gcy = cy / (float)GRID_SCALE;
    float grx = rx / (float)GRID_SCALE;
    float gry = ry / (float)GRID_SCALE;

    if (grx < 1.0f) grx = 1.0f;
    if (gry < 1.0f) gry = 1.0f;

    // 严密紧凑的包围盒（主体形状保持紧致稳定）
    int min_gx = std::max(0, (int)std::floor(gcx - grx * 1.85f));
    int max_gx = std::min(GRID_W - 1, (int)std::ceil(gcx + grx * 1.85f));
    int min_gy = std::max(0, (int)std::floor(gcy - gry * 1.85f));
    int max_gy = std::min(GRID_H - 1, (int)std::ceil(gcy + gry * 1.85f));

    for (int gy = min_gy; gy <= max_gy; ++gy) {
        float dy = (float)gy - gcy;
        int row_offset = gy * GRID_W;

        for (int gx = min_gx; gx <= max_gx; ++gx) {
            float dx = (float)gx - gcx;

            float cur_grx = grx;
            float cur_gry = gry;

            // 紧绷高密度微尖刺调制（16 阶与 24 阶高频紧密刺牙，突出仅 1~2.5px，绝无大范围畸变）
            if (micro_spike_amp > 0.01f) {
                float theta = std::atan2(dy, dx);
                float spike_mod = 1.0f + micro_spike_amp * (0.60f * std::sin(16.0f * theta + spike_phase) +
                                                            0.40f * std::cos(24.0f * theta - spike_phase * 1.3f));
                cur_grx *= spike_mod;
                cur_gry *= spike_mod;
            }

            float inv_grx2 = 1.0f / (cur_grx * cur_grx * 3.24f);
            float inv_gry2 = 1.0f / (cur_gry * cur_gry * 3.24f);
            float d_norm2 = dx * dx * inv_grx2 + dy * dy * inv_gry2;

            if (d_norm2 < 1.0f) {
                float val = 1.0f - d_norm2;
                float contrib = val * val * (float)intensity;

                // 贴边接触面整形增强
                if (contact_b > 0.2f && gy >= GRID_H - 2) {
                    float edge_boost = (gy == GRID_H - 1) ? 1.35f : 1.15f;
                    contrib *= edge_boost * (1.0f + contact_b * 0.35f);
                }
                if (contact_t > 0.2f && gy <= 1) {
                    float edge_boost = (gy == 0) ? 1.35f : 1.15f;
                    contrib *= edge_boost * (1.0f + contact_t * 0.35f);
                }
                if (contact_l > 0.2f && gx <= 1) {
                    float edge_boost = (gx == 0) ? 1.35f : 1.15f;
                    contrib *= edge_boost * (1.0f + contact_l * 0.35f);
                }
                if (contact_r > 0.2f && gx >= GRID_W - 2) {
                    float edge_boost = (gx == GRID_W - 1) ? 1.35f : 1.15f;
                    contrib *= edge_boost * (1.0f + contact_r * 0.35f);
                }

                int current = field_buffer[row_offset + gx] + (int)contrib;
                field_buffer[row_offset + gx] = (current > 255) ? 255 : (uint8_t)current;
            }
        }
    }
}

void MetaballSystem::computeField(const SkeletonSystem &skeleton, const PhysiologySystem &physiology) {
    memset(field_buffer, 0, sizeof(field_buffer));

    // 紧绷微尖刺幅度：平静时 0.04 (微小紧绷)，紧张/高频声音时 0.08~0.12 (清晰锐刺)
    float tension = physiology.getNeuroTension();
    float spike_int = physiology.getSpikeIntensity();
    EmotionState emotion = physiology.getEmotion();

    float micro_spike_amp = 0.04f + tension * 0.05f + spike_int * 0.06f;
    if (emotion == EMOTION_ANGER) {
        micro_spike_amp += 0.04f;
    }

    int node_count = skeleton.getNodeCount();
    for (int i = 0; i < node_count; ++i) {
        const SkeletonNode &n = skeleton.getNode(i);
        uint8_t strength = (i == 0) ? 190 : (i == node_count - 1 ? 145 : 170);
        float node_phase = spike_time_phase + i * 0.7f;

        addMetaballToField(n.x + n.bleb_offset_x, n.y + n.bleb_offset_y,
                           n.radius_x, n.radius_y, strength,
                           n.contact_bottom, n.contact_top, n.contact_left, n.contact_right,
                           micro_spike_amp, node_phase);
    }

    for (int i = 0; i < MAX_DROPLETS; ++i) {
        if (droplets[i].active) {
            addMetaballToField(droplets[i].x, droplets[i].y, droplets[i].radius, droplets[i].radius, 120, 0, 0, 0, 0, 0, 0);
        }
    }
}
