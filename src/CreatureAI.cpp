#include "CreatureAI.h"
#include <cmath>

CreatureAI::CreatureAI() {}

void CreatureAI::init() {
    enterState(STATE_OBSERVE);
}

const char* CreatureAI::getStateName() const {
    switch (current_state) {
        case STATE_IDLE:       return "IDLE";
        case STATE_CRAWL:      return "CRAWL";
        case STATE_OBSERVE:    return "OBSERVE";
        case STATE_SLEEP:      return "SLEEP";
        case STATE_STARTLED:   return "STARTLED";
        case STATE_HESITATING: return "HESITATE";
        case STATE_JOLTING:    return "JOLT";
        case STATE_EXPRESSING: return "EXPRESS";
        case STATE_SWING:      return "SWING";
        default:               return "UNKNOWN";
    }
}

void CreatureAI::enterHesitation(CreatureState target_state, float delay_sec) {
    current_state = STATE_HESITATING;
    pending_state = target_state;
    hesitation_timer = delay_sec;
    crawl_force_x *= 0.2f;
    crawl_force_y *= 0.2f;
}

void CreatureAI::enterState(CreatureState new_state, TentacleRenderer *tentacles, SkeletonSystem *skeleton, float hx, float hy) {
    current_state = new_state;
    state_timer = 0.0f;

    // 安全保障：离开 SWING 或进入其他状态时强制清理悬挂与拉动死锁
    if (skeleton && new_state != STATE_SWING) {
        skeleton->clearHangingAnchor();
        skeleton->clearPullTarget();
    }

    switch (new_state) {
        case STATE_IDLE:
            state_duration = 3.5f + (rand() % 30) * 0.1f; // 3.5 ~ 6.5s 舒缓发呆呼吸
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_CRAWL: {
            state_duration = 3.2f + (rand() % 15) * 0.1f; // 3.2 ~ 4.7s 爬1~2步即休整
            crawl_shoot_timer = 0.60f; // 刚进入爬行时先观察准备 0.60s 再迈出第一步

            // 基于原生好奇心与全景空间探索模型选择目标 (优先开阔腹地)
            int roll = rand() % 100;
            if (hx < 30.0f || hx > SCREEN_W - 30.0f || hy < 30.0f || hy > SCREEN_H - 30.0f) {
                // 处于边缘区域时：80% 概率向屏幕中心腹地大步爬行脱离！
                if (roll < 80) {
                    crawl_target_x = 70.0f + (rand() % (SCREEN_W - 140));
                    crawl_target_y = 40.0f + (rand() % (SCREEN_H - 80));
                } else {
                    crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                    crawl_target_y = 22.0f;
                }
            } else {
                if (roll < 55) {
                    // 55% 目标直指屏幕中央观察窗口腹地
                    crawl_target_x = 60.0f + (rand() % (SCREEN_W - 120));
                    crawl_target_y = 35.0f + (rand() % (SCREEN_H - 70));
                } else if (roll < 80) {
                    // 25% 目标指向天花板安全高度
                    crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                    crawl_target_y = 22.0f;
                } else if (roll < 90) {
                    crawl_target_x = 25.0f;
                    crawl_target_y = 35.0f + (rand() % (SCREEN_H - 70));
                } else {
                    crawl_target_x = SCREEN_W - 25.0f;
                    crawl_target_y = 35.0f + (rand() % (SCREEN_H - 70));
                }
            }

            target_look_x = crawl_target_x;
            target_look_y = crawl_target_y;
            break;
        }

        case STATE_SWING: {
            // 【高空蛛丝悬挂荡秋千模式】
            state_duration = 5.5f + (rand() % 25) * 0.1f; // 持续 5.5 ~ 8.0 秒
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;

            // 选取正上方安全天花板锚点 (y=16 绝不挤压到顶部死角)
            float anchor_x = std::max(30.0f, std::min(SCREEN_W - 30.0f, hx + (rand() % 40 - 20)));
            float anchor_y = 16.0f;
            float rope_len = SWING_ROPE_LENGTH;

            if (skeleton) {
                skeleton->setHangingAnchor(anchor_x, anchor_y, rope_len);
            }
            if (tentacles) {
                tentacles->startCeilingSwing(hx, hy, anchor_x, anchor_y, rope_len);
            }
            break;
        }

        case STATE_OBSERVE:
            state_duration = 4.0f + (rand() % 40) * 0.1f; // 4.0 ~ 8.0s 停顿观察与发呆
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            target_look_x = 40.0f + (rand() % (SCREEN_W - 80));
            target_look_y = 25.0f + (rand() % (SCREEN_H - 50));
            break;

        case STATE_SLEEP:
            state_duration = 18.0f + (rand() % 140) * 0.1f; // 安详深度睡眠 18 ~ 32 秒，恢复体力
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_STARTLED:
            state_duration = 1.0f;
            startle_energy = 1.0f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_JOLTING:
            state_duration = 0.6f;
            startle_energy = 1.3f;
            break;

        case STATE_EXPRESSING:
            state_duration = 3.5f;
            break;

        case STATE_HESITATING:
            break;
    }
}

