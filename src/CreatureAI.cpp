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
        case STATE_CATCH_DUST: return "CATCH_DUST";
        case STATE_ROLL:       return "ROLL";
        case STATE_BOUNCE:     return "BOUNCE";
        case STATE_BAT_HANG:   return "BAT_HANG";
        case STATE_BALL_PLAY:  return "BALL_PLAY";
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

    // 安全保障：离开相关状态或进入新状态时强制清理物理状态与拉动死锁
    if (skeleton && new_state != STATE_SWING) {
        skeleton->clearHangingAnchor();
        skeleton->clearPullTarget();
    }
    if (skeleton) {
        skeleton->setRollingMode(false);
        skeleton->setBatHangMode(false);
    }

    if (skeleton && new_state != STATE_CREEP) {
        skeleton->clearCreepingTarget();
    }

    if (tentacles && new_state != STATE_CREEP) {
        tentacles->setCreepMode(false);
    }

    if (new_state != STATE_BALL_PLAY) {
        symbiote_ball.active = false;
    }

    switch (new_state) {
        case STATE_IDLE:
            state_duration = 2.2f + (rand() % 20) * 0.1f; // 2.2 ~ 4.2s 短暂休憩呼吸后立即出发探索！
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            break;

        case STATE_CREEP: {
            // 【表皮小触手近距离缓慢蠕动漫步模式】
            state_duration = 6.5f + (rand() % 25) * 0.1f; // 6.5 ~ 9.0s 充分长途漫步
            
            // 智能方向选择：优先沿头部当前朝向前进，避免“倒退”
            float dir_sign;
            if (skeleton) {
                float heading = skeleton->getHeadingAngle();
                dir_sign = (std::cos(heading) >= 0.0f) ? 1.0f : -1.0f;
            } else {
                dir_sign = ((rand() % 100) < 50) ? 1.0f : -1.0f;
            }
            // 边缘保护：靠边时强制转向
            if (hx < 40.0f) dir_sign = 1.0f;
            if (hx > (float)SCREEN_W - 40.0f) dir_sign = -1.0f;

            // 2D 广阔空间巡逻漫步：不仅在地表横行，也能向天花板和背景中央爬行！
            int roll = rand() % 100;
            if (hy > (float)SCREEN_H - 35.0f) {
                // 靠底边时：65% 向上攀爬探索中央和天花板背景！
                if (roll < 65) {
                    crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                    crawl_target_y = 25.0f + (rand() % (SCREEN_H - 65));
                } else {
                    float step_dist = 60.0f + (rand() % 60);
                    crawl_target_x = std::max(25.0f, std::min((float)SCREEN_W - 25.0f, hx + dir_sign * step_dist));
                    crawl_target_y = std::max(25.0f, std::min((float)SCREEN_H - 25.0f, hy + ((rand() % 16) - 8)));
                }
            } else {
                crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                crawl_target_y = 25.0f + (rand() % (SCREEN_H - 50));
            }

            target_look_x = crawl_target_x;
            target_look_y = crawl_target_y;

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
            state_duration = 7.0f + (rand() % 30) * 0.1f; // 7.0 ~ 10.0s 连贯多步攀爬探索
            crawl_shoot_timer = 0.0f; // 立即迈出第一步大触手！

            // 基于原生好奇心选择高空/开阔腹地目标
            int roll = rand() % 100;
            if (hy > (float)SCREEN_H - 35.0f) {
                // 靠底边时：75% 强力射向中央/天花板高空背景攀爬！
                if (roll < 75) {
                    crawl_target_x = 40.0f + (rand() % (SCREEN_W - 80));
                    crawl_target_y = 22.0f + (rand() % 50); // 上半区天花板/高空背景
                } else {
                    crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                    crawl_target_y = (float)SCREEN_H - 25.0f;
                }
            } else {
                // 已在空中/中央：探索对角线或天花板
                crawl_target_x = 35.0f + (rand() % (SCREEN_W - 70));
                crawl_target_y = 22.0f + (rand() % (SCREEN_H - 45));
            }

            target_look_x = crawl_target_x;
            target_look_y = crawl_target_y;

            if (skeleton) {
                skeleton->alignHeadingToTarget(crawl_target_x, crawl_target_y);
            }
            break;
        }

        case STATE_SWING: {
            // 【高空蛛丝悬挂荡秋千模式 (牛顿摆物理)】
            state_duration = 22.0f; // 充足的 22.0 秒悬挂时间
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;

            // 选取正上方天花板锚点 (y=2 紧紧吸附在屏幕顶边)
            float anchor_x = std::max(50.0f, std::min((float)SCREEN_W - 50.0f, hx));
            float anchor_y = 2.0f;
            float rope_len = SWING_ROPE_LENGTH;

            // 由 TentacleRenderer 先向上极速射出触手抓天花板，随后强力将肉身提拉上去！
            if (tentacles) {
                tentacles->startCeilingSwing(hx, hy, anchor_x, anchor_y, rope_len);
            }
            Serial.printf("[AI] enterState(STATE_SWING): anchor=(%.1f, %.1f) dur=%.1f (shoot & hoist started)\n", anchor_x, anchor_y, state_duration);
            break;
        }

        case STATE_OBSERVE:
            state_duration = 2.0f + (rand() % 18) * 0.1f; // 2.0 ~ 3.8s 敏锐好奇观察片刻后即刻行动！
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            target_look_x = 40.0f + (rand() % (SCREEN_W - 80));
            target_look_y = 25.0f + (rand() % (SCREEN_H - 50));
            break;

        case STATE_SLEEP:
            state_duration = 18.0f + (rand() % 100) * 0.1f; // 快速舒适小憩 18 ~ 28 秒，恢复满满元气
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            sleep_zz_timer = 1.5f; // 入睡后很快喷出第一串 Zz
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

        case STATE_CATCH_DUST: {
            state_duration = 3.2f; // 0.75s 蓄力盯梢 + 0.10s 飞扑 + 2.35s 好奇注视
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            crawl_target_x = std::max(28.0f, std::min((float)SCREEN_W - 28.0f, hx + ((rand() % 2 == 0) ? (35.0f + (rand() % 35)) : -(35.0f + (rand() % 35)))));
            crawl_target_y = std::max(22.0f, hy - (25.0f + (rand() % 25)));
            target_look_x = crawl_target_x;
            target_look_y = crawl_target_y;
            break;
        }

        case STATE_ROLL: {
            state_duration = 5.5f;
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            roll_vx = (rand() % 2 == 0) ? 42.0f : -42.0f;
            roll_bounces = 0;
            if (skeleton) skeleton->setRollingMode(true);
            break;
        }

        case STATE_BOUNCE: {
            bool is_vertical = (std::abs(last_imu_gx) > 3.0f || std::abs(last_imu_gy) > 6.5f);
            state_duration = 10.0f; // 充足时间完成多轮弹性高空弹跳
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            bounce_max_jumps = is_vertical ? (4 + (rand() % 2)) : (3 + (rand() % 2));
            bounce_jump_count = 0;
            bounce_phase = 0;
            bounce_timer = 0.0f;
            if (skeleton) {
                skeleton->setBouncingMode(true);
                skeleton->setBounceDeform(0.35f, 1.85f);
            }
            break;
        }

        case STATE_BAT_HANG: {
            state_duration = 20.0f + (rand() % 150) * 0.1f; // 20.0~35.0s hanging
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            float anchor_x = std::max(50.0f, std::min((float)SCREEN_W - 50.0f, hx + (rand() % 40 - 20)));
            float anchor_y = 2.0f;
            float rope_len = 35.0f; // 倒挂金钟较短悬挂长度

            if (skeleton) {
                skeleton->setBatHangMode(true);
            }
            if (tentacles) {
                tentacles->startCeilingSwing(hx, hy, anchor_x, anchor_y, rope_len);
            }
            break;
        }

        case STATE_BALL_PLAY: {
            state_duration = 50.0f; // 充足时间确保 8~20 次尽兴颠球
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            symbiote_ball.active = true;
            symbiote_ball.bounce_count = 0;
            symbiote_ball.max_bounces = 8 + (rand() % 13); // 8 ~ 20 次颠球
            symbiote_ball.phase = BALL_PLAYING;
            symbiote_ball.phase_timer = 0.0f;
            symbiote_ball.hit_cooldown = 0.0f;
            symbiote_ball.radius = 2.0f; // 初始萌芽小球

            // 从尾部分裂出生 (严格限制在安全屏幕内)
            float tail_x = hx, tail_y = hy;
            if (skeleton) {
                const SkeletonNode &tail = skeleton->getNode(SKELETON_NODE_COUNT - 1);
                tail_x = tail.x;
                tail_y = tail.y;
                skeleton->triggerLocalBleb(SKELETON_NODE_COUNT - 1, 1.8f);
            }
            symbiote_ball.x = std::max(20.0f, std::min((float)SCREEN_W - 20.0f, tail_x));
            symbiote_ball.y = std::max(25.0f, std::min((float)SCREEN_H - 20.0f, tail_y - 4.0f));
            symbiote_ball.vx = (rand() % 2 == 0 ? 1.8f : -1.8f);
            symbiote_ball.vy = -5.5f; // 向上方抛起
            break;
        }
    }
}

