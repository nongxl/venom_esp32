#include "MetaballSystem.h"
#include <cstring>
#include <cmath>
#include <algorithm>

MetaballSystem::MetaballSystem() {
    memset(field_buffer, 0, sizeof(field_buffer));
    for (int i = 0; i < MAX_DROPLETS; ++i) droplets[i].active = false;
    for (int i = 0; i < MAX_SPIKE_ERUPTIONS; ++i) spikes[i].active = false;
}

void MetaballSystem::init() {
    memset(field_buffer, 0, sizeof(field_buffer));
    for (int i = 0; i < MAX_DROPLETS; ++i) droplets[i].active = false;
    for (int i = 0; i < MAX_SPIKE_ERUPTIONS; ++i) spikes[i].active = false;
    auto_droplet_timer = 0.0f;
    spike_spawn_timer = 0.0f;
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

    // 震动受惊时瞬间暴射多根尖刺
    triggerSpikeBurst(8, 1.4f);
}

void MetaballSystem::spawnRandomSpike(const PhysiologySystem &physiology) {
    for (int i = 0; i < MAX_SPIKE_ERUPTIONS; ++i) {
        if (!spikes[i].active) {
            SpikeEruption &sp = spikes[i];
            sp.active = true;
            sp.age = 0.0f;

            // 背部 (1, 2) 与尾部 (3, 4) 突刺概率更高
            int r_node = rand() % 10;
            if (r_node < 3)      sp.node_idx = 1;
            else if (r_node < 6) sp.node_idx = 2;
            else if (r_node < 8) sp.node_idx = 3;
            else if (r_node < 9) sp.node_idx = 4;
            else                 sp.node_idx = 0;

            // 随机突刺发射方向 (背脊或侧向为主)
            float base_angle = (rand() % 360) * 0.017453f;
            sp.angle = base_angle;

            float tension = physiology.getNeuroTension();
            float spike_int = physiology.getSpikeIntensity();
            float sound_ex = physiology.getSoundExcitation();

            // 尖刺突刺最大长度 (大幅加长到 14.0px ~ 28.0px，利刃般突出！)
            sp.max_len = 12.0f + (rand() % 70) * 0.1f + tension * 5.0f + spike_int * 9.0f + sound_ex * 5.0f;
            if (sp.node_idx == 1 || sp.node_idx == 2) {
                sp.max_len *= 1.35f;
            }

            sp.duration = 0.32f + (rand() % 16) * 0.01f;
            sp.attack_time = 0.04f + (rand() % 2) * 0.01f; // 40ms 极速刺出
            break;
        }
    }
}

void MetaballSystem::triggerSpikeBurst(int count, float max_len_boost) {
    for (int k = 0; k < count; ++k) {
        for (int i = 0; i < MAX_SPIKE_ERUPTIONS; ++i) {
            if (!spikes[i].active) {
                SpikeEruption &sp = spikes[i];
                sp.active = true;
                sp.age = 0.0f;
                sp.node_idx = rand() % SKELETON_NODE_COUNT;
                sp.angle = (k * (360.0f / count) + (rand() % 25 - 12)) * 0.017453f;
                sp.max_len = (10.0f + (rand() % 80) * 0.1f) * max_len_boost;
                sp.duration = 0.34f + (rand() % 12) * 0.01f;
                sp.attack_time = 0.04f;
                break;
            }
        }
    }
}