void CreatureAI::triggerStartle(float intensity) {
    startle_energy = intensity;
    enterState(STATE_STARTLED);
}

void CreatureAI::triggerReactiveCrawl(SkeletonSystem &skeleton, TentacleRenderer &tentacles) {
    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    // 挑选开阔腹地或上方天花板作为挣脱目标
    float target_x = 50.0f + (rand() % (SCREEN_W - 100));
    float target_y = (hy > 70.0f) ? (20.0f + (rand() % 40)) : (60.0f + (rand() % 50));

    crawl_target_x = target_x;
    crawl_target_y = target_y;
    target_look_x = target_x;
    target_look_y = target_y;

    current_state = STATE_CRAWL;
    state_timer = 0.0f;
    state_duration = 5.0f;
    crawl_shoot_timer = 0.0f;

    tentacles.startGrappleCrawl(hx, hy, target_x, target_y);
}

void CreatureAI::triggerJolt(SkeletonSystem &skeleton, MetaballSystem &metaballs, float intensity) {
    metaballs.triggerJoltSpurt(skeleton, intensity);
    skeleton.applyImpulse((rand() % 80 - 40) * 0.1f, -3.2f * intensity);
    enterState(STATE_JOLTING);
}

void CreatureAI::triggerInteraction() {
    if (current_state == STATE_SLEEP) {
        enterState(STATE_OBSERVE);
    } else if (current_state == STATE_IDLE) {
        enterHesitation(STATE_CRAWL, 0.12f);
    } else {
        triggerStartle(0.7f);
    }
}

void CreatureAI::updateSensors(float imu_gx, float imu_gy, float imu_gz, const PhysiologySystem &physiology, bool btn_a_pressed) {
    last_imu_gx = imu_gx;
    last_imu_gy = imu_gy;

    if (btn_a_pressed) {
        triggerInteraction();
        return;
    }

    float total_g = std::sqrt(imu_gx * imu_gx + imu_gy * imu_gy + imu_gz * imu_gz);
    if (total_g > IMU_SHAKE_THRESHOLD && current_state != STATE_STARTLED && current_state != STATE_JOLTING) {
        triggerStartle(1.2f);
        return;
    }

    if (physiology.getAudioHigh() > 0.70f && current_state != STATE_STARTLED && current_state != STATE_JOLTING) {
        triggerStartle(0.9f);
        return;
    }
}

void CreatureAI::updateOrganicBreathing(float dt, const PhysiologySystem &physiology, const ExpressionLayer &expression) {
    float base_speed = (physiology.getEmotion() == EMOTION_STRESS || physiology.getEmotion() == EMOTION_FEAR) ? 4.5f : 2.0f;
    float audio_low_boost = physiology.getAudioLow() * 3.5f;

    if (expression.getCurrentExpression() == EXPR_OBSERVE || expression.getCurrentExpression() == EXPR_SILENT_OBSERVATION) {
        base_speed *= 0.65f;
    }

    respiration_phase += dt * (base_speed + audio_low_boost);

    float s = std::sin(respiration_phase);
    float raw_resp = (s > 0) ? std::pow(s, 0.75f) : -std::pow(-s, 1.2f);

    twitch_timer += dt;
    if (twitch_timer > 1.8f) {
        twitch_timer = 0.0f;
        if ((rand() % 100) < 40) {
            twitch_offset = ((rand() % 40) - 20) * 0.002f;
        } else {
            twitch_offset = 0.0f;
        }
    }

    respiration_factor = raw_resp * (0.04f + physiology.getAudioLow() * 0.03f) + twitch_offset;
}

void CreatureAI::updateMicroBehaviors(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology) {
    micro_behavior_timer += dt;
    if (micro_behavior_timer > 1.2f) {
        micro_behavior_timer = 0.0f;
        if ((rand() % 100) < 70) {
            int node = rand() % SKELETON_NODE_COUNT;
            skeleton.triggerLocalBleb(node, 0.35f + physiology.getNeuroTension() * 0.5f);
        }
    }
}

