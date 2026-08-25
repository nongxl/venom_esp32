#include "CreatureAI.h"
#include <cmath>

CreatureAI::CreatureAI() {}

void CreatureAI::init() {
    enterState(STATE_IDLE);
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

void CreatureAI::enterState(CreatureState new_state, TentacleRenderer *tentacles, float hx, float hy) {
    current_state = new_state;
    state_timer = 0.0f;

    switch (new_state) {
        case STATE_IDLE:
            state_duration = 3.0f + (rand() % 35) * 0.1f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_CRAWL:
            state_duration = 4.5f + (rand() % 40) * 0.1f;
            crawl_shoot_timer = 0.0f;
            crawl_perimeter_edge = rand() % 4;

            if (crawl_perimeter_edge == 0) {
                crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                crawl_target_y = SCREEN_H - 16.0f;
            } else if (crawl_perimeter_edge == 1) {
                crawl_target_x = SCREEN_W - 16.0f;
                crawl_target_y = 25.0f + (rand() % (SCREEN_H - 50));
            } else if (crawl_perimeter_edge == 2) {
                crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                crawl_target_y = 16.0f;
            } else {
                crawl_target_x = 16.0f;
                crawl_target_y = 25.0f + (rand() % (SCREEN_H - 50));
            }

            // 触发主动射出爬行触手
            if (tentacles) {
                tentacles->startGrappleCrawl(hx, hy, crawl_target_x, crawl_target_y);
            }
            break;

        case STATE_OBSERVE:
            state_duration = 2.8f + (rand() % 25) * 0.1f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            target_look_x = (rand() % SCREEN_W);
            target_look_y = (rand() % SCREEN_H);
            break;

        case STATE_SLEEP:
            state_duration = 7.0f + (rand() % 60) * 0.1f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_STARTLED:
            state_duration = 1.4f;
            startle_energy = 1.0f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_JOLTING:
            state_duration = 0.8f;
            startle_energy = 1.3f;
            break;

        case STATE_EXPRESSING:
            state_duration = 4.0f;
            break;

        case STATE_HESITATING:
            break;
    }
}

void CreatureAI::triggerStartle(float intensity) {
    startle_energy = intensity;
    enterState(STATE_STARTLED);
}

void CreatureAI::triggerJolt(SkeletonSystem &skeleton, MetaballSystem &metaballs, float intensity) {
    metaballs.triggerJoltSpurt(skeleton, intensity);
    skeleton.applyImpulse((rand() % 80 - 40) * 0.1f, -2.8f * intensity);
    enterState(STATE_JOLTING);
}

void CreatureAI::triggerInteraction() {
    if (current_state == STATE_SLEEP) {
        enterHesitation(STATE_OBSERVE, 0.4f);
    } else if (current_state == STATE_IDLE) {
        enterHesitation(STATE_CRAWL, 0.35f);
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
    float base_speed = (physiology.getEmotion() == EMOTION_STRESS || physiology.getEmotion() == EMOTION_FEAR) ? 4.5f : 1.8f;
    float audio_low_boost = physiology.getAudioLow() * 3.5f;

    if (expression.getCurrentExpression() == EXPR_OBSERVE || expression.getCurrentExpression() == EXPR_SILENT_OBSERVATION) {
        base_speed *= 0.45f;
    }

    respiration_phase += dt * (base_speed + audio_low_boost);

    float s = std::sin(respiration_phase);
    float raw_resp = (s > 0) ? std::pow(s, 0.75f) : -std::pow(-s, 1.2f);

    twitch_timer += dt;
    if (twitch_timer > 2.0f) {
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
    if (micro_behavior_timer > 1.6f) {
        micro_behavior_timer = 0.0f;
        if ((rand() % 100) < 65) {
            int node = rand() % SKELETON_NODE_COUNT;
            skeleton.triggerLocalBleb(node, 0.4f + physiology.getNeuroTension() * 0.6f);
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
        crawl_force_x = 0.35f;
        crawl_force_y = 0.0f;
    } else if (p_mod < 2.0f) {
        crawl_force_x = 0.0f;
        crawl_force_y = 0.0f;
    } else if (p_mod < 3.0f) {
        crawl_force_x = -0.30f;
        crawl_force_y = 0.0f;
    } else {
        crawl_force_x = 0.25f;
        crawl_force_y = 0.0f;
    }
}

void CreatureAI::updateIdle(float dt, float hx, float hy, const PhysiologySystem &physiology, const RelationshipSystem &relationship, TentacleRenderer &tentacles) {
    target_look_x = hx + std::cos(respiration_phase * 0.5f) * 35.0f;
    target_look_y = hy + std::sin(respiration_phase * 0.7f) * 20.0f - 10.0f;

    if (state_timer >= state_duration) {
        float curiosity = physiology.getCuriosity();
        float openness = relationship.getSocialOpenness();
        int r = rand() % 100;

        if (r < (int)(curiosity * openness * 80.0f)) {
            enterState(STATE_CRAWL, &tentacles, hx, hy);
        } else if (r < 75) {
            enterState(STATE_OBSERVE);
        } else if (physiology.getEnergy() < 0.3f || r < 90) {
            enterState(STATE_SLEEP);
        } else {
            enterState(STATE_IDLE);
        }
    }
}

void CreatureAI::updateCrawl(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles) {
    target_look_x = crawl_target_x;
    target_look_y = crawl_target_y;

    float dx = crawl_target_x - hx;
    float dy = crawl_target_y - hy;
    float dist = std::sqrt(dx * dx + dy * dy);

    // 如果触手抓取已完成且还未到达，或者触手未激活，重新射击一次完成爬行跨越
    if (!tentacles.isGrappling()) {
        crawl_shoot_timer += dt;
        if (crawl_shoot_timer > 0.3f && dist > 14.0f) {
            crawl_shoot_timer = 0.0f;
            tentacles.startGrappleCrawl(hx, hy, crawl_target_x, crawl_target_y);
        }
    }

    if (dist <= 12.0f || state_timer >= state_duration) {
        enterState(STATE_OBSERVE);
    }
}

void CreatureAI::updateObserve(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles) {
    if (state_timer >= state_duration) {
        if ((rand() % 100) < 55) {
            enterState(STATE_CRAWL, &tentacles, hx, hy);
        } else {
            enterState(STATE_IDLE);
        }
    }
}

void CreatureAI::updateSleep(float dt, float hx, float hy, const PhysiologySystem &physiology) {
    if (state_timer >= state_duration || physiology.getStress() > 0.4f) {
        enterState(STATE_IDLE);
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
                        const ConsciousnessStateV3 &v3_state) {
    state_timer += dt;

    updateOrganicBreathing(dt, physiology, expression);
    updateMicroBehaviors(dt, skeleton, physiology);

    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    if (expression.getCurrentExpression() != EXPR_NONE &&
        current_state != STATE_STARTLED && current_state != STATE_JOLTING &&
        current_state != STATE_HESITATING) {
        current_state = STATE_EXPRESSING;
    }

    switch (current_state) {
        case STATE_HESITATING: updateHesitating(dt, hx, hy, expression); break;
        case STATE_IDLE:       updateIdle(dt, hx, hy, physiology, relationship, tentacles); break;
        case STATE_CRAWL:      updateCrawl(dt, hx, hy, physiology, tentacles); break;
        case STATE_OBSERVE:    updateObserve(dt, hx, hy, physiology, tentacles); break;
        case STATE_SLEEP:      updateSleep(dt, hx, hy, physiology); break;
        case STATE_STARTLED:   updateStartled(dt, hx, hy, physiology); break;
        case STATE_JOLTING:    updateJolting(dt, hx, hy, physiology); break;
        case STATE_EXPRESSING: updateExpressing(dt, hx, hy, expression); break;
    }
}