void CreatureAI::triggerStartle(float intensity) {
    // 荡秋千/倒挂等悬挂玩耍模式下，绝对豁免受惊打断！
    if (current_state == STATE_SWING || current_state == STATE_BAT_HANG) return;
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

    // 荡秋千/倒挂等悬挂玩耍模式下，用户倾斜、翻转、晃动是核心交互，不触发任何惊吓打断！
    if (current_state == STATE_SWING || current_state == STATE_BAT_HANG) {
        return;
    }

    float total_g = std::sqrt(imu_gx * imu_gx + imu_gy * imu_gy + imu_gz * imu_gz);
    float delta_g = std::abs(total_g - last_total_g);
    last_total_g = total_g;

    // 【睡眠感知】：受到 IMU 突然大范围晃动（拿起/甩动）或 MIC 突然高声惊醒
    if (isSleeping()) {
        if (total_g > 1.35f || delta_g > 0.35f) {
            triggerStartle(1.2f);
            Serial.println("[AI] Symbiote startled awake from sleep by sudden IMU motion!");
            return;
        }
        if (physiology.getMicDecibels() > 58.0f) {
            triggerStartle(0.9f);
            Serial.println("[AI] Symbiote startled awake from sleep by sudden noise!");
            return;
        }
    }

    float dynamic_threshold = (current_state == STATE_SWING || current_state == STATE_BAT_HANG || current_state == STATE_ROLL) ? 3.5f : IMU_SHAKE_THRESHOLD;
    
    if (total_g > dynamic_threshold && current_state != STATE_STARTLED && current_state != STATE_JOLTING) {
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

    // 发呆静止中：无聊度持续自然累积，平静中好奇心蓄积
    const_cast<PhysiologySystem&>(physiology).addBoredom(dt * 0.08f);
    const_cast<PhysiologySystem&>(physiology).applyStimulus(0.0f, dt * 0.02f);

    float energy = physiology.getEnergy();
    float boredom = physiology.getBoredom();

    // 1. 疲惫驱动：体力过低 (< 0.18) 时自然进入睡眠恢复
    if (energy < 0.18f) {
        enterState(STATE_SLEEP, &tentacles, &skeleton, hx, hy);
        return;
    }

    // 2. 无聊/好奇心欲望驱动闭环 (无聊度累积至阈值或自然微停顿结束)
    if (boredom >= 0.30f || state_timer >= state_duration) {
        int r = rand() % 100;
        bool is_vertical = (std::abs(last_imu_gx) > 3.0f || std::abs(last_imu_gy) > 6.5f);

        if (is_vertical) {
            // 【竖屏模式】：屏幕垂直纵深高达 240px，65% 极高概率触发壮观的高空蹦蹦床特技！
            if (r < 65) {
                enterState(STATE_BOUNCE, &tentacles, &skeleton, hx, hy); // 65% 竖屏蹦蹦床高空弹跳
            } else if (r < 80) {
                enterState(STATE_BALL_PLAY, &tentacles, &skeleton, hx, hy); // 15% 自体颠球
            } else if (r < 90) {
                enterState(STATE_ROLL, &tentacles, &skeleton, hx, hy);      // 10% 软体翻滚
            } else {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);     // 10% 攀爬
            }
        } else {
            // 【横屏模式】：丰富多样化自娱自乐生态
            if (r < 35) {
                enterState(STATE_BOUNCE, &tentacles, &skeleton, hx, hy);    // 35% 蹦蹦床弹跳
            } else if (r < 55) {
                enterState(STATE_BALL_PLAY, &tentacles, &skeleton, hx, hy); // 20% 自体颠球
            } else if (r < 70) {
                enterState(STATE_CATCH_DUST, &tentacles, &skeleton, hx, hy);// 15% 飞扑抓发光浮尘
            } else if (r < 85) {
                enterState(STATE_ROLL, &tentacles, &skeleton, hx, hy);      // 15% 软体翻滚
            } else if (r < 95) {
                enterState(STATE_CREEP, &tentacles, &skeleton, hx, hy);     // 10% 地表漫步
            } else {
                enterState(STATE_BAT_HANG, &tentacles, &skeleton, hx, hy);  // 5% 倒挂金钟
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

    // 持续对齐身体朝向，确保头部始终朝前不倒退
    skeleton.alignHeadingToTarget(crawl_target_x, crawl_target_y);

    // 漫步释放无聊感，并消耗适量体力
    const_cast<PhysiologySystem&>(physiology).reduceBoredom(dt * 0.12f);
    const_cast<PhysiologySystem&>(physiology).consumeEnergy(dt * 0.008f);

    // 蠕动到达目标或超时，转入从容观察发呆
    if (dist <= 8.0f || state_timer >= state_duration) {
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

    float gravity_dot = norm_dx * norm_gx + norm_dy * norm_gy;
    float anti_gravity_mult = 1.0f;
    if (gravity_dot < 0.0f) {
        anti_gravity_mult += (-gravity_dot) * 2.8f;
    }

    const_cast<PhysiologySystem&>(physiology).reduceBoredom(dt * 0.16f);
    const_cast<PhysiologySystem&>(physiology).consumeEnergy(dt * 0.018f * anti_gravity_mult);

    // 连贯大触手迈步攀爬：如果当前触手未在抓爬，且距离目标还比较远，立即射出触手迈步！
    if (!tentacles.isGrappling()) {
        crawl_shoot_timer += dt;
        if (crawl_shoot_timer >= 0.10f && dist > 12.0f) {
            crawl_shoot_timer = 0.0f;
            float step_len = std::min(dist, 60.0f);
            float step_x = hx + (dx / dist) * step_len;
            float step_y = hy + (dy / dist) * step_len;
            tentacles.startGrappleCrawl(hx, hy, step_x, step_y);
        }
    } else {
        crawl_shoot_timer = 0.0f;
    }

    // 爬行到达目标或超时，转入观察或抓浮尘/翻滚等动作
    if ((dist <= 16.0f && !tentacles.isGrappling()) || (state_timer >= state_duration && !tentacles.isGrappling())) {
        int r = rand() % 100;
        if (crawl_target_y < 35.0f && physiology.getEnergy() > 0.65f && r < 18) {
            enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
        } else if (r < 25) {
            enterState(STATE_CATCH_DUST, &tentacles, &skeleton, hx, hy);
        } else if (r < 45) {
            enterState(STATE_BOUNCE, &tentacles, &skeleton, hx, hy);
        } else {
            enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
        }
    }
}

void CreatureAI::updateObserve(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    // 观察阶段：无聊度持续累积，好奇心活跃
    const_cast<PhysiologySystem&>(physiology).addBoredom(dt * 0.085f);
    const_cast<PhysiologySystem&>(physiology).applyStimulus(0.0f, dt * 0.025f);

    float energy = physiology.getEnergy();
    float boredom = physiology.getBoredom();

    if (energy < 0.18f) {
        enterState(STATE_SLEEP, &tentacles, &skeleton, hx, hy);
        return;
    }

    if (boredom >= 0.28f || state_timer >= state_duration) {
        int roll = rand() % 100;
        bool is_vertical = (std::abs(last_imu_gx) > 3.0f || std::abs(last_imu_gy) > 6.5f);

        if (is_vertical) {
            if (roll < 65) {
                enterState(STATE_BOUNCE, &tentacles, &skeleton, hx, hy); // 65% 竖屏高空蹦蹦床
            } else if (roll < 80) {
                enterState(STATE_BALL_PLAY, &tentacles, &skeleton, hx, hy); // 15% 自体颠球
            } else if (roll < 90) {
                enterState(STATE_ROLL, &tentacles, &skeleton, hx, hy);      // 10% 软体翻滚
            } else {
                enterState(STATE_CRAWL, &tentacles, &skeleton, hx, hy);     // 10% 攀爬
            }
        } else {
            if (roll < 35) {
                enterState(STATE_BOUNCE, &tentacles, &skeleton, hx, hy);    // 35% 蹦蹦床
            } else if (roll < 55) {
                enterState(STATE_BALL_PLAY, &tentacles, &skeleton, hx, hy); // 20% 自体颠球
            } else if (roll < 70) {
                enterState(STATE_CATCH_DUST, &tentacles, &skeleton, hx, hy);// 15% 飞扑抓浮尘
            } else if (roll < 85) {
                enterState(STATE_ROLL, &tentacles, &skeleton, hx, hy);      // 15% 软体翻滚
            } else if (roll < 95) {
                enterState(STATE_CREEP, &tentacles, &skeleton, hx, hy);     // 10% 地表小触手
            } else {
                enterState(STATE_BAT_HANG, &tentacles, &skeleton, hx, hy);  // 5% 倒挂
            }
        }
    }
}

void CreatureAI::updateSwing(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles) {
    target_look_x = hx + std::sin(state_timer * 2.0f) * 30.0f;
    target_look_y = hy + 45.0f;

    if (state_timer >= state_duration) {
        skeleton.clearHangingAnchor();
        tentacles.endCeilingSwing();
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::updateCatchDust(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, ExpressionLayer &expression, PhysiologySystem &physiology, const PreyBugSystem *bugs, FluidSymbolSystem *fluid_symbols) {
    physiology.reduceBoredom(dt * 0.22f);
    physiology.consumeEnergy(dt * 0.012f);

    float target_x = crawl_target_x;
    float target_y = crawl_target_y;

    // 若场上有小虫，则锁定小虫位置作为扑跃玩耍目标（但不立刻吃掉）
    if (bugs) {
        float bx, by;
        BugState b_state;
        int bug_idx = bugs->getNearestBug(hx, hy, bx, by, b_state);
        if (bug_idx >= 0 && b_state != BUG_DEAD) {
            target_x = bx;
            target_y = by;
        }
    }

    if (state_timer < 0.75f) {
        // 【阶段 1: 猫咪式压低身躯瞄准蓄力 (0.75s)】
        target_look_x = target_x;
        target_look_y = target_y;
        expression.triggerExpression(EXPR_CURIOSITY, 2.0f);
        // 尾部节点微微蓄力轻颤
        skeleton.triggerLocalBleb(SKELETON_NODE_COUNT - 1, 0.6f);
    } else if (state_timer < 0.85f && state_timer - dt < 0.75f) {
        // 【阶段 2: 飞身闪电猛扑 (Explosive Pounce, 0.10s)】
        float dx = target_x - hx;
        float dy = target_y - hy;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 1.0f) {
            float pounce_speed = 38.0f;
            skeleton.applyImpulse((dx / dist) * pounce_speed, (dy / dist) * pounce_speed - 10.0f);
            skeleton.triggerLocalBleb(0, 1.8f);
        }
    } else if (state_timer >= 0.85f && state_timer < 1.30f) {
        // 【阶段 3: 扑落贴近、好奇注视 (Playful Stare, 不吃)】
        target_look_x = target_x;
        target_look_y = target_y;
        expression.triggerExpression(EXPR_CURIOSITY, 2.5f);
    } else if (state_timer >= 1.8f && state_timer - dt < 1.8f) {
        // 【阶段 4: 落地后困惑地吐出问号】
        if (fluid_symbols) {
            float sym_x = std::max(45.0f, std::min((float)SCREEN_W - 45.0f, hx));
            float sym_y = (hy > 65.0f) ? (hy - 45.0f) : (hy + 45.0f);
            fluid_symbols->trigger("?", sym_x, sym_y);
        }
    }

    if (state_timer >= state_duration) {
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::updateRoll(float dt, float hx, float hy, SkeletonSystem &skeleton, ExpressionLayer &expression, PhysiologySystem &physiology, MetaballSystem *metaballs) {
    physiology.reduceBoredom(dt * 0.22f);
    physiology.consumeEnergy(dt * 0.015f);

    skeleton.setRollingMode(true);

    // 软体球形在地面高速滚动 (vx = 36.0~45.0 px/s)
    skeleton.applyImpulse(roll_vx * dt * 3.5f, 0.0f);

    // 墙壁弹性碰撞掉头
    if (hx < 22.0f && roll_vx < 0.0f) {
        roll_vx = -roll_vx * 0.95f;
        roll_bounces++;
        skeleton.applyImpulse(18.0f, -8.0f);
        expression.triggerExpression(EXPR_CURIOSITY, 1.2f);
        if (metaballs) metaballs->spawnDroplet(hx + 4.0f, hy, 1.2f, -1.0f, 2.0f, true);
    } else if (hx > SCREEN_W - 22.0f && roll_vx > 0.0f) {
        roll_vx = -roll_vx * 0.95f;
        roll_bounces++;
        skeleton.applyImpulse(-18.0f, -8.0f);
        expression.triggerExpression(EXPR_CURIOSITY, 1.2f);
        if (metaballs) metaballs->spawnDroplet(hx - 4.0f, hy, -1.2f, -1.0f, 2.0f, true);
    }

    target_look_x = hx + ((roll_vx > 0) ? 25.0f : -25.0f);
    target_look_y = hy;

    if (state_timer >= state_duration || roll_bounces >= 3) {
        skeleton.setRollingMode(false);
        enterState(STATE_OBSERVE, nullptr, &skeleton, hx, hy);
    }
}

void CreatureAI::updateBounce(float dt, float hx, float hy, SkeletonSystem &skeleton, MetaballSystem &metaballs, PhysiologySystem &physiology, ExpressionLayer &expression) {
    physiology.reduceBoredom(dt * 0.25f);
    physiology.consumeEnergy(dt * 0.015f);

    skeleton.setBouncingMode(true);
    bounce_timer += dt;
    bool is_vertical = (std::abs(last_imu_gx) > 3.0f || std::abs(last_imu_gy) > 6.5f);

    switch (bounce_phase) {
        case 0: { // 【阶段 0: 触地瞬间极限压扁与紧绷蓄能 (Extreme Squash & Compression, 0.10s)】
            target_look_x = hx;
            target_look_y = hy - 45.0f; // 紧缩身体、兴奋向上瞄准
            expression.triggerExpression(EXPR_CURIOSITY, 1.5f);

            // 骨骼极度压扁（高度压缩到 0.35x，横向像高弹胶皮一样拉伸到 1.85x）
            skeleton.setBounceDeform(0.35f, 1.85f);

            if (bounce_timer >= 0.10f) {
                bounce_phase = 1;
                bounce_timer = 0.0f;
            }
            break;
        }

        case 1: { // 【阶段 1: 蹦蹦床/超弹球火箭弹射瞬间 (Explosive Launch Snap, 0.05s)】
            // 超高弹性反弹初速度：竖屏时高达 -90 ~ -115 px/s，横屏时 -64 ~ -85 px/s
            float launch_vy = is_vertical ? (-90.0f - (bounce_jump_count * 8.0f)) : (-64.0f - (bounce_jump_count * 6.0f));
            float launch_vx = ((rand() % 30) - 15) * 0.18f;

            skeleton.applyImpulse(launch_vx, launch_vy);

            // 瞬间在垂直方向暴风拉伸 (1.65x 高度, 0.62x 宽度)
            skeleton.setBounceDeform(1.65f, 0.62f);

            // 触地反冲向地面激射 3 颗黑色高速飞溅液滴
            metaballs.spawnDroplet(hx - 7.0f, hy + 8.0f, -0.8f, -1.8f, 2.4f, true);
            metaballs.spawnDroplet(hx + 7.0f, hy + 8.0f, 0.8f, -1.8f, 2.4f, true);
            metaballs.spawnDroplet(hx, hy + 9.0f, 0.0f, -1.2f, 2.0f, true);

            bounce_phase = 2;
            bounce_timer = 0.0f;
            break;
        }

        case 2: { // 【阶段 2: 空中飞跃与高频果冻弹性回弹震颤 (High Air Flight & Jelly Wobble, 0.38s)】
            target_look_x = hx;
            target_look_y = (bounce_timer < 0.20f) ? (hy - 25.0f) : (hy + 40.0f);

            // 飞跃过程中产生阻尼高频弹性震颤 (拉长 <-> 压扁快速回弹振荡)
            float wobble = std::sin(bounce_timer * 30.0f) * std::exp(-3.2f * bounce_timer);
            float sq_y = 1.0f + 0.40f * wobble;
            float st_x = 1.0f / std::sqrt(std::max(0.35f, sq_y));
            skeleton.setBounceDeform(sq_y, st_x);

            if (bounce_timer >= 0.38f) {
                bounce_phase = 3;
                bounce_timer = 0.0f;
            }
            break;
        }

        case 3: { // 【阶段 3: 高速砸地触地瞬间极致压扁 (Impact Trampoline Squash, 0.08s)】
            target_look_x = hx;
            target_look_y = hy;

            // 触地瞬间像蹦床一样再次极度压扁
            skeleton.setBounceDeform(0.36f, 1.82f);

            if (bounce_timer >= 0.08f) {
                bounce_jump_count++;
                if (bounce_jump_count >= bounce_max_jumps) {
                    // 弹跳表演圆满完成！解开球体形态，从容舒展
                    skeleton.setBouncingMode(false);
                    skeleton.setBounceDeform(1.0f, 1.0f);
                    enterState(STATE_OBSERVE, nullptr, &skeleton, hx, hy);
                } else {
                    // 立即进入下一轮更高更弹的反冲弹射！
                    bounce_phase = 0;
                    bounce_timer = 0.0f;
                }
            }
            break;
        }
    }
}

void CreatureAI::updateBatHang(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, PhysiologySystem &physiology, FluidSymbolSystem *fluid_symbols) {
    // 如果精力很低，进入深度睡眠（闭眼打呼噜 Zzz）
    if (physiology.getEnergy() < 0.4f) {
        is_bat_hang_sleeping = true;
        const_cast<PhysiologySystem&>(physiology).recoverEnergy(dt * 0.035f);

        // 倒挂睡眠也喷出 Zz 符号
        sleep_zz_timer += dt;
        if (sleep_zz_timer >= 6.0f) {
            sleep_zz_timer = 0.0f;
            if (fluid_symbols) {
                fluid_symbols->trigger("zz", hx + ((rand() % 14) - 7), hy + 18.0f);
            }
        }
    } else {
        is_bat_hang_sleeping = false;
    }
    
    target_look_x = hx;
    target_look_y = hy + 30.0f;

    if (state_timer >= state_duration) {
        skeleton.clearHangingAnchor();
        skeleton.setBatHangMode(false);
        tentacles.endCeilingSwing();
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::updateBallPlay(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, MetaballSystem &metaballs, ExpressionLayer &expression, PhysiologySystem &physiology, FluidSymbolSystem *fluid_symbols) {
    if (!symbiote_ball.active) {
        physiology.resetBoredom();
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
        return;
    }

    // 玩球剧烈运动：无聊感迅速归零，消耗体力，提升心情与舒适度
    physiology.reduceBoredom(dt * 0.25f);
    physiology.consumeEnergy(dt * 0.022f);
    physiology.applyStimulus(0.0f, dt * 0.035f);

    // 眼神始终注视小球
    target_look_x = symbiote_ball.x;
    target_look_y = symbiote_ball.y;

    // 1. 分裂萌芽阶段 (0.0s ~ 0.45s 快速膨胀，从尾部诞生)
    if (state_timer < 0.45f) {
        float grow_t = state_timer / 0.45f;
        symbiote_ball.radius = 2.0f + 6.0f * grow_t; // 从 2px 生长到 8px
    }

    // 冷却时间更新
    if (symbiote_ball.hit_cooldown > 0.0f) {
        symbiote_ball.hit_cooldown -= dt;
    }

    // ─────────────────────────────────────────────────────────────
    // 【阶段 A: 正常颠球空中飞行与击球阶段】
    // ─────────────────────────────────────────────────────────────
    if (symbiote_ball.phase == BALL_PLAYING) {
        // 重力加速 (结合用户倾斜 IMU 重力)
        float grav_x = last_imu_gx * 0.08f;
        float grav_y = 0.14f + last_imu_gy * 0.08f;

        symbiote_ball.vx += grav_x;
        symbiote_ball.vy += grav_y;

        // 速度限幅
        symbiote_ball.vx = std::max(-4.2f, std::min(4.2f, symbiote_ball.vx));
        symbiote_ball.vy = std::max(-6.2f, std::min(6.2f, symbiote_ball.vy));

        // 空气阻力
        symbiote_ball.vx *= 0.992f;
        symbiote_ball.vy *= 0.992f;

        symbiote_ball.x += symbiote_ball.vx * dt * 45.0f;
        symbiote_ball.y += symbiote_ball.vy * dt * 45.0f;

        float min_ball_x = symbiote_ball.radius + 3.0f;
        float max_ball_x = (float)SCREEN_W - symbiote_ball.radius - 3.0f;
        float min_ball_y = symbiote_ball.radius + 3.0f;
        float max_ball_y = (float)SCREEN_H - symbiote_ball.radius - 3.0f;

        // 左右边框高弹反弹
        if (symbiote_ball.x < min_ball_x) {
            symbiote_ball.x = min_ball_x;
            symbiote_ball.vx = std::max(1.8f, std::abs(symbiote_ball.vx) * 0.92f);
            metaballs.spawnDroplet(symbiote_ball.x + 3.0f, symbiote_ball.y, 1.2f, (rand()%20-10)*0.1f, 2.0f);
        } else if (symbiote_ball.x > max_ball_x) {
            symbiote_ball.x = max_ball_x;
            symbiote_ball.vx = -std::max(1.8f, std::abs(symbiote_ball.vx) * 0.92f);
            metaballs.spawnDroplet(symbiote_ball.x - 3.0f, symbiote_ball.y, -1.2f, (rand()%20-10)*0.1f, 2.0f);
        }

        // 天花板高弹反弹
        if (symbiote_ball.y < min_ball_y) {
            symbiote_ball.y = min_ball_y;
            symbiote_ball.vy = std::max(2.0f, std::abs(symbiote_ball.vy) * 0.90f);
            metaballs.spawnDroplet(symbiote_ball.x, symbiote_ball.y + 3.0f, (rand()%20-10)*0.1f, 1.2f, 2.0f);
        }

        // 地面保底反弹
        if (symbiote_ball.y > max_ball_y) {
            symbiote_ball.y = max_ball_y;
            symbiote_ball.vy = -std::max(2.2f, std::abs(symbiote_ball.vy) * 0.85f);
        }

        // 预判小球落点，平稳预判跑位 (带死区滞回，杜绝抽搐)
        float target_pos_x = symbiote_ball.x + symbiote_ball.vx * 3.5f;
        target_pos_x = std::max(32.0f, std::min((float)SCREEN_W - 32.0f, target_pos_x));

        float dx_ball = target_pos_x - hx;
        if (std::abs(dx_ball) > 18.0f) {
            // 偏差较大时平稳跑位
            skeleton.alignHeadingToTarget(target_pos_x, hy);
            skeleton.setCreepingTarget(target_pos_x, hy, 1.1f);
        } else if (std::abs(dx_ball) < 8.0f) {
            // 准确定位后平稳就位，等待击球
            skeleton.clearCreepingTarget();
        }

        // 颠球达到目标次数后，最后一下由触手凌空抓球 (BALL_SNATCH)
        if (symbiote_ball.bounce_count >= symbiote_ball.max_bounces) {
            // 当球下落到身体周围可抓取高度 (y >= 42.0f 且在下落) 时，触手闪电射出抓球！
            if (symbiote_ball.y >= 42.0f && symbiote_ball.vy > -0.5f) {
                symbiote_ball.phase = BALL_SNATCH;
                symbiote_ball.phase_timer = 0.0f;
                symbiote_ball.snatch_start_x = symbiote_ball.x;
                symbiote_ball.snatch_start_y = symbiote_ball.y;
                tentacles.triggerVolleyTentacle(hx, hy - 3.0f, symbiote_ball.x, symbiote_ball.y);
                skeleton.triggerLocalBleb(0, 1.6f);
                return;
            }
        }

        // 击球判定
        float dist_x = std::abs(dx_ball);
        float dy_head = hy - symbiote_ball.y; // 球在头部上方为正 (hy 约为 100~110)
        float total_dist = std::sqrt(dx_ball * dx_ball + (symbiote_ball.y - hy) * (symbiote_ball.y - hy));

        if (symbiote_ball.hit_cooldown <= 0.0f && symbiote_ball.y >= 52.0f && symbiote_ball.vy > 0.1f) {
            // 【判定 1: 头部近战正上方头槌起跳 (Header Smash)】
            if (dist_x <= 18.0f && dy_head >= 4.0f && dy_head <= 28.0f) {
                float head_impulse_x = dx_ball * 0.12f;
                skeleton.applyImpulse(head_impulse_x, -4.8f);
                skeleton.triggerLocalBleb(0, 1.6f);

                symbiote_ball.vy = -5.4f - (rand() % 10) * 0.1f;
                symbiote_ball.vx = dx_ball * 0.15f + ((rand() % 16) - 8) * 0.06f;

                symbiote_ball.bounce_count++;
                symbiote_ball.hit_cooldown = 0.45f;
                expression.triggerExpression(EXPR_CURIOSITY, 1.5f);
                metaballs.spawnDroplet(hx, hy - 10.0f, symbiote_ball.vx * 0.3f, -1.8f, 2.5f);
            }
            // 【判定 2: 前后侧向触手抽射 (Horizontal Whip & Volley)】
            else if (dist_x >= 20.0f && total_dist <= 95.0f && symbiote_ball.y >= 58.0f) {
                tentacles.triggerVolleyTentacle(hx, hy - 2.0f, symbiote_ball.x, symbiote_ball.y);
                skeleton.triggerLocalBleb(0, 1.5f);
                skeleton.triggerLocalBleb(1, 1.3f);

                float dir_to_center = (symbiote_ball.x < (float)SCREEN_W * 0.5f) ? 1.0f : -1.0f;
                symbiote_ball.vy = -5.5f - (rand() % 10) * 0.1f;
                symbiote_ball.vx = dir_to_center * (2.2f + (rand() % 14) * 0.1f);

                symbiote_ball.bounce_count++;
                symbiote_ball.hit_cooldown = 0.45f;
                expression.triggerExpression(EXPR_TRUST, 1.5f);
                metaballs.spawnDroplet(symbiote_ball.x, symbiote_ball.y, symbiote_ball.vx * 0.2f, -2.0f, 2.8f);
            }
        }
    }
    // ─────────────────────────────────────────────────────────────
    // 【阶段 B: 触手凌空伸出抓住小球 (BALL_SNATCH，0.25s)】
    // ─────────────────────────────────────────────────────────────
    else if (symbiote_ball.phase == BALL_SNATCH) {
        symbiote_ball.phase_timer += dt;
        // 小球在空中减速滞空，等待触手爪尖到达
        symbiote_ball.vx *= 0.92f;
        symbiote_ball.vy = std::max(-0.5f, std::min(1.2f, symbiote_ball.vy + dt * 1.5f));
        symbiote_ball.x += symbiote_ball.vx * dt * 45.0f;
        symbiote_ball.y += symbiote_ball.vy * dt * 45.0f;

        // 持续引导触手朝向小球抓取
        tentacles.triggerVolleyTentacle(hx, hy - 3.0f, symbiote_ball.x, symbiote_ball.y);

        if (symbiote_ball.phase_timer >= 0.25f) {
            // 触手死死抓住小球，开启拉回阶段
            symbiote_ball.phase = BALL_RETRACT;
            symbiote_ball.phase_timer = 0.0f;
            symbiote_ball.snatch_start_x = symbiote_ball.x;
            symbiote_ball.snatch_start_y = symbiote_ball.y;
            skeleton.triggerLocalBleb(0, 1.6f);
            expression.triggerExpression(EXPR_TRUST, 3.5f);
            metaballs.spawnDroplet(symbiote_ball.x, symbiote_ball.y, ((rand()%20)-10)*0.1f, -1.5f, 2.5f);
        }
    }
    // ─────────────────────────────────────────────────────────────
    // 【阶段 C: 触手抓紧小球强力拉回身体 (BALL_RETRACT，0.45s)】
    // ─────────────────────────────────────────────────────────────
    else if (symbiote_ball.phase == BALL_RETRACT) {
        symbiote_ball.phase_timer += dt;
        float t = std::min(1.0f, symbiote_ball.phase_timer / 0.45f);
        // EaseOutCubic 平滑拉扯曲线
        float ease = 1.0f - std::pow(1.0f - t, 3.0f);

        // 小球位置被触手稳稳拉向头部
        symbiote_ball.x = symbiote_ball.snatch_start_x + (hx - symbiote_ball.snatch_start_x) * ease;
        symbiote_ball.y = symbiote_ball.snatch_start_y + ((hy - 3.0f) - symbiote_ball.snatch_start_y) * ease;

        // 触手始终连接头部与小球
        tentacles.triggerVolleyTentacle(hx, hy - 3.0f, symbiote_ball.x, symbiote_ball.y);

        if (symbiote_ball.phase_timer >= 0.45f) {
            // 小球抵达身体肉体表面，触手收回，开启液态融合
            symbiote_ball.phase = BALL_FUSING;
            symbiote_ball.phase_timer = 0.0f;
            symbiote_ball.snatch_start_x = symbiote_ball.x;
            symbiote_ball.snatch_start_y = symbiote_ball.y;
            skeleton.triggerLocalBleb(0, 1.8f);
        }
    }
    // ─────────────────────────────────────────────────────────────
    // 【阶段 D: 慢速液态融合、肉体吞噬与体积收缩 (BALL_FUSING，0.75s)】
    // ─────────────────────────────────────────────────────────────
    else if (symbiote_ball.phase == BALL_FUSING) {
        symbiote_ball.phase_timer += dt;
        float t = std::min(1.0f, symbiote_ball.phase_timer / 0.75f); // 0.0 -> 1.0

        // 小球平滑沉入头部核心深处
        symbiote_ball.x = symbiote_ball.snatch_start_x + (hx - symbiote_ball.snatch_start_x) * (t * 0.7f);
        symbiote_ball.y = symbiote_ball.snatch_start_y + (hy - symbiote_ball.snatch_start_y) * (t * 0.7f);

        // 小球半径从 8.0px 平滑收缩融化为 0.0px (Metaball 标量场自然呈现液桥与肉体融合变形)
        symbiote_ball.radius = 8.0f * (1.0f - t * t);

        // 头部与颈部肉体随融合产生持续吞咽鼓包与流体涟漪
        if (fmod(symbiote_ball.phase_timer, 0.16f) < dt) {
            skeleton.triggerLocalBleb(0, 1.5f * (1.0f - t));
            skeleton.triggerLocalBleb(1, 1.2f * (1.0f - t));
            metaballs.spawnDroplet(symbiote_ball.x, symbiote_ball.y, ((rand()%20)-10)*0.06f, ((rand()%20)-10)*0.06f, 2.0f);
        }

        // 融合彻底完成
        if (symbiote_ball.phase_timer >= 0.75f || symbiote_ball.radius <= 0.2f) {
            symbiote_ball.active = false;
            metaballs.triggerSpikeBurst(4, 1.2f);
            skeleton.triggerLocalBleb(0, 1.6f);

            if (fluid_symbols) {
                float sym_x = std::max(45.0f, std::min((float)SCREEN_W - 45.0f, hx));
                float sym_y = (hy > 65.0f) ? (hy - 45.0f) : (hy + 45.0f);
                fluid_symbols->trigger("heart", sym_x, sym_y);
            }

            physiology.resetBoredom();
            enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
            return;
        }
    }
}

void CreatureAI::updateSleep(float dt, float hx, float hy, const PhysiologySystem &physiology, FluidSymbolSystem *fluid_symbols) {
    // 睡眠中深度平缓恢复体力 (+0.045/s) 且重置无聊度
    const_cast<PhysiologySystem&>(physiology).recoverEnergy(dt * 0.045f);
    const_cast<PhysiologySystem&>(physiology).resetBoredom();

    target_look_x = hx;
    target_look_y = hy + 10.0f;

    // 睡眠中偶尔喷出代表睡眠的大小不一 Zz 符号 (每 5.5s 周期)
    sleep_zz_timer += dt;
    if (sleep_zz_timer >= 5.5f) {
        sleep_zz_timer = 0.0f;
        if (fluid_symbols) {
            fluid_symbols->trigger("zz", hx + ((rand() % 14) - 7), hy - 14.0f);
        }
    }

    // 睡眠结束条件：数值闭环！体力恢复到 0.90 满血睡饱自然苏醒，或受到外界剧烈应激 (无需硬编码时间)！
    if (physiology.getEnergy() >= 0.90f || physiology.getStress() > 0.45f) {
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
                        const PreyBugSystem *bugs,
                        FluidSymbolSystem *fluid_symbols) {
    state_timer += dt;

    updateOrganicBreathing(dt, physiology, expression);
    updateMicroBehaviors(dt, skeleton, physiology);

    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    // 表达层视觉效果由 ExpressionLayer 独立渲染，不再劫持 AI 状态机转换
    // (原表达劫持会阻断 OBSERVE→CREEP/CRAWL 等正常状态转换，造成 OBSERVE↔EXPRESS 死循环)

    switch (current_state) {
        case STATE_HESITATING: updateHesitating(dt, hx, hy, expression); break;
        case STATE_IDLE:       updateIdle(dt, hx, hy, physiology, relationship, tentacles, skeleton); break;
        case STATE_CREEP:      updateCreep(dt, hx, hy, physiology, tentacles, skeleton); break;
        case STATE_CRAWL:      updateCrawl(dt, hx, hy, physiology, tentacles, skeleton); break;
        case STATE_OBSERVE:    updateObserve(dt, hx, hy, physiology, tentacles, skeleton); break;
        case STATE_SWING:      updateSwing(dt, hx, hy, skeleton, tentacles); break;
        case STATE_CATCH_DUST: updateCatchDust(dt, hx, hy, skeleton, tentacles, expression, physiology, bugs, fluid_symbols); break;
        case STATE_ROLL:       updateRoll(dt, hx, hy, skeleton, expression, physiology, &metaballs); break;
        case STATE_BOUNCE:     updateBounce(dt, hx, hy, skeleton, metaballs, physiology, expression); break;
        case STATE_BAT_HANG:   updateBatHang(dt, hx, hy, skeleton, tentacles, physiology, fluid_symbols); break;
        case STATE_BALL_PLAY:  updateBallPlay(dt, hx, hy, skeleton, tentacles, metaballs, expression, physiology, fluid_symbols); break;
        case STATE_SLEEP:      updateSleep(dt, hx, hy, physiology, fluid_symbols); break;
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

    // 猎物感知与好奇环绕/眼球注视锁定 (Stalking & Curious Circling)
    if (bugs && bugs->hasActiveBug() && current_state != STATE_SLEEP && current_state != STATE_STARTLED && current_state != STATE_BALL_PLAY && current_state != STATE_SWING) {
        float bx, by;
        BugState b_state;
        int bug_idx = bugs->getNearestBug(hx, hy, bx, by, b_state);
        if (bug_idx >= 0 && b_state != BUG_DEAD) {
            target_look_x = bx;
            target_look_y = by;

            // 饱腹状态下 (energy >= 0.58)，若正处于发呆或慢速蠕动状态，主动进入好奇环绕小虫巡游观察！
            if (physiology.getEnergy() >= 0.58f && (current_state == STATE_IDLE || current_state == STATE_OBSERVE || current_state == STATE_CREEP)) {
                static float bug_circle_timer = 0.0f;
                bug_circle_timer += dt;
                if (bug_circle_timer > 1.0f) {
                    bug_circle_timer = 0.0f;
                    float cur_ang = std::atan2(hy - by, hx - bx);
                    float next_ang = cur_ang + 0.70f; // 环绕切向角
                    float orbit_r = 44.0f;
                    crawl_target_x = std::max(22.0f, std::min((float)SCREEN_W - 22.0f, bx + std::cos(next_ang) * orbit_r));
                    crawl_target_y = std::max(22.0f, std::min((float)SCREEN_H - 22.0f, by + std::sin(next_ang) * orbit_r));
                    if (current_state == STATE_IDLE || current_state == STATE_OBSERVE) {
                        enterState(STATE_CREEP, &tentacles, &skeleton, hx, hy);
                    }
                }
            }
        }
    }

    // 睡眠惺忪半睁眼倒计时
    if (is_sleep_peeking) {
        sleep_peek_timer -= dt;
        if (sleep_peek_timer <= 0.0f) {
            sleep_peek_timer = 0.0f;
            is_sleep_peeking = false;
        }
    }
}

void CreatureAI::handleSingleTap(FluidSymbolSystem &symbols, ExpressionLayer &expr, PhysiologySystem &phys) {
    if (isSleeping()) {
        // 睡眠中轻敲：缓慢半睁眼（微眯惺忪状态），不惊醒，若无后续事件 2.8 秒后继续深睡
        is_sleep_peeking = true;
        sleep_peek_timer = 2.8f;
        target_look_x = SCREEN_W * 0.5f;
        target_look_y = SCREEN_H * 0.5f;
    } else {
        // 醒着时轻敲一下：引起注意，眼睛注视屏幕中心，轻微好奇
        phys.applyStimulus(0.12f, 0.15f);
        target_look_x = SCREEN_W * 0.5f + ((rand() % 40) - 20);
        target_look_y = SCREEN_H * 0.5f + ((rand() % 40) - 20);
    }
}

void CreatureAI::handleDoubleTap(FluidSymbolSystem &symbols, ExpressionLayer &expr, PhysiologySystem &phys,
                                 SkeletonSystem &skeleton, TentacleRenderer &tentacles) {
    if (isSleeping()) {
        // 睡眠中双击：轻柔唤醒
        is_sleep_peeking = false;
        sleep_peek_timer = 0.0f;
        enterState(STATE_OBSERVE, &tentacles, &skeleton, skeleton.getNode(0).x, skeleton.getNode(0).y);
        float sym_x = std::max(45.0f, std::min((float)SCREEN_W - 45.0f, skeleton.getNode(0).x));
        float sym_y = (skeleton.getNode(0).y > 65.0f) ? (skeleton.getNode(0).y - 48.0f) : (skeleton.getNode(0).y + 45.0f);
        symbols.trigger("?", sym_x, sym_y);
        expr.triggerExpression(EXPR_CURIOSITY, 3.5f);
        return;
    }

    // 闲置/清醒时双击设备：与主人亲密互动！
    // 随机 3 种惊喜：
    // 1. 主动荡秋千（延长至 22 秒，尽情玩耍）+ 吐出爱心
    // 2. 喷出水墨爱心图腾 (宽敞开阔处漂浮)
    // 3. 喷出水墨问号图腾
    float hx = skeleton.getNode(0).x;
    float hy = skeleton.getNode(0).y;
    float sym_x = std::max(45.0f, std::min((float)SCREEN_W - 45.0f, hx));
    float sym_y = (hy > 65.0f) ? (hy - 48.0f) : (hy + 45.0f);
    int choice = rand() % 3;

    if (choice == 0) {
        // 1. 主动进入荡秋千状态（不喷符号），延长秋千时间至 22.0 秒！
        enterState(STATE_SWING, &tentacles, &skeleton, hx, hy);
        state_duration = 22.0f; // 延长荡秋千时间，尽情摇摆！
        expr.triggerExpression(EXPR_TRUST, 4.0f);
        phys.applyStimulus(0.0f, 0.50f);
    } else if (choice == 1) {
        // 2. 喷出水墨爱心图腾 (与身体拉开 48px 空间，绝不被肉身融合)
        symbols.trigger("heart", sym_x, sym_y);
        expr.triggerExpression(EXPR_TRUST, 4.0f);
        phys.applyStimulus(0.0f, 0.45f); // 提升 comfort
    } else {
        // 3. 喷出水墨问号图腾
        symbols.trigger("?", sym_x, sym_y);
        expr.triggerExpression(EXPR_CURIOSITY, 4.0f);
        phys.applyStimulus(0.0f, 0.35f);
    }
}

void CreatureAI::handleMultiTapIrritate(FluidSymbolSystem &symbols, ExpressionLayer &expr, PhysiologySystem &phys,
                                       SkeletonSystem &skeleton) {
    // 连续多次敲击（激惹骚扰）：毒液感到强烈烦躁与愤怒！
    is_sleep_peeking = false;
    sleep_peek_timer = 0.0f;

    // 强行注入最高应激与神经张力 -> 触发 EMOTION_ANGER -> 表皮亮起青色荧光神经脉冲！
    phys.applyStimulus(0.95f, 0.95f);
    expr.triggerExpression(EXPR_WARNING, 4.5f);

    float hx = skeleton.getNode(0).x;
    float hy = skeleton.getNode(0).y;
    float sym_x = std::max(45.0f, std::min((float)SCREEN_W - 45.0f, hx));
    float sym_y = (hy > 65.0f) ? (hy - 48.0f) : (hy + 45.0f);

    // 喷出感叹号 "!"
    symbols.trigger("!", sym_x, sym_y);

    // 触发身体局部鼓包应力突变与受惊震颤
    skeleton.triggerLocalBleb(1, 1.5f);
    skeleton.triggerLocalBleb(2, 1.5f);
    skeleton.triggerLocalBleb(3, 1.5f);
    skeleton.applyImpulse(((rand() % 60) - 30) * 0.1f, -1.8f);
    triggerStartle(1.5f);
}
