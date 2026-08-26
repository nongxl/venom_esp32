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
        case STATE_CREEP:      return "CREEP";
        case STATE_OBSERVE:    return "OBSERVE";
        case STATE_SLEEP:      return "SLEEP";
        case STATE_STARTLED:   return "STARTLED";
        case STATE_HESITATING: return "HESITATE";
        case STATE_JOLTING:    return "JOLTING";
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

    if (skeleton && new_state != STATE_CREEP) {
        skeleton->clearCreepingTarget();
    }

    if (tentacles && new_state != STATE_CREEP) {
        tentacles->setCreepMode(false);
    }

    switch (new_state) {
        case STATE_IDLE:
            state_duration = 8.0f + (rand() % 80) * 0.1f; // 8.0 ~ 16.0s 舒缓发呆生活与平稳呼吸
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_CREEP: {
            // 【表皮小触手近距离缓慢蠕动漫步模式】
            state_duration = 3.5f + (rand() % 25) * 0.1f; // 3.5 ~ 6.0s 表皮小触手缓慢蠕动
            float angle = (float)(rand() % 360) * 0.017453f;
            float step_dist = 28.0f + (float)(rand() % 25);
            crawl_target_x = std::max(25.0f, std::min(SCREEN_W - 25.0f, hx + std::cos(angle) * step_dist));
            crawl_target_y = std::max(25.0f, std::min(SCREEN_H - 25.0f, hy + std::sin(angle) * step_dist));

            target_look_x = crawl_target_x;
            target_look_y = crawl_target_y;

            // 关键：起步前先原地对齐头部朝向，头部在最前端领路，杜绝倒退！
            if (skeleton) {
                skeleton->alignHeadingToTarget(crawl_target_x, crawl_target_y);
                skeleton->setCreepingTarget(crawl_target_x, crawl_target_y, 1.0f);
            }
            if (tentacles) {
                tentacles->setCreepMode(true, 1.0f);
            }
            break;
        }

        case STATE_CRAWL: {
            // 【粗壮大触手远距离大步爬行攀爬模式】
            state_duration = 4.0f + (rand() % 25) * 0.1f; // 4.0 ~ 6.5s 探索爬行
            crawl_shoot_timer = 0.40f; // 刚进入爬行时先观察准备 0.4s 再迈出第一步

            // 基于原生好奇心与全景空间探索模型选择目标 (优先开阔腹地)
            int roll = rand() % 100;
            if (hx < 30.0f || hx > SCREEN_W - 30.0f || hy < 30.0f || hy > SCREEN_H - 30.0f) {
                if (roll < 85) {
                    crawl_target_x = 70.0f + (rand() % (SCREEN_W - 140));
                    crawl_target_y = 40.0f + (rand() % (SCREEN_H - 80));
                } else {
                    crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                    crawl_target_y = 22.0f;
                }
            } else {
                if (roll < 55) {
                    crawl_target_x = 60.0f + (rand() % (SCREEN_W - 120));
                    crawl_target_y = 35.0f + (rand() % (SCREEN_H - 70));
                } else if (roll < 80) {
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

            // 关键：发射大触手前头部转向目标，杜绝倒退！
            if (skeleton) {
                skeleton->alignHeadingToTarget(crawl_target_x, crawl_target_y);
            }
            break;
        }

        case STATE_SWING: {
            // 【高空蛛丝悬挂荡秋千模式 (牛顿摆物理)】
            state_duration = 9.0f + (rand() % 70) * 0.1f; // 持续 9.0 ~ 16.0 秒舒适悬挂
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;

            // 选取正上方天花板锚点 (y=2 紧紧吸附在屏幕顶边)
            float anchor_x = std::max(50.0f, std::min(SCREEN_W - 50.0f, hx + (rand() % 40 - 20)));
            float anchor_y = 2.0f;
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
            state_duration = 6.0f + (rand() % 60) * 0.1f; // 6.0 ~ 12.0s 好奇观察周围与打量观察者
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            target_look_x = 40.0f + (rand() % (SCREEN_W - 80));
            target_look_y = 25.0f + (rand() % (SCREEN_H - 50));
            break;

        case STATE_SLEEP:
            state_duration = 20.0f + (rand() % 150) * 0.1f; // 安详深度睡眠 20 ~ 35 秒，完全恢复体力
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

    if (physiology.getAudioHigh() > 0.85f && physiology.getMicDecibels() > 82.0f &&
        current_state != STATE_STARTLED && current_state != STATE_JOLTING) {
        triggerStartle(0.9f);
        return;
    }
}

void CreatureAI::updateOrganicBreathing(float dt, const PhysiologySystem &physiology, const ExpressionLayer &expression) {
    // 自然深沉共生体生命呼吸律动 (一次完整呼吸 6.5 ~ 9.0 秒，极其平缓悠长舒展)
    float base_speed = 0.90f; // 0.9 rad/s 对应周期约 7.0 秒
    if (current_state == STATE_SLEEP) {
        base_speed = 0.55f;   // 睡眠时深长呼吸 (周期约 11.4 秒)
    } else if (physiology.getEmotion() == EMOTION_STRESS || physiology.getEmotion() == EMOTION_FEAR) {
        base_speed = 1.80f;   // 惊恐时呼吸加快 (周期约 3.5 秒)
    }

    if (expression.getCurrentExpression() == EXPR_OBSERVE || expression.getCurrentExpression() == EXPR_SILENT_OBSERVATION) {
        base_speed *= 0.75f;  // 屏息凝神观察
    }

    respiration_phase += dt * base_speed;

    float s = std::sin(respiration_phase);
    // 柔和平滑的自然非线性吸气-呼气曲线 (吸气慢舒张，呼气微停留)
    float raw_resp = (s > 0) ? std::pow(s, 0.85f) : -std::pow(-s, 1.15f);

    // 呼吸起伏幅度温和深沉 (0.065)，视觉上极其平缓悠远，绝无急促抽动
    respiration_factor = raw_resp * 0.065f;
}

void CreatureAI::updateMicroBehaviors(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology) {
    micro_behavior_timer += dt;
    if (micro_behavior_timer > 4.5f) {
        micro_behavior_timer = 0.0f;
        // 仅在有明显神经张力时才偶尔触发轻微局部隆起 (30% 概率)
        if (physiology.getNeuroTension() > 0.35f && (rand() % 100) < 30) {
            int node = 1 + (rand() % 3);
            skeleton.triggerLocalBleb(node, 0.4f * physiology.getNeuroTension());
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

        // 体力低下时 (< 0.25) 自然进入深度睡眠恢复状态
        if (energy < 0.25f) {
            enterState(STATE_SLEEP, &tentacles, &skeleton, hx, hy);
        } else if (energy < 0.60f) {
            // 中度疲惫：35% 表皮小触手轻松蠕动漫步, 5% 大触手攀爬, 30% 好奇观察, 30% 继续安详呼吸发呆
            if (r < 35) {
                enterState(STATE_CREEP, &tentacles, &skeleton, hx, hy);
            } else if (r < 40) {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
            } else if (r < 70) {
                enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
            } else {
                enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
            }
        } else {
            // 精力充沛：40% 表皮小触手优雅蠕动巡逻, 12% 荡秋千, 12% 大触手攀爬, 20% 观察周围, 16% 安静发呆
            if (r < 40) {
                enterState(STATE_CREEP, &tentacles, &skeleton, hx, hy);
            } else if (r < 52) {
                enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
            } else if (r < 64) {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
            } else if (r < 84) {
                enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
            } else {
                enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
            }
        }
    }
}

void CreatureAI::updateCreep(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    target_look_x = crawl_target_x;
    target_look_y = crawl_target_y;

    float dx = crawl_target_x - hx;
    float dy = crawl_target_y - hy;
    float dist = std::sqrt(dx * dx + dy * dy);

    // 确保触手与全节点推进处于活跃状态
    tentacles.setCreepMode(true, 1.0f);
    skeleton.setCreepingTarget(crawl_target_x, crawl_target_y, 1.0f);

    const_cast<PhysiologySystem&>(physiology).consumeEnergy(dt * 0.006f); // 蠕动能耗极低

    // 蠕动到达目标或超时，转入从容观察发呆
    if (dist <= 14.0f || state_timer >= state_duration) {
        skeleton.clearCreepingTarget();
        tentacles.setCreepMode(false);
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::updateCrawl(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    target_look_x = crawl_target_x;
    target_look_y = crawl_target_y;

    float dx = crawl_target_x - hx;
    float dy = crawl_target_y - hy;
    float dist = std::sqrt(dx * dx + dy * dy);

    // 确保头部朝向目标
    skeleton.alignHeadingToTarget(crawl_target_x, crawl_target_y);

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

    // 自然触手迈步：触手收回后停顿 0.85s 观察呼吸，再从容迈出下一步！
    if (!tentacles.isGrappling()) {
        crawl_shoot_timer += dt;
        if (crawl_shoot_timer > 0.85f && dist > 14.0f) {
            crawl_shoot_timer = 0.0f;
            tentacles.startGrappleCrawl(hx, hy, crawl_target_x, crawl_target_y);
        }
    }

    // 爬行到达目标或超时，转入观察发呆
    if (dist <= 16.0f || (state_timer >= state_duration && !tentacles.isGrappling())) {
        if (crawl_target_y < 35.0f && physiology.getEnergy() > 0.55f && (rand() % 100) < 45) {
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

        if (energy < 0.25f) {
            enterState(STATE_SLEEP, &tentacles, &skeleton, hx, hy);
        } else if (energy < 0.60f) {
            // 中度疲惫：35% 表皮小触手蠕动, 5% 大触手爬行, 30% 继续观察, 30% 安静发呆
            if (roll < 35) {
                enterState(STATE_CREEP, &tentacles, &skeleton, hx, hy);
            } else if (roll < 40) {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
            } else if (roll < 70) {
                enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
            } else {
                enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
            }
        } else {
            // 精力充沛：40% 表皮小触手蠕动, 12% 荡秋千, 12% 大触手攀爬, 20% 观察周围, 16% 安静发呆
            if (roll < 40) {
                enterState(STATE_CREEP, &tentacles, &skeleton, hx, hy);
            } else if (roll < 52) {
                enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
            } else if (roll < 64) {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);
            } else if (roll < 84) {
                enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
            } else {
                enterState(STATE_IDLE, &tentacles, &skeleton, hx, hy);
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
        case STATE_CREEP:      updateCreep(dt, hx, hy, physiology, tentacles, skeleton); break;
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
