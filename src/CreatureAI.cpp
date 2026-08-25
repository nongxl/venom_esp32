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

    switch (new_state) {
        case STATE_IDLE:
            state_duration = 0.4f + (rand() % 5) * 0.1f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_CRAWL: {
            state_duration = 5.0f + (rand() % 20) * 0.1f;
            crawl_shoot_timer = 0.0f;

            // 基于原生好奇心与全景空间探索模型选择目标
            int roll = rand() % 100;
            if (roll < 50) {
                // 50% 目标直指屏幕中央观察窗口腹地 (Center Stage)
                crawl_target_x = 60.0f + (rand() % (SCREEN_W - 120));
                crawl_target_y = 35.0f + (rand() % (SCREEN_H - 70));
            } else if (roll < 78) {
                // 28% 目标指向天花板高处悬吊与俯瞰 (Top Ceiling)
                crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                crawl_target_y = 16.0f;
            } else if (roll < 89) {
                // 11% 探索左壁中高段
                crawl_target_x = 16.0f;
                crawl_target_y = 25.0f + (rand() % (SCREEN_H - 50));
            } else {
                // 11% 探索右壁中高段
                crawl_target_x = SCREEN_W - 16.0f;
                crawl_target_y = 25.0f + (rand() % (SCREEN_H - 50));
            }

            target_look_x = crawl_target_x;
            target_look_y = crawl_target_y;

            if (tentacles) {
                tentacles->startGrappleCrawl(hx, hy, crawl_target_x, crawl_target_y);
            }
            break;
        }

        case STATE_SWING: {
            // 【高空蛛丝悬挂荡秋千模式】
            state_duration = 6.0f + (rand() % 35) * 0.1f; // 持续 6.0 ~ 9.5 秒
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;

            // 选取正上方天花板锚点
            float anchor_x = std::max(24.0f, std::min(SCREEN_W - 24.0f, hx + (rand() % 50 - 25)));
            float anchor_y = 6.0f;
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
            state_duration = 0.6f + (rand() % 6) * 0.1f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            target_look_x = 40.0f + (rand() % (SCREEN_W - 80));
            target_look_y = 25.0f + (rand() % (SCREEN_H - 50));
            break;

        case STATE_SLEEP:
            state_duration = 1.8f;
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
        int r = rand() % 100;
        // 若毒液处于上半区 (hy < 65)，有 28% 概率直接挂上天花板荡秋千！
        if (hy < 65.0f && r < 28) {
            enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
        } else if (r < 88) { // 88% 超高运动意愿！
            enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
        } else {
            enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
        }
    }
}

void CreatureAI::updateCrawl(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    target_look_x = crawl_target_x;
    target_look_y = crawl_target_y;

    float dx = crawl_target_x - hx;
    float dy = crawl_target_y - hy;
    float dist = std::sqrt(dx * dx + dy * dy);

    // 极速连环触手迈步：前一根触手刚吸收完，下一发在 0.04s 内瞬间爆射！
    if (!tentacles.isGrappling()) {
        crawl_shoot_timer += dt;
        if (crawl_shoot_timer > 0.04f && dist > 10.0f) {
            crawl_shoot_timer = 0.0f;
            tentacles.startGrappleCrawl(hx, hy, crawl_target_x, crawl_target_y);
        }
    }

    // 若目的地在天花板 (crawl_target_y < 25) 且已接近 (dist <= 14px)，50% 概率转入高空荡秋千玩耍！
    if (dist <= 12.0f || (state_timer >= state_duration && !tentacles.isGrappling())) {
        if (crawl_target_y < 25.0f && (rand() % 100) < 50) {
            enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
        } else {
            enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
        }
    }
}

void CreatureAI::updateObserve(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    if (state_timer >= state_duration) {
        int roll = rand() % 100;
        if (hy < 65.0f && roll < 26) {
            enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
        } else if (roll < 88) { // 88% 爬行概率！
            enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
        } else {
            enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
        }
    }
}

void CreatureAI::updateSwing(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles) {
    // 眼睛注视点：好奇地注视正下方屏幕或观察者
    target_look_x = hx + std::sin(state_timer * 2.0f) * 30.0f;
    target_look_y = hy + 45.0f;

    // 荡秋千时间结束，或身体受到剧烈激惹时，平稳落地
    if (state_timer >= state_duration || !skeleton.isHanging()) {
        skeleton.clearHangingAnchor();
        tentacles.endCeilingSwing();
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::updateSleep(float dt, float hx, float hy, const PhysiologySystem &physiology) {
    if (state_timer >= state_duration || physiology.getStress() > 0.20f) {
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