void CreatureAI::updateHesitating(float dt, float hx, float hy, const ExpressionLayer &expression) {
    hesitation_timer -= dt;
    if (hesitation_timer <= 0.0f) {
        enterState(pending_state);
        return;
    }

    float phase = expression.getHesitationStep();
    float p_mod = fmod(phase, 4.0f);

    if (p_mod < 1.0f) {
        crawl_force_x = 0.45f;
        crawl_force_y = 0.0f;
    } else if (p_mod < 2.0f) {
        crawl_force_x = 0.0f;
        crawl_force_y = 0.0f;
    } else if (p_mod < 3.0f) {
        crawl_force_x = -0.40f;
        crawl_force_y = 0.0f;
    } else {
        crawl_force_x = 0.35f;
        crawl_force_y = 0.0f;
    }
}

void CreatureAI::updateIdle(float dt, float hx, float hy, const PhysiologySystem &physiology, const RelationshipSystem &relationship, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    target_look_x = hx + std::cos(respiration_phase * 0.5f) * 45.0f;
    target_look_y = hy + std::sin(respiration_phase * 0.7f) * 25.0f - 12.0f;

    if (state_timer >= state_duration) {
        float energy = physiology.getEnergy();
        int r = rand() % 100;

        // 体力低下时 (< 0.35) 优先进入深度睡眠恢复状态
        if (energy < 0.35f) {
            enterState(STATE_SLEEP, &tentacles, &skeleton, hx, hy);
        } else if (energy < 0.65f) {
            // 中度疲惫：72% 停顿观察, 23% 呼吸发呆, 仅 5% 偶尔移动
            if (r < 5) {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
            } else if (r < 77) {
                enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
            } else {
                enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
            }
        } else {
            // 精力充沛：60% 观察周围, 28% 安静发呆, 仅 7% 偶尔爬行, 5% 荡秋千
            if (r < 5) {
                enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
            } else if (r < 12) {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
            } else if (r < 72) {
                enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
            } else {
                enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
            }
        }
    }
}

void CreatureAI::updateCrawl(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    target_look_x = crawl_target_x;
    target_look_y = crawl_target_y;

    float dx = crawl_target_x - hx;
    float dy = crawl_target_y - hy;
    float dist = std::sqrt(dx * dx + dy * dy);

    // 【克服重力能耗倍率计算】：顺着重力下滑省力，逆着重力向上攀爬消耗成倍体力！
    float g_len = std::sqrt(last_imu_gx * last_imu_gx + last_imu_gy * last_imu_gy);
    float norm_gx = (g_len > 0.01f) ? (last_imu_gx / g_len) : 0.0f;
    float norm_gy = (g_len > 0.01f) ? (last_imu_gy / g_len) : 1.0f;

    float norm_dx = (dist > 0.01f) ? (dx / dist) : 0.0f;
    float norm_dy = (dist > 0.01f) ? (dy / dist) : -1.0f;

    // 点积为负代表逆着重力向上攀爬
    float gravity_dot = norm_dx * norm_gx + norm_dy * norm_gy;
    float anti_gravity_mult = 1.0f;
    if (gravity_dot < 0.0f) {
        anti_gravity_mult += (-gravity_dot) * 2.8f; // 逆重力攀爬最高达 3.8 倍能耗！
    }

    const_cast<PhysiologySystem&>(physiology).consumeEnergy(dt * 0.022f * anti_gravity_mult);

    // 从容触手迈步：触手收回后必须深呼吸等待 1.35s，绝不连续连迈！
    if (!tentacles.isGrappling()) {
        crawl_shoot_timer += dt;
        if (crawl_shoot_timer > 1.35f && dist > 14.0f) {
            crawl_shoot_timer = 0.0f;
            tentacles.startGrappleCrawl(hx, hy, crawl_target_x, crawl_target_y);
        }
    }

    // 爬行 1 步或接近目标 (dist <= 16px)，立刻主动停下来休整发呆！
    if (dist <= 16.0f || (state_timer >= state_duration && !tentacles.isGrappling())) {
        if (crawl_target_y < 35.0f && physiology.getEnergy() > 0.55f && (rand() % 100) < 40) {
            enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
        } else {
            enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
        }
    }
}

void CreatureAI::updateObserve(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    if (state_timer >= state_duration) {
        float energy = physiology.getEnergy();
        int roll = rand() % 100;

        if (energy < 0.35f) {
            enterState(STATE_SLEEP, &tentacles, &skeleton, hx, hy);
        } else if (energy < 0.65f) {
            // 中度疲惫：75% 安静发呆, 20% 观察周围, 仅 5% 偶尔移动
            if (roll < 5) {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
            } else if (roll < 80) {
                enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
            } else {
                enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
            }
        } else {
            // 精力充沛：65% 安静发呆, 23% 观察周围, 仅 7% 偶尔爬行, 5% 荡秋千
            if (roll < 5) {
                enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
            } else if (roll < 12) {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
            } else if (roll < 77) {
                enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
            } else {
                enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
            }
        }
    }
}

