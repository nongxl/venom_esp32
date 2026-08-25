#include "CreatureAI.h"
#include <cmath>

CreatureAI::CreatureAI() {}

void CreatureAI::init() {
    enterState(STATE_OBSERVE);
}

const char* CreatureAI::getStateName() const {
    switch (current_state) {
        case STATE_IDLE:          return "IDLE";
        case STATE_CRAWL:         return "CRAWL";
        case STATE_OBSERVE:       return "OBSERVE";
        case STATE_SLEEP:         return "SLEEP";
        case STATE_STARTLED:      return "STARTLED";
        case STATE_HESITATING:    return "HESITATE";
        case STATE_JOLTING:       return "JOLT";
        case STATE_EXPRESSING:    return "EXPRESS";
        case STATE_SWING:         return "SWING";
        case STATE_HUNT_TONGUE:   return "HUNT_TONGUE";
        case STATE_HUNT_TENTACLE: return "HUNT_TENTACLE";
        case STATE_HUNT_SNARE:    return "HUNT_SNARE";
        default:                  return "UNKNOWN";
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

        case STATE_HUNT_TONGUE:
            state_duration = 0.60f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_HUNT_TENTACLE:
            state_duration = 1.20f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_HUNT_SNARE:
            state_duration = 4.0f;
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
    float roll = (rand() % 100) * 0.01f;
    if (roll < 0.60f) {
        enterHesitation(STATE_OBSERVE, 0.20f);
    } else {
        enterState(STATE_IDLE);
    }
}

void CreatureAI::updateSensors(float imu_gx, float imu_gy, float imu_gz, const PhysiologySystem &physiology, bool btn_a_pressed) {
    float total_g = std::sqrt(imu_gx * imu_gx + imu_gy * imu_gy + imu_gz * imu_gz);
    if (total_g > 18.0f) {
        triggerStartle(0.9f);
    }
}

void CreatureAI::updateOrganicBreathing(float dt, const PhysiologySystem &physiology, const ExpressionLayer &expression) {
    float rate = 2.2f + physiology.getNeuroTension() * 2.8f;
    if (expression.getCurrentExpression() != EXPR_NONE) rate *= 1.4f;

    respiration_phase += dt * rate;
    if (respiration_phase > 6.2831853f) respiration_phase -= 6.2831853f;

    float sin_val = std::sin(respiration_phase);
    respiration_factor = (sin_val > 0.0f) ? std::pow(sin_val, 1.2f) : -std::pow(-sin_val, 0.8f);
}

void CreatureAI::updateMicroBehaviors(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology) {
    micro_behavior_timer += dt;
    if (micro_behavior_timer > 3.5f) {
        micro_behavior_timer = 0.0f;
        int action = rand() % 100;
        if (action < 35) {
            twitch_timer = 0.18f;
            twitch_offset = ((rand() % 40) - 20) * 0.12f;
        }
    }

    if (twitch_timer > 0.0f) {
        twitch_timer -= dt;
        skeleton.applyImpulse(twitch_offset, -twitch_offset * 0.5f);
    }
}

void CreatureAI::updateHesitating(float dt, float hx, float hy, const ExpressionLayer &expression) {
    hesitation_timer -= dt;
    target_look_x = hx + std::sin(state_timer * 12.0f) * 6.0f;
    target_look_y = hy + std::cos(state_timer * 12.0f) * 6.0f;

    if (hesitation_timer <= 0.0f) {
        current_state = pending_state;
        state_timer = 0.0f;
    }
}

void CreatureAI::updateIdle(float dt, float hx, float hy, const PhysiologySystem &physiology, const RelationshipSystem &relationship, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    target_look_x = hx + std::cos(respiration_phase * 0.5f) * 45.0f;
    target_look_y = hy + std::sin(respiration_phase * 0.7f) * 25.0f - 12.0f;

    if (state_timer >= state_duration) {
        int r = rand() % 100;
        if (hy < 65.0f && r < 28) {
            enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
        } else if (r < 88) {
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
        } else if (roll < 88) {
            enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
        } else {
            enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
        }
    }
}

void CreatureAI::updateSwing(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles) {
    target_look_x = hx + std::sin(state_timer * 2.0f) * 30.0f;
    target_look_y = hy + 45.0f;

    if (state_timer >= state_duration || !skeleton.isHanging()) {
        skeleton.clearHangingAnchor();
        tentacles.endCeilingSwing();
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::checkAndTriggerHunting(float hx, float hy, PreySystem &prey, MouthSystem &mouth, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    if (isHunting() || isStartled() || current_state == STATE_SWING) return;

    float bug_dist = 999.0f;
    int bug_idx = prey.findClosestBug(hx, hy, bug_dist, 140.0f);

    if (bug_idx >= 0) {
        float bx, by;
        prey.getBugPos(bug_idx, bx, by);
        PreyState b_state = prey.getBugState(bug_idx);

        // 视线瞬间紧盯猎物！
        target_look_x = bx;
        target_look_y = by;

        if (b_state == PREY_FREE) {
            int roll = rand() % 100;
            if (bug_dist < 85.0f || roll < 52) {
                // 【方式 1：52% 变色龙闪电长舌弹射】
                targeted_bug_idx = bug_idx;
                enterState(STATE_HUNT_TONGUE, &tentacles, &skeleton, hx, hy);
                mouth.triggerTongueStrike(bx, by, bug_idx);
                prey.hookBugWithTongue(bug_idx);
            } else if (roll < 80) {
                // 【方式 2：28% 触手抓取喂嘴】
                targeted_bug_idx = bug_idx;
                enterState(STATE_HUNT_TENTACLE, &tentacles, &skeleton, hx, hy);
                tentacles.startGrappleCrawl(hx, hy, bx, by);
                prey.grabBugWithTentacle(bug_idx);
                mouth.triggerReceiveFeed(bug_idx);
            } else {
                // 【方式 3：20% 黏液定身爬食】
                targeted_bug_idx = bug_idx;
                enterState(STATE_HUNT_SNARE, &tentacles, &skeleton, hx, hy);
                prey.launchSlimeSnare(hx, hy, bug_idx);
                crawl_target_x = bx;
                crawl_target_y = by;
            }
        } else if (b_state == PREY_SNARED && current_state != STATE_HUNT_SNARE) {
            // 发现被定身的虫子，直接开启爬行捕食
            targeted_bug_idx = bug_idx;
            enterState(STATE_HUNT_SNARE, &tentacles, &skeleton, hx, hy);
            crawl_target_x = bx;
            crawl_target_y = by;
        }
    }
}

void CreatureAI::updateHuntTongue(float dt, float hx, float hy, PreySystem &prey, MouthSystem &mouth, PhysiologySystem &physiology) {
    if (targeted_bug_idx >= 0) {
        float tx, ty;
        mouth.getTongueTipPos(tx, ty);
        // 舌头弹射卷中后，虫子跟随舌尖极速拉回
        prey.updateSnaredBugPos(targeted_bug_idx, tx, ty);
        target_look_x = tx;
        target_look_y = ty;

        // 舌头收回口中，触发吞噬！
        if (mouth.getState() == MOUTH_CHEW || mouth.getState() == MOUTH_LICK || state_timer >= state_duration) {
            prey.eatBug(targeted_bug_idx);
            physiology.feedNutrient(0.20f);
            targeted_bug_idx = -1;
            enterState(STATE_OBSERVE);
        }
    } else {
        enterState(STATE_OBSERVE);
    }
}

void CreatureAI::updateHuntTentacle(float dt, float hx, float hy, PreySystem &prey, MouthSystem &mouth, TentacleRenderer &tentacles, PhysiologySystem &physiology) {
    if (targeted_bug_idx >= 0) {
        float bx, by;
        prey.getBugPos(targeted_bug_idx, bx, by);
        target_look_x = bx;
        target_look_y = by;

        // 触手收回到头部附近时，虫子被送入口中
        float dx = bx - hx;
        float dy = by - hy;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 14.0f || state_timer >= state_duration || !tentacles.isGrappling()) {
            prey.eatBug(targeted_bug_idx);
            mouth.triggerChewAndSwallow();
            physiology.feedNutrient(0.20f);
            targeted_bug_idx = -1;
            enterState(STATE_OBSERVE);
        }
    } else {
        enterState(STATE_OBSERVE);
    }
}

void CreatureAI::updateHuntSnare(float dt, float hx, float hy, PreySystem &prey, MouthSystem &mouth, SkeletonSystem &skeleton, TentacleRenderer &tentacles, PhysiologySystem &physiology) {
    if (targeted_bug_idx >= 0) {
        float bx, by;
        prey.getBugPos(targeted_bug_idx, bx, by);
        target_look_x = bx;
        target_look_y = by;
        crawl_target_x = bx;
        crawl_target_y = by;

        float dx = bx - hx;
        float dy = by - hy;
        float dist = std::sqrt(dx * dx + dy * dy);

        // 连续大步触手冲向虫子
        if (!tentacles.isGrappling()) {
            crawl_shoot_timer += dt;
            if (crawl_shoot_timer > 0.05f && dist > 12.0f) {
                crawl_shoot_timer = 0.0f;
                tentacles.startGrappleCrawl(hx, hy, bx, by);
            }
        }

        // 毒液爬到虫子跟前（距离 < 16px），大嘴猛张一口吞下！
        if (dist <= 16.0f) {
            prey.eatBug(targeted_bug_idx);
            mouth.triggerChewAndSwallow();
            physiology.feedNutrient(0.25f);
            targeted_bug_idx = -1;
            enterState(STATE_OBSERVE);
        } else if (state_timer >= state_duration) {
            enterState(STATE_OBSERVE);
        }
    } else {
        enterState(STATE_OBSERVE);
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
                        PreySystem &prey, MouthSystem &mouth,
                        const ConsciousnessStateV3 &v3_state) {
    state_timer += dt;

    updateOrganicBreathing(dt, physiology, expression);
    updateMicroBehaviors(dt, skeleton, physiology);

    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    // 1. 猎物侦测与捕食动机触发
    checkAndTriggerHunting(hx, hy, prey, mouth, tentacles, skeleton);

    // 2. 状态机分发
    if (expression.getCurrentExpression() != EXPR_NONE &&
        current_state != STATE_STARTLED && current_state != STATE_JOLTING &&
        current_state != STATE_HESITATING && current_state != STATE_SWING &&
        !isHunting()) {
        current_state = STATE_EXPRESSING;
    }

    switch (current_state) {
        case STATE_HESITATING:    updateHesitating(dt, hx, hy, expression); break;
        case STATE_IDLE:          updateIdle(dt, hx, hy, physiology, relationship, tentacles, skeleton); break;
        case STATE_CRAWL:         updateCrawl(dt, hx, hy, physiology, tentacles, skeleton); break;
        case STATE_OBSERVE:       updateObserve(dt, hx, hy, physiology, tentacles, skeleton); break;
        case STATE_SWING:         updateSwing(dt, hx, hy, skeleton, tentacles); break;
        case STATE_HUNT_TONGUE:   updateHuntTongue(dt, hx, hy, prey, mouth, physiology); break;
        case STATE_HUNT_TENTACLE: updateHuntTentacle(dt, hx, hy, prey, mouth, tentacles, physiology); break;
        case STATE_HUNT_SNARE:    updateHuntSnare(dt, hx, hy, prey, mouth, skeleton, tentacles, physiology); break;
        case STATE_SLEEP:         updateSleep(dt, hx, hy, physiology); break;
        case STATE_STARTLED:      updateStartled(dt, hx, hy, physiology); break;
        case STATE_JOLTING:       updateJolting(dt, hx, hy, physiology); break;
        case STATE_EXPRESSING:    updateExpressing(dt, hx, hy, expression); break;
    }
}