void MetaballSystem::updateSpikes(float dt, const PhysiologySystem &physiology) {
    // 1. 推进所有尖刺生命周期
    for (int i = 0; i < MAX_SPIKE_ERUPTIONS; ++i) {
        if (spikes[i].active) {
            spikes[i].age += dt;
            if (spikes[i].age >= spikes[i].duration) {
                spikes[i].active = false;
            }
        }
    }

    // 2. 【音乐播放器 5 频段背脊频谱柱状尖刺系统 (5-Band Spine Equalizer Visualizer)】
    // Node 0: Sub-Bass, Node 1: Bass, Node 2: Mid, Node 3: Presence, Node 4: Treble
    for (int n = 0; n < SKELETON_NODE_COUNT; ++n) {
        float band_e = physiology.getSpectrumBand(n);
        if (band_e > 0.14f) {
            // 检查该节点当前是否已有活动尖刺
            bool has_node_spike = false;
            for (int k = 0; k < MAX_SPIKE_ERUPTIONS; ++k) {
                if (spikes[k].active && spikes[k].node_idx == n) {
                    has_node_spike = true;
                    // 动态跟随频谱柱高度
                    float desired_len = 8.0f + band_e * 18.0f;
                    if (desired_len > spikes[k].max_len) {
                        spikes[k].max_len = desired_len;
                    }
                    break;
                }
            }

            if (!has_node_spike) {
                for (int k = 0; k < MAX_SPIKE_ERUPTIONS; ++k) {
                    if (!spikes[k].active) {
                        SpikeEruption &sp = spikes[k];
                        sp.active = true;
                        sp.age = 0.0f;
                        sp.node_idx = n;
                        // 背部向外侧斜上方刺出
                        float side_angle = (n % 2 == 0) ? -1.57f : -1.25f;
                        sp.angle = side_angle + ((rand() % 24) - 12) * 0.017453f;

                        // 尖刺长度直接对应频段 EQ 柱状高度 (8.0px ~ 24.0px)
                        sp.max_len = 8.0f + band_e * 18.0f;
                        sp.duration = 0.16f + (rand() % 6) * 0.01f; // 极速灵动随音乐跳动
                        sp.attack_time = 0.03f;
                        break;
                    }
                }
            }
        }
    }

    // 3. 愤怒与惊吓防御尖刺
    EmotionState emo = physiology.getEmotion();
    float tension = physiology.getNeuroTension();
    if (emo == EMOTION_ANGER && tension > 0.50f) {
        spike_spawn_timer += dt;
        if (spike_spawn_timer >= 0.22f) {
            spike_spawn_timer = 0.0f;
            spawnRandomSpike(physiology);
        }
    } else {
        spike_spawn_timer = 0.0f;
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
    updateSpikes(dt, physiology);
    updateDroplets(dt, skeleton, gravity_x, gravity_y, physiology);
}

void MetaballSystem::addMetaballToField(float cx, float cy, float rx, float ry, uint8_t intensity,
                                       float contact_b, float contact_t, float contact_l, float contact_r) {
    float gcx = cx / (float)GRID_SCALE;
    float gcy = cy / (float)GRID_SCALE;
    float grx = rx / (float)GRID_SCALE;
    float gry = ry / (float)GRID_SCALE;

    if (grx < 1.0f) grx = 1.0f;
    if (gry < 1.0f) gry = 1.0f;

    int min_gx = std::max(0, (int)std::floor(gcx - grx * 1.85f));
    int max_gx = std::min(GRID_W - 1, (int)std::ceil(gcx + grx * 1.85f));
    int min_gy = std::max(0, (int)std::floor(gcy - gry * 1.85f));
    int max_gy = std::min(GRID_H - 1, (int)std::ceil(gcy + gry * 1.85f));

    for (int gy = min_gy; gy <= max_gy; ++gy) {
        float dy = (float)gy - gcy;
        int row_offset = gy * GRID_W;

        for (int gx = min_gx; gx <= max_gx; ++gx) {
            float dx = (float)gx - gcx;

            float inv_grx2 = 1.0f / (grx * grx * 3.24f);
            float inv_gry2 = 1.0f / (gry * gry * 3.24f);
            float d_norm2 = dx * dx * inv_grx2 + dy * dy * inv_gry2;

            if (d_norm2 < 1.0f) {
                float val = 1.0f - d_norm2;
                float contrib = val * val * (float)intensity;

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

void MetaballSystem::computeField(const SkeletonSystem &skeleton, const PhysiologySystem &physiology,
                                  const FluidSymbolSystem &fluid_symbols, float gx, float gy,
                                  float ball_x, float ball_y, float ball_r) {
    memset(field_buffer, 0, sizeof(field_buffer));

    // 1. 骨架主球坚实融合 (无连续旋转变形，纯正饱满水滴长条)
    int node_count = skeleton.getNodeCount();
    for (int i = 0; i < node_count; ++i) {
        const SkeletonNode &n = skeleton.getNode(i);
        uint8_t strength = (i == 0) ? 190 : (i == node_count - 1 ? 145 : 170);

        addMetaballToField(n.x + n.bleb_offset_x, n.y + n.bleb_offset_y,
                           n.radius_x, n.radius_y, strength,
                           n.contact_bottom, n.contact_top, n.contact_left, n.contact_right);
    }

    // 2. 物理突发尖刺能量注入 (极速刺出 -> 软化变圆融回母体)
    for (int i = 0; i < MAX_SPIKE_ERUPTIONS; ++i) {
        if (!spikes[i].active) continue;

        const SpikeEruption &sp = spikes[i];
        const SkeletonNode &node = skeleton.getNode(sp.node_idx);

        float current_len = 0.0f;
        float tip_radius = 1.0f;
        float spike_energy = 1.0f;

        if (sp.age < sp.attack_time) {
            // [爆发刺出阶段 (0.05s)]：极速冲出，尖端如针
            float t = sp.age / sp.attack_time;
            current_len = sp.max_len * t;
            tip_radius = 1.0f;
            spike_energy = 0.85f + 0.15f * t;
        } else {
            // [软化融入阶段 (0.30s)]：表面张力阻尼，尖端变钝变粗，缓缓融回身体
            float t = (sp.age - sp.attack_time) / (sp.duration - sp.attack_time);
            float decay = 1.0f - t;
            current_len = sp.max_len * (decay * decay); // 非线性回缩
            tip_radius = 1.0f + t * 2.8f;              // 尖端变粗变圆融入
            spike_energy = decay;
        }

        if (current_len < 0.5f) continue;

        // 计算该节点在 sp.angle 方向上的表面边缘半径
        float cos_a = std::cos(sp.angle);
        float sin_a = std::sin(sp.angle);
        float r_edge = std::sqrt(node.radius_x * cos_a * node.radius_x * cos_a +
                                 node.radius_y * sin_a * node.radius_y * sin_a);

        // 沿突刺方向在 field_buffer 注入能量 (细密锋利利刃：根部 1.65px 渐变至针尖 0.50px)
        for (float l = 0.0f; l <= current_len; l += 1.3f) {
            float dist_from_center = r_edge + l;
            float world_x = node.x + cos_a * dist_from_center;
            float world_y = node.y + sin_a * dist_from_center;

            float g_px = world_x / (float)GRID_SCALE;
            float g_py = world_y / (float)GRID_SCALE;

            // 细致修长利刃：根部 1.65px 递减至针尖 0.50px
            float progress = (current_len > 0.1f) ? (l / current_len) : 0.0f;
            float current_r = (1.65f - progress * 1.15f) * tip_radius / (float)GRID_SCALE;
            if (current_r < 0.72f) current_r = 0.72f;

            float inv_r2 = 1.0f / (current_r * current_r * 2.2f);

            int min_gx = std::max(0, (int)std::floor(g_px - current_r * 1.5f));
            int max_gx = std::min(GRID_W - 1, (int)std::ceil(g_px + current_r * 1.5f));
            int min_gy = std::max(0, (int)std::floor(g_py - current_r * 1.5f));
            int max_gy = std::min(GRID_H - 1, (int)std::ceil(g_py + current_r * 1.5f));

            for (int gy = min_gy; gy <= max_gy; ++gy) {
                float dy = (float)gy - g_py;
                int row_offset = gy * GRID_W;

                for (int gx = min_gx; gx <= max_gx; ++gx) {
                    float dx = (float)gx - g_px;
                    float d2 = (dx * dx + dy * dy) * inv_r2;
                    if (d2 < 1.0f) {
                        float v = (1.0f - d2) * 215.0f * spike_energy;
                        int cur = field_buffer[row_offset + gx] + (int)v;
                        field_buffer[row_offset + gx] = (cur > 255) ? 255 : (uint8_t)cur;
                    }
                }
            }
        }
    }

    // 3. 飞溅微球累加
    for (int i = 0; i < MAX_DROPLETS; ++i) {
        if (droplets[i].active) {
            addMetaballToField(droplets[i].x, droplets[i].y, droplets[i].radius, droplets[i].radius, 120, 0, 0, 0, 0);
        }
    }

    // 3.1 自体分裂弹球累加 (饱满液态球体)
    if (ball_r > 0.5f) {
        addMetaballToField(ball_x, ball_y, ball_r, ball_r, 220, 0, 0, 0, 0);
    }

    // 4. 符号粒子流体势能注入
    int symCount = fluid_symbols.getPointCount();
    if (symCount > 0) {
        float gMag = std::sqrt(gx * gx + gy * gy);
        float norm_gx = 0.0f, norm_gy = 0.0f;
        if (gMag > 0.05f) { norm_gx = gx / gMag; norm_gy = gy / gMag; }
        float stretch = std::min(2.0f, 1.0f + 0.35f * gMag);

        for (int i = 0; i < symCount; ++i) {
            const SymbolPoint &sp = fluid_symbols.getPoint(i);
            float nx = sp.x / (float)GRID_SCALE;
            float ny = sp.y / (float)GRID_SCALE;
            float r = (sp.radius * 1.1f) / (float)GRID_SCALE;

            int r_int = (int)(r * stretch) + 3;
            int min_y = std::max(0, (int)std::floor(ny - r_int));
            int max_y = std::min(GRID_H - 1, (int)std::ceil(ny + r_int));
            int min_x = std::max(0, (int)std::floor(nx - r_int));
            int max_x = std::min(GRID_W - 1, (int)std::ceil(nx + r_int));

            for (int y = min_y; y <= max_y; ++y) {
                float dy = (float)y - ny;
                int row_offset = y * GRID_W;

                for (int x = min_x; x <= max_x; ++x) {
                    float dx = (float)x - nx;
                    float dist_px = std::sqrt(dx * dx + dy * dy);
                    float dotG = (dist_px > 0.001f) ? (dx * norm_gx + dy * norm_gy) / dist_px : 0.0f;

                    float scaledDist = dist_px;
                    if (dotG > 0) scaledDist /= (1.0f + dotG * (stretch - 1.0f));
                    else scaledDist *= (1.0f - dotG * 0.15f);

                    float dist_ratio = scaledDist / r;
                    if (dist_ratio < 1.0f) {
                        float val = (0.5f + 0.5f * std::cos(dist_ratio * 3.14159f)) * 260.0f * sp.life * (1.0f + dotG * 0.35f);
                        if (dist_ratio < 0.85f) {
                            val = std::max(val, 60.0f * sp.life);
                        }
                        int cur = field_buffer[row_offset + x] + (int)val;
                        field_buffer[row_offset + x] = (cur > 255) ? 255 : (uint8_t)cur;
                    }
                }
            }
        }
    }
}