void CreatureAI::updateSwing(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles) {
    // 眼睛注视点：好奇地注视正下方屏幕或观察者
    target_look_x = hx + std::sin(state_timer * 2.0f) * 30.0f;
    target_look_y = hy + 45.0f;

    // 【高空悬吊全力抗重力消耗能量】
    // 荡秋千时间结束，或身体受到剧烈激惹时，平稳落地
    if (state_timer >= state_duration || !skeleton.isHanging()) {
        skeleton.clearHangingAnchor();
        tentacles.endCeilingSwing();
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::updateSleep(float dt, float hx, float hy, const PhysiologySystem &physiology) {
    // 睡眠中深度恢复体力 (+0.065/s)
    const_cast<PhysiologySystem&>(physiology).recoverEnergy(dt * 0.065f);

    target_look_x = hx;
    target_look_y = hy + 10.0f;

    // 睡眠结束条件：体力回满 (>=0.80) 且睡眠时间达到，或者受到外界剧烈惊吓/大声
    if ((state_timer >= state_duration && physiology.getEnergy() >= 0.80f) || physiology.getStress() > 0.35f) {
        enterState(STATE_OBSERVE);
    }
}

void CreatureAI::updateStartled(float dt, float hx, float hy, const PhysiologySystem &physiology) {
    startle_energy -= dt * 0.80f;
    if (state_timer >= state_duration || startle_energy <= 0.0f) {
        enterState(STATE_OBSERVE);
    }
}

void CreatureAI::updateJolting(float dt, float hx, float hy, const PhysiologySystem &physiology) {
    startle_energy -= dt * 1.5f;
    if (state_timer >= state_duration || startle_energy <= 0.0f) {
        enterState(STATE_OBSERVE);
    }
}

void CreatureAI::updateExpressing(float dt, float hx, float hy, const ExpressionLayer &expression) {
    if (expression.getCurrentExpression() == EXPR_NONE || state_timer >= state_duration) {
        enterState(STATE_OBSERVE);
    }
}

void CreatureAI::update(float dt, SkeletonSystem &skeleton, MetaballSystem &metaballs,
                        TentacleRenderer &tentacles, PhysiologySystem &physiology,
                        RelationshipSystem &relationship, ExpressionLayer &expression,
                        const ConsciousnessStateV3 &v3_state,
                        const PreyBugSystem *bugs) {
    state_timer += dt;

    updateOrganicBreathing(dt, physiology, expression);
    updateMicroBehaviors(dt, skeleton, physiology);

    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    if (expression.getCurrentExpression() != EXPR_NONE &&
        current_state != STATE_STARTLED && current_state != STATE_JOLTING &&
        current_state != STATE_HESITATING && current_state != STATE_SWING) {
        current_state = STATE_EXPRESSING;
    }

    switch (current_state) {
        case STATE_HESITATING: updateHesitating(dt, hx, hy, expression); break;
        case STATE_IDLE:       updateIdle(dt, hx, hy, physiology, relationship, tentacles, skeleton); break;
        case STATE_CRAWL:      updateCrawl(dt, hx, hy, physiology, tentacles, skeleton); break;
        case STATE_OBSERVE:    updateObserve(dt, hx, hy, physiology, tentacles, skeleton); break;
        case STATE_SWING:      updateSwing(dt, hx, hy, skeleton, tentacles); break;
        case STATE_SLEEP:      updateSleep(dt, hx, hy, physiology); break;
        case STATE_STARTLED:   updateStartled(dt, hx, hy, physiology); break;
        case STATE_JOLTING:    updateJolting(dt, hx, hy, physiology); break;
        case STATE_EXPRESSING: updateExpressing(dt, hx, hy, expression); break;
    }

    // 麦克风声控互动与高分贝惊吓防卫
    float mic_db = physiology.getMicDecibels();
    if (mic_db > 73.0f && current_state != STATE_SWING && current_state != STATE_STARTLED && current_state != STATE_JOLTING) {
        // 突发高分贝/大声吹气：瞬间受惊防卫姿态并向四周激射微液滴！
        triggerStartle(0.75f);
        metaballs.triggerJoltSpurt(skeleton, 1.2f);
    } else if (mic_db > 55.0f && current_state == STATE_IDLE) {
        // 中等环境音/人声：唤醒毒液进入警觉观察
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
    }

    // 猎物感知与眼球注视锁定 (Stalking Focus)
    if (bugs && current_state != STATE_SLEEP && current_state != STATE_STARTLED) {
        float bx, by;
        BugState b_state;
        int bug_idx = bugs->getNearestBug(hx, hy, bx, by, b_state);
        if (bug_idx >= 0 && b_state != BUG_DEAD) {
            target_look_x = bx;
            target_look_y = by;
        }
    }
}
