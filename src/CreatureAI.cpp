#include "CreatureAI.h"
#include <cmath>

CreatureAI::CreatureAI() {}

void CreatureAI::init() {
    enterState(STATE_OBSERVE);
    state_duration = 1.2f; // 开机短暂观察 1.2s 即刻精神饱满出发探索自娱自乐！
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
        case STATE_PEEK:       return "PEEK";
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

CreatureState CreatureAI::decideNextState(CreatureState from_state, float hx, float hy, const PhysiologySystem &physiology) {
    float energy = physiology.getEnergy();
    float boredom = physiology.getBoredom();
    bool is_vertical = (std::abs(last_imu_gx) > 3.0f || std::abs(last_imu_gy) > 6.5f);
    bool near_ceiling = (hy < 45.0f);
    bool near_edge = (hx < 35.0f || hx > (float)SCREEN_W - 35.0f || hy > (float)SCREEN_H - 35.0f || hy < 35.0f);
    int roll = rand() % 100;

    // 【1. 潜行机警状态出度 (从 PEEK 暗中观察退出)】
    if (from_state == STATE_PEEK) {
        // 刚在边缘暗中观察完毕：严禁突兀地直接玩蹦床跳跃或高速翻滚！
        // 必须维持生物动机的连续性：顺势沿边滑出漫步、或伸出触手大步出击、或探头就地警惕打量
        if (roll < 65) {
            return STATE_CREEP; // 65% 顺势沿边缘滑出漫步巡游
        } else if (roll < 85) {
            return STATE_CRAWL; // 20% 发现目标，射出粗触手向腹地开阔区大步攀爬
        } else {
            return STATE_OBSERVE; // 15% 探头就地观察
        }
    }

    // 【2. 地表缓慢巡游出度 (从 CREEP 退出)】
    if (from_state == STATE_CREEP) {
        if (roll < 45) {
            return STATE_OBSERVE; // 45% 巡游就位后停下脚步警惕打量四周
        } else if (near_edge && roll < 70) {
            return STATE_PEEK;    // 25% 靠近边框时顺势缩回暗中观察
        } else if (energy > 0.5f && roll < 88) {
            return STATE_CATCH_DUST; // 18% 发现漂浮尘埃，突然爆发飞扑
        } else {
            return STATE_IDLE;    // 12% 从容发呆平复呼吸
        }
    }

    // 【3. 大步快速攀爬出度 (从 CRAWL 退出)】
    if (from_state == STATE_CRAWL) {
        if (near_ceiling) {
            // 到达天花板附近
            if (roll < 45) {
                return STATE_SWING;    // 45% 顺手射出蛛丝荡秋千
            } else if (roll < 75) {
                return STATE_BAT_HANG; // 30% 倒挂金钩发呆
            } else {
                return STATE_OBSERVE;  // 25% 贴顶观察
            }
        } else {
            // 在地面或开阔中段
            if (roll < 35) {
                return STATE_CATCH_DUST; // 35% 飞扑抓尘
            } else if (roll < 60) {
                return STATE_OBSERVE;    // 25% 驻足观察
            } else if (energy > 0.6f && roll < 80) {
                return STATE_BOUNCE;     // 20% 兴奋起跳玩蹦床
            } else {
                return STATE_CREEP;      // 20% 转为贴地潜行
            }
        }
    }

    // 【4. 高能弹跳特技出度 (从 BOUNCE 退出)】
    if (from_state == STATE_BOUNCE) {
        // 连续剧烈高弹跳跃落地后，动能巨大且体力消耗
        if (roll < 45) {
            return STATE_OBSERVE;    // 45% 砸地平稳，歪头喘息舒展身体
        } else if (roll < 70) {
            return STATE_ROLL;       // 25% 落地余速顺势蜷缩成球地表高速翻滚（动量守恒）
        } else if (energy > 0.6f && roll < 90) {
            return STATE_BALL_PLAY;  // 20% 极度兴奋，吐出共生小球自娱自乐颠球
        } else {
            return STATE_IDLE;       // 10% 原地喘息发呆
        }
    }

    // 【5. 软体球形翻滚出度 (从 ROLL 退出)】
    if (from_state == STATE_ROLL) {
        if (roll < 50) {
            return STATE_OBSERVE;    // 50% 舒展身体平复张望
        } else if (roll < 80) {
            return STATE_CREEP;      // 30% 解开球形，顺势地表游走
        } else {
            return STATE_BOUNCE;     // 20% 借力弹跳起飞
        }
    }

    // 【6. 自体颠球吞噬融合出度 (从 BALL_PLAY 退出)】
    if (from_state == STATE_BALL_PLAY) {
        if (roll < 60) {
            return STATE_OBSERVE;    // 60% 吞咽后心满意足环顾四周
        } else if (roll < 85) {
            return STATE_IDLE;       // 25% 平静休息消化
        } else {
            return STATE_CREEP;      // 15% 漫步巡视领地
        }
    }

    // 【7. 高空荡秋千出度 (从 SWING 退出)】
    if (from_state == STATE_SWING) {
        if (hy < 50.0f && roll < 45) {
            return STATE_BAT_HANG;   // 45% 滞留高空时倒挂小憩
        } else if (roll < 75) {
            return STATE_OBSERVE;    // 30% 落地张望
        } else {
            return STATE_ROLL;       // 25% 落地顺势受身翻滚
        }
    }

    // 【8. 飞扑抓浮尘出度 (从 CATCH_DUST 退出)】
    if (from_state == STATE_CATCH_DUST) {
        if (roll < 55) {
            return STATE_OBSERVE;    // 55% 歪头困惑注视
        } else if (roll < 85) {
            return STATE_CRAWL;      // 30% 没抓到，迈步向远处探索
        } else {
            return STATE_CREEP;      // 15% 贴地游走搜寻
        }
    }

    // 【9. 倒挂金钩出度 (从 BAT_HANG 退出)】
    if (from_state == STATE_BAT_HANG) {
        if (roll < 50) {
            return STATE_SWING;      // 50% 倒挂清醒后顺势荡秋千
        } else {
            return STATE_OBSERVE;    // 50% 落地观察
        }
    }

    // 【10. 观察与发呆中枢出度 (从 OBSERVE / IDLE 退出)】
    // 此处必须依赖 last_state 与生理参数，避免突兀乱跳！
    
    // 10.1 疲惫乏力（energy < 0.35）：严禁高能剧烈运动！
    if (energy < 0.35f) {
        if (near_ceiling && roll < 50) return STATE_BAT_HANG;
        if (roll < 60) return STATE_CREEP;
        return STATE_IDLE;
    }

    // 10.2 若前序状态是潜伏系 (PEEK 或 CREEP)，维持机警潜行连续性，严禁直接蹦床翻滚！
    if (last_state == STATE_PEEK || last_state == STATE_CREEP) {
        if (roll < 45) {
            return (last_state == STATE_PEEK) ? STATE_CREEP : STATE_PEEK; // 潜行互转
        } else if (roll < 75) {
            return STATE_CRAWL;      // 30% 伸出大触手探索，打破潜行
        } else {
            return STATE_CATCH_DUST; // 25% 发现动静暴起飞扑
        }
    }

    // 10.3 高能玩耍决策 (精力充足 energy >= 0.55 且无聊度高)
    if (energy >= 0.55f && (boredom >= 0.20f || roll < 65)) {
        if (is_vertical) {
            // 竖屏：优先垂直跳跃与高空动作
            if (roll < 28) return STATE_BOUNCE;
            if (roll < 52) return STATE_BALL_PLAY;
            if (roll < 72) return STATE_SWING;
            if (roll < 88) return STATE_CRAWL;
            return STATE_CATCH_DUST;
        } else {
            // 横屏生态
            if (roll < 22) return STATE_BOUNCE;
            if (roll < 42) return STATE_BALL_PLAY;
            if (roll < 60) return STATE_ROLL;
            if (roll < 75) return STATE_CATCH_DUST;
            if (roll < 90) return STATE_CRAWL;
            return STATE_CREEP;
        }
    }

    // 10.4 默认稳妥探索
    if (near_edge && roll < 40) return STATE_PEEK;
    if (roll < 60) return STATE_CRAWL;
    return STATE_CREEP;
}

void CreatureAI::enterState(CreatureState new_state, TentacleRenderer *tentacles, SkeletonSystem *skeleton, float hx, float hy) {
    if (new_state != current_state) {
        last_state = current_state;
    }
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
    if (skeleton && new_state != STATE_PEEK) {
        skeleton->setPeekingMode(false);
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

            // 边缘吸附漫游：只沿屏幕边缘（主要底边）巡游，拒绝在屏幕中央悬浮（避免飞碟感）
            int edge_choice = rand() % 100;
            if (edge_choice < 75) {
                // 75% 概率沿底边横向紧贴爬行
                float step_dist = 40.0f + (rand() % 80);
                crawl_target_x = std::max(25.0f, std::min((float)SCREEN_W - 25.0f, hx + dir_sign * step_dist));
                crawl_target_y = (float)SCREEN_H - 5.0f; // 目标设在更深处，确保紧紧吸附底边产生完美平坦面
            } else if (edge_choice < 90) {
                // 15% 概率沿左侧或右侧边框游走
                crawl_target_x = (hx < SCREEN_W / 2) ? 5.0f : ((float)SCREEN_W - 5.0f);
                float step_dist = 30.0f + (rand() % 50);
                float y_dir = ((rand() % 100) < 50) ? 1.0f : -1.0f;
                crawl_target_y = std::max(25.0f, std::min((float)SCREEN_H - 25.0f, hy + y_dir * step_dist));
            } else {
                // 10% 概率沿天花板游走
                float step_dist = 40.0f + (rand() % 80);
                crawl_target_x = std::max(25.0f, std::min((float)SCREEN_W - 25.0f, hx + dir_sign * step_dist));
                crawl_target_y = 5.0f; // 紧贴顶边
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
            crawl_step_count = 0;

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
            state_duration = 660.0f + (rand() % 300); // 11~16分钟超长安稳深度睡眠（660~960秒）
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            sleep_zz_timer = 1.0f; // 入睡后 1.0s 立即从口鼻飘出第一串 Zz
            is_sleep_peeking = false;
            sleep_peek_timer = 0.0f;
            sleep_peek_cooldown = 4.0f; // 刚入睡 4s 内免打扰静默
            if (skeleton) {
                skeleton->clearPullTarget();
                skeleton->clearCreepingTarget();
            }
            if (tentacles) {
                tentacles->setCreepMode(false);
            }
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
            state_duration = 3.2f; // 0.50s 紧绷压低蓄力 + 0.15s 闪电破空暴冲 + 0.20s 猛烈砸地抓扁 + 2.35s 困惑歪头吐问号
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            crawl_target_x = std::max(28.0f, std::min((float)SCREEN_W - 28.0f, hx + ((rand() % 2 == 0) ? (50.0f + (rand() % 40)) : -(50.0f + (rand() % 40)))));
            crawl_target_y = std::max(22.0f, hy - (15.0f + (rand() % 35)));
            target_look_x = crawl_target_x;
            target_look_y = crawl_target_y;
            if (skeleton) {
                skeleton->setBouncingMode(true);
                skeleton->setBounceDeform(0.45f, 1.42f); // 初始紧绷压扁蓄力
            }
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

        case STATE_PEEK: {
            // 【边缘暗中观察/潜行窥视模式】
            state_duration = 9.0f + (rand() % 40) * 0.1f; // 9.0 ~ 13.0s
            crawl_force_x = 0.0f;
            crawl_force_y = 0.0f;
            
            // 找出最近的边 (0:Top, 1:Right, 2:Bottom, 3:Left)
            float dist_top = hy;
            float dist_right = (float)SCREEN_W - hx;
            float dist_bottom = (float)SCREEN_H - hy;
            float dist_left = hx;
            
            float min_dist = dist_top;
            peek_edge = 0;
            if (dist_right < min_dist) { min_dist = dist_right; peek_edge = 1; }
            if (dist_bottom < min_dist) { min_dist = dist_bottom; peek_edge = 2; }
            if (dist_left < min_dist) { min_dist = dist_left; peek_edge = 3; }
            
            peek_target_x = hx;
            peek_target_y = hy;
            if (peek_edge == 0 || peek_edge == 2) {
                peek_target_x = std::max(40.0f, std::min((float)SCREEN_W - 40.0f, hx));
            } else {
                peek_target_y = std::max(40.0f, std::min((float)SCREEN_H - 40.0f, hy));
            }

            peek_move_dir = ((rand() % 100) < 50) ? 1.0f : -1.0f;
            peek_raise_timer = 0.0f;
            peek_submerge_offset = 0.0f;
            peek_is_raised = false;
            peek_raise_interval = 2.0f + (rand() % 20) * 0.1f;
            if (skeleton) {
                skeleton->setPeekingMode(true, 0.0f, peek_edge);
                // 立即施加朝向该边缘外的牵引初冲量，使毒液迅速平滑钻入边框深处藏好
                float pull_x = 0.0f;
                float pull_y = 0.0f;
                if (peek_edge == 0) pull_y = -18.0f;
                else if (peek_edge == 1) pull_x = 18.0f;
                else if (peek_edge == 2) pull_y = 18.0f;
                else if (peek_edge == 3) pull_x = -18.0f;
                skeleton->applyImpulse(pull_x, pull_y);
            }
            if (tentacles) {
                tentacles->setCreepMode(true, 0.85f); // 增强扒边感
            }
            break;
        }
    }
}

void CreatureAI::triggerStartle(float intensity, SkeletonSystem *skeleton, TentacleRenderer *tentacles) {
    // 荡秋千/倒挂等悬挂玩耍模式下，绝对豁免受惊打断！
    if (current_state == STATE_SWING || current_state == STATE_BAT_HANG) return;
    startle_energy = intensity;
    enterState(STATE_STARTLED, tentacles, skeleton);
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

void CreatureAI::triggerInteraction(SkeletonSystem *skeleton, TentacleRenderer *tentacles) {
    if (current_state == STATE_SLEEP) {
        enterState(STATE_OBSERVE);
    } else if (current_state == STATE_IDLE) {
        enterHesitation(STATE_CRAWL, 0.12f);
    } else {
        triggerStartle(0.7f, skeleton, tentacles);
    }
}

void CreatureAI::updateSensors(float imu_gx, float imu_gy, float imu_gz, const PhysiologySystem &physiology, bool btn_a_pressed, SkeletonSystem *skeleton, TentacleRenderer *tentacles) {
    last_imu_gx = imu_gx;
    last_imu_gy = imu_gy;

    if (btn_a_pressed) {
        triggerInteraction(skeleton, tentacles);
        return;
    }

    // 荡秋千/倒挂等悬挂玩耍模式下，用户倾斜、翻转、晃动是核心交互，不触发任何惊吓打断！
    if (current_state == STATE_SWING || current_state == STATE_BAT_HANG) {
        return;
    }

    float total_g = std::sqrt(imu_gx * imu_gx + imu_gy * imu_gy + imu_gz * imu_gz);
    float delta_g = std::abs(total_g - last_total_g);
    last_total_g = total_g;

    // 【多级睡眠扰动感知体系 (10+分钟深度睡眠抗扰)】
    if (isSleeping()) {
        // 1. 剧烈大动静：强力猛甩/猛摔 (>2.7g 或 delta > 1.1g) 或 极高爆鸣巨响 (>82dB 伴随 AudioHigh) -> 彻底惊醒！
        if (total_g > 2.7f || delta_g > 1.10f || (physiology.getMicDecibels() > 82.0f && physiology.getAudioHigh() > 0.82f)) {
            is_sleep_peeking = false;
            sleep_peek_timer = 0.0f;
            triggerStartle(1.2f, skeleton, tentacles);
            Serial.println("[AI] Symbiote startled wide awake from sleep by violent shock/explosion!");
            return;
        }

        // 2. 轻微小动静判定（需避开环境日常底噪 40~52dB 与传感器固有抖动）：
        // 晃动阈值：total_g > 1.68g 或 delta_g > 0.48g；声音阈值：Mic > 72.0dB (人声说话/清脆响声)
        if (sleep_peek_cooldown <= 0.0f && (total_g > 1.68f || delta_g > 0.48f || physiology.getMicDecibels() > 72.0f)) {
            if (!is_sleep_peeking) {
                is_sleep_peeking = true;
                sleep_peek_timer = 3.5f;     // 半睁眼张望 3.5 秒
                sleep_peek_cooldown = 8.5f;  // 探视冷却 8.5 秒，避免环境轻微杂音造成闭眼后立刻重复秒睁眼！
                target_look_x = 40.0f + (rand() % (SCREEN_W - 80));
                target_look_y = 30.0f + (rand() % (SCREEN_H - 60));
                Serial.println("[AI] Symbiote lightly disturbed: drowsily peeking with half-open eye...");
            }
        }
        return; // 无动静保持安心深睡
    }

    float dynamic_threshold = (current_state == STATE_SWING || current_state == STATE_BAT_HANG || current_state == STATE_ROLL) ? 3.5f : 2.2f;
    
    if (total_g > dynamic_threshold && current_state != STATE_STARTLED && current_state != STATE_JOLTING) {
        triggerStartle(1.2f, skeleton, tentacles);
        return;
    }

    if (physiology.getAudioHigh() > 0.85f && physiology.getMicDecibels() > 82.0f &&
        current_state != STATE_STARTLED && current_state != STATE_JOLTING) {
        triggerStartle(0.9f, skeleton, tentacles);
        return;
    }
}

void CreatureAI::updateOrganicBreathing(float dt, const PhysiologySystem &physiology, const ExpressionLayer &expression) {
    // 自然深沉共生体生命呼吸律动 (一次完整呼吸 6.5 ~ 9.0 秒，极其平缓悠长舒展)
    float base_speed = 0.90f; // 0.9 rad/s 对应周期约 7.0 秒
    if (current_state == STATE_SLEEP) {
        base_speed = 0.38f;   // 睡眠时深长低频呼吸 (周期约 16.5 秒，极度安详)
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

    // 呼吸起伏幅度调整：降低日常呼吸，放大睡眠呼吸以凸显安睡特征
    float resp_amp = (current_state == STATE_SLEEP) ? 0.080f : 0.030f;
    respiration_factor = raw_resp * resp_amp;
}

void CreatureAI::updateMicroBehaviors(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology) {
    if (current_state == STATE_SLEEP) return; // 沉睡时彻底停止任何局部偶发应激鼓包，维持皮肤如平静水面

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

    // 静止静养：发呆时自然微弱恢复体力 (+0.015/s)
    const_cast<PhysiologySystem&>(physiology).recoverEnergy(dt * 0.015f);

    float energy = physiology.getEnergy();
    float boredom = physiology.getBoredom();
    float curiosity = physiology.getCuriosity();
    bool is_vertical = (std::abs(last_imu_gx) > 3.0f || std::abs(last_imu_gy) > 6.5f);

    // 【1. 真正疲惫力竭入睡驱动 (Energy < 0.15 且必须无互动保鲜期 interaction_wake_timer <= 0)】：
    // 只有在主人长时间不理它（发呆停留 8.0 秒以上）且精力确实枯竭时才自然入睡，互动期间绝对禁止秒睡！
    if (energy < 0.15f && interaction_wake_timer <= 0.0f) {
        if (state_timer >= 8.0f || energy < 0.06f) {
            if (hy < 35.0f && (rand() % 100) < 40) {
                enterState(STATE_BAT_HANG, &tentacles, &skeleton, hx, hy); // 靠近天花板时倒挂小憩
            } else {
                enterState(STATE_SLEEP, &tentacles, &skeleton, hx, hy);    // 蜷缩闭眼深睡
            }
            return;
        }
    }

    // 【2. 好奇与警惕交织驱动 (Curiosity > 0.60 && Stress > 0.38)】：边缘暗中观察/窥视
    if (curiosity > 0.60f && (physiology.getStress() > 0.38f || physiology.getAttachment() < 0.35f) && (rand() % 100) < 65) {
        enterState(STATE_PEEK, &tentacles, &skeleton, hx, hy);
        return;
    }

    // 【3. 自主活跃探索与特技游玩决策 (依据行为链与上下文决策)】
    if (state_timer >= state_duration || boredom >= 0.25f) {
        const_cast<PhysiologySystem&>(physiology).reduceBoredom(0.25f);
        CreatureState nxt = decideNextState(current_state, hx, hy, physiology);
        enterState(nxt, &tentacles, &skeleton, hx, hy);
        return;
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

    // 蠕动到达目标或超时，转入后续动作
    if (dist <= 8.0f || state_timer >= state_duration) {
        skeleton.clearCreepingTarget();
        tentacles.setCreepMode(false);
        CreatureState nxt = decideNextState(STATE_CREEP, hx, hy, physiology);
        enterState(nxt, &tentacles, &skeleton, hx, hy);
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
        if (crawl_shoot_timer >= 0.05f && dist > 12.0f) { // 缩短射出间隔，提升频率
            crawl_shoot_timer = 0.0f;
            float step_len = std::min(dist, 140.0f); // 极大地增加迈步幅度，允许一步跨越半个屏幕
            
            // 加入些许随机偏移，防止完全走直线显得生硬且总是瞄准同一个像素
            float dx_rand = dx + (rand() % 40 - 20);
            float dy_rand = dy + (rand() % 40 - 20);
            float dist_rand = std::sqrt(dx_rand * dx_rand + dy_rand * dy_rand);
            if (dist_rand > 0.01f) {
                dx_rand /= dist_rand;
                dy_rand /= dist_rand;
            } else {
                dx_rand = dx / dist;
                dy_rand = dy / dist;
            }

            float step_x = hx + dx_rand * step_len;
            float step_y = hy + dy_rand * step_len;
            
            // 限制在屏幕内
            step_x = std::max(10.0f, std::min((float)SCREEN_W - 10.0f, step_x));
            step_y = std::max(10.0f, std::min((float)SCREEN_H - 10.0f, step_y));

            tentacles.startGrappleCrawl(hx, hy, step_x, step_y);
            crawl_step_count++;
        }
    } else {
        crawl_shoot_timer = 0.0f;
    }

    // 爬行到达目标或超时或达到步数上限，依据物理位置与行为链智能转入后续动作
    if ((dist <= 16.0f && !tentacles.isGrappling()) || (state_timer >= state_duration && !tentacles.isGrappling()) || (!tentacles.isGrappling() && crawl_step_count >= 2)) {
        CreatureState nxt = decideNextState(STATE_CRAWL, hx, hy, physiology);
        enterState(nxt, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::updateObserve(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton) {
    // 观察阶段：无聊度持续累积，好奇心活跃
    const_cast<PhysiologySystem&>(physiology).addBoredom(dt * 0.085f);
    const_cast<PhysiologySystem&>(physiology).applyStimulus(0.0f, dt * 0.025f);

    // 静止观察时自然微弱恢复体力 (+0.015/s)
    const_cast<PhysiologySystem&>(physiology).recoverEnergy(dt * 0.015f);

    float energy = physiology.getEnergy();
    float boredom = physiology.getBoredom();
    float curiosity = physiology.getCuriosity();
    bool is_vertical = (std::abs(last_imu_gx) > 3.0f || std::abs(last_imu_gy) > 6.5f);

    // 【1. 真正疲惫力竭入睡驱动 (Energy < 0.15 且必须无互动保鲜期 interaction_wake_timer <= 0)】：
    if (energy < 0.15f && interaction_wake_timer <= 0.0f) {
        if (state_timer >= 8.0f || energy < 0.06f) {
            if (hy < 35.0f && (rand() % 100) < 40) {
                enterState(STATE_BAT_HANG, &tentacles, &skeleton, hx, hy); // 倒挂小憩
            } else {
                enterState(STATE_SLEEP, &tentacles, &skeleton, hx, hy);    // 蜷缩闭眼深睡
            }
            return;
        }
    }

    // 【2. 好奇与警惕交织驱动 (Curiosity > 0.60 && Stress > 0.38)】：边缘暗中观察/窥视
    if (curiosity > 0.60f && (physiology.getStress() > 0.38f || physiology.getAttachment() < 0.35f) && (rand() % 100) < 65) {
        enterState(STATE_PEEK, &tentacles, &skeleton, hx, hy);
        return;
    }

    // 【3. 观察完毕后依据上下文与行为链转换后续动作】
    if (state_timer >= state_duration || boredom >= 0.25f) {
        const_cast<PhysiologySystem&>(physiology).reduceBoredom(0.25f);
        CreatureState nxt = decideNextState(current_state, hx, hy, physiology);
        enterState(nxt, &tentacles, &skeleton, hx, hy);
        return;
    }
}

void CreatureAI::updateSwing(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, const PhysiologySystem &physiology) {
    target_look_x = hx + std::sin(state_timer * 2.0f) * 30.0f;
    target_look_y = hy + 45.0f;

    if (state_timer >= state_duration) {
        skeleton.clearHangingAnchor();
        tentacles.endCeilingSwing();
        CreatureState nxt = decideNextState(STATE_SWING, hx, hy, physiology);
        enterState(nxt, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::updateCatchDust(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, ExpressionLayer &expression, PhysiologySystem &physiology, MetaballSystem *metaballs, const PreyBugSystem *bugs, FluidSymbolSystem *fluid_symbols) {
    physiology.reduceBoredom(dt * 0.22f);
    physiology.consumeEnergy(dt * 0.045f); // 剧烈飞扑消耗体力

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

    if (state_timer < 0.50f) {
        // 【阶段 1: 猎食者式压低紧绷压缩蓄力 (0.0s ~ 0.50s)】
        target_look_x = target_x;
        target_look_y = target_y;
        skeleton.setBouncingMode(true);
        skeleton.setBounceDeform(0.45f, 1.42f); // 极度压低紧绷蓄能
        expression.triggerExpression(EXPR_CURIOSITY, 0.6f);
        
        // 尾部与后躯紧绷高频轻颤
        skeleton.triggerLocalBleb(SKELETON_NODE_COUNT - 1, 0.45f);
        skeleton.triggerLocalBleb(SKELETON_NODE_COUNT - 2, 0.35f);
    } 
    else if (state_timer < 0.65f && state_timer - dt < 0.50f) {
        // 【阶段 2: 闪电破空暴冲 (Explosive Predator Pounce, 0.15s, 155px/s)】
        float dx = target_x - hx;
        float dy = target_y - hy;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 1.0f) {
            float nx = dx / dist;
            float ny = dy / dist;
            float pounce_speed = 155.0f; // 闪电爆发速度
            skeleton.triggerPounce(nx, ny - 0.15f, pounce_speed);
            
            // 离弦之箭紧绷流线型拉长 (1.85x 长度, 0.50x 截面)
            skeleton.setBounceDeform(1.85f, 0.50f);
            skeleton.triggerLocalBleb(0, 1.8f);

            // 起跳地面受力激射反冲水墨液滴
            if (metaballs) {
                metaballs->spawnDroplet(hx - nx * 8.0f, hy + 6.0f, -nx * 1.8f, 1.2f, 2.5f, true);
            }
        }
    } 
    else if (state_timer >= 0.65f && state_timer < 0.85f) {
        // 【阶段 3: 猛烈落地/抓地极限拍扁减震 (Violent Impact Slam & Squash)】
        target_look_x = target_x;
        target_look_y = target_y;
        skeleton.setBounceDeform(0.36f, 1.82f); // 砸地极限压扁
        
        for (int i = 0; i < SKELETON_NODE_COUNT; ++i) {
            skeleton.triggerLocalBleb(i, 0.8f);
        }
    } 
    else if (state_timer >= 0.85f) {
        // 【阶段 4: 困惑抬头、歪头注视并吐出问号】
        skeleton.setBouncingMode(false);
        skeleton.setBounceDeform(1.0f, 1.0f);
        target_look_x = target_x;
        target_look_y = target_y;
        expression.triggerExpression(EXPR_CURIOSITY, 2.0f);

        if (state_timer >= 1.5f && state_timer - dt < 1.5f) {
            if (fluid_symbols) {
                float sym_x = std::max(45.0f, std::min((float)SCREEN_W - 45.0f, hx));
                float sym_y = (hy > 65.0f) ? (hy - 45.0f) : (hy + 45.0f);
                fluid_symbols->trigger("?", sym_x, sym_y, hx, hy);
            }
        }
    }

    if (state_timer >= state_duration) {
        skeleton.setBouncingMode(false);
        skeleton.setBounceDeform(1.0f, 1.0f);
        CreatureState nxt = decideNextState(STATE_CATCH_DUST, hx, hy, physiology);
        enterState(nxt, &tentacles, &skeleton, hx, hy);
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
        CreatureState nxt = decideNextState(STATE_ROLL, hx, hy, physiology);
        enterState(nxt, nullptr, &skeleton, hx, hy);
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
                    CreatureState nxt = decideNextState(STATE_BOUNCE, hx, hy, physiology);
                    enterState(nxt, nullptr, &skeleton, hx, hy);
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
        const_cast<PhysiologySystem&>(physiology).recoverEnergy(dt * 0.0012f);

        // 倒挂睡眠也喷出 Zz 符号 (从身体处喷出)
        if (!is_sleep_peeking) {
            sleep_zz_timer += dt;
            if (sleep_zz_timer >= 5.5f) {
                sleep_zz_timer = 0.0f;
                if (fluid_symbols) {
                    fluid_symbols->trigger("zz", hx + ((rand() % 14) - 7), hy + 24.0f, hx, hy);
                }
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
        CreatureState nxt = decideNextState(STATE_BAT_HANG, hx, hy, physiology);
        enterState(nxt, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::updateBallPlay(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, MetaballSystem &metaballs, ExpressionLayer &expression, PhysiologySystem &physiology, FluidSymbolSystem *fluid_symbols) {
    if (!symbiote_ball.active) {
        physiology.resetBoredom();
        CreatureState nxt = decideNextState(STATE_BALL_PLAY, hx, hy, physiology);
        enterState(nxt, &tentacles, &skeleton, hx, hy);
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
            CreatureState nxt = decideNextState(STATE_BALL_PLAY, hx, hy, physiology);
            enterState(nxt, &tentacles, &skeleton, hx, hy);
            return;
        }
    }
}

void CreatureAI::updateSleep(float dt, float hx, float hy, const PhysiologySystem &physiology, FluidSymbolSystem *fluid_symbols) {
    // 睡眠中深度平稳长效恢复体力 (+0.0012/s，需 10~15 分钟充满) 且重置无聊度
    const_cast<PhysiologySystem&>(physiology).recoverEnergy(dt * 0.0012f);
    const_cast<PhysiologySystem&>(physiology).resetBoredom();

    target_look_x = hx;
    target_look_y = hy + 10.0f;

    // 睡眠中规律喷出上升的 Zz 气泡 (每 5.5s 周期，半睁眼探视时暂停喷泡)，顺序为口鼻小z -> 顶端大Z
    if (!is_sleep_peeking) {
        sleep_zz_timer += dt;
        if (sleep_zz_timer >= 5.5f) {
            sleep_zz_timer = 0.0f;
            if (fluid_symbols) {
                fluid_symbols->trigger("zz", hx + ((rand() % 14) - 7), hy - 18.0f, hx, hy);
            }
        }
    }

    // 睡眠结束条件：睡满 10+ 分钟且体力恢复到 0.95 满血自然睡饱苏醒，或受到外界剧烈惊吓！
    if ((state_timer >= state_duration && physiology.getEnergy() >= 0.95f) || physiology.getStress() > 0.65f) {
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
        case STATE_SWING:      updateSwing(dt, hx, hy, skeleton, tentacles, physiology); break;
        case STATE_CATCH_DUST: updateCatchDust(dt, hx, hy, skeleton, tentacles, expression, physiology, &metaballs, bugs, fluid_symbols); break;
        case STATE_ROLL:       updateRoll(dt, hx, hy, skeleton, expression, physiology, &metaballs); break;
        case STATE_BOUNCE:     updateBounce(dt, hx, hy, skeleton, metaballs, physiology, expression); break;
        case STATE_BAT_HANG:   updateBatHang(dt, hx, hy, skeleton, tentacles, physiology, fluid_symbols); break;
        case STATE_BALL_PLAY:  updateBallPlay(dt, hx, hy, skeleton, tentacles, metaballs, expression, physiology, fluid_symbols); break;
        case STATE_PEEK:       updatePeek(dt, hx, hy, skeleton, tentacles, const_cast<PhysiologySystem&>(physiology), expression); break;
        case STATE_SLEEP:      updateSleep(dt, hx, hy, physiology, fluid_symbols); break;
        case STATE_STARTLED:   updateStartled(dt, hx, hy, physiology); break;
        case STATE_JOLTING:    updateJolting(dt, hx, hy, physiology); break;
        case STATE_EXPRESSING: updateExpressing(dt, hx, hy, expression); break;
    }

    // 动作测试自然过渡调度
    if (demo_transitioning) {
        updateDemoTransition(dt, skeleton, tentacles, hx, hy);
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

    // 互动唤醒保鲜期倒计时
    if (interaction_wake_timer > 0.0f) {
        interaction_wake_timer -= dt;
    }

    // 睡眠惺忪半睁眼与触发冷却倒计时
    if (sleep_peek_cooldown > 0.0f) {
        sleep_peek_cooldown -= dt;
    }
    if (is_sleep_peeking) {
        sleep_peek_timer -= dt;
        if (sleep_peek_timer <= 0.0f) {
            sleep_peek_timer = 0.0f;
            is_sleep_peeking = false;
        }
    }
}

void CreatureAI::handleSingleTap(FluidSymbolSystem &symbols, ExpressionLayer &expr, PhysiologySystem &phys) {
    interaction_wake_timer = 35.0f; // 注入 35 秒互动活跃保鲜期
    if (isSleeping()) {
        // 睡眠中轻敲：缓慢半睁眼（微眯惺忪状态），不惊醒，若无后续事件 2.8 秒后继续深睡
        is_sleep_peeking = true;
        sleep_peek_timer = 2.8f;
        target_look_x = SCREEN_W * 0.5f;
        target_look_y = SCREEN_H * 0.5f;
    } else {
        // 醒着时轻敲一下：引起注意，眼睛注视屏幕中心，轻微好奇
        phys.applyStimulus(0.12f, 0.15f);
        phys.recoverEnergy(0.04f);
        target_look_x = SCREEN_W * 0.5f + ((rand() % 40) - 20);
        target_look_y = SCREEN_H * 0.5f + ((rand() % 40) - 20);
    }
}

void CreatureAI::handleDoubleTap(FluidSymbolSystem &symbols, ExpressionLayer &expr, PhysiologySystem &phys,
                                 SkeletonSystem &skeleton, TentacleRenderer &tentacles) {
    interaction_wake_timer = 45.0f; // 双击注入 45 秒高能互动保鲜期，绝对禁止秒睡
    phys.recoverEnergy(0.08f);      // 互动振奋精神

    float hx = skeleton.getNode(0).x;
    float hy = skeleton.getNode(0).y;

    if (isSleeping()) {
        // 睡眠中双击：轻柔唤醒
        is_sleep_peeking = false;
        sleep_peek_timer = 0.0f;
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
        float sym_x = std::max(45.0f, std::min((float)SCREEN_W - 45.0f, hx));
        float sym_y = (hy > 65.0f) ? (hy - 48.0f) : (hy + 45.0f);
        symbols.trigger("?", sym_x, sym_y, hx, hy);
        expr.triggerExpression(EXPR_CURIOSITY, 3.5f);
        return;
    }

    // 闲置/清醒时双击设备：与主人亲密互动！
    // 随机 3 种惊喜：
    // 1. 主动荡秋千（延长至 22 秒，尽情玩耍）+ 吐出爱心
    // 2. 喷出水墨爱心图腾 (宽敞开阔处漂浮)
    // 3. 喷出水墨问号图腾
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
        // 2. 喷出水墨爱心图腾 (从头部喷出并进入观察互动)
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
        state_duration = 4.5f;
        symbols.trigger("heart", sym_x, sym_y, hx, hy);
        expr.triggerExpression(EXPR_TRUST, 4.0f);
        phys.applyStimulus(0.0f, 0.45f); // 提升 comfort
    } else {
        // 3. 喷出水墨问号图腾
        enterState(STATE_OBSERVE, &tentacles, &skeleton, hx, hy);
        state_duration = 4.5f;
        symbols.trigger("?", sym_x, sym_y, hx, hy);
        expr.triggerExpression(EXPR_CURIOSITY, 4.0f);
        phys.applyStimulus(0.0f, 0.35f);
    }
}

void CreatureAI::handleMultiTapIrritate(FluidSymbolSystem &symbols, ExpressionLayer &expr, PhysiologySystem &phys,
                                       SkeletonSystem &skeleton) {
    // 连续多次敲击（激惹骚扰）：毒液感到强烈烦躁与愤怒！
    interaction_wake_timer = 40.0f;
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
    symbols.trigger("!", sym_x, sym_y, hx, hy);

    // 触发身体局部鼓包应力突变与受惊震颤
    skeleton.triggerLocalBleb(1, 1.5f);
    skeleton.triggerLocalBleb(2, 1.5f);
    skeleton.triggerLocalBleb(3, 1.5f);
    skeleton.applyImpulse(((rand() % 60) - 30) * 0.1f, -1.8f);
    triggerStartle(1.5f);
}

void CreatureAI::updatePeek(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, PhysiologySystem &physiology, ExpressionLayer &expression) {
    // 边缘暗中观察/窥视状态
    // 好奇心与警惕性并存：小幅增加舒适度，缓慢消耗精力
    physiology.addBoredom(dt * 0.035f);
    physiology.consumeEnergy(dt * 0.006f);

    // 沿边缓慢巡游移动 (平滑低速 18px/s)
    if (peek_edge == 0 || peek_edge == 2) {
        peek_target_x += peek_move_dir * dt * 18.0f;
        if (peek_target_x < 36.0f) { peek_target_x = 36.0f; peek_move_dir = 1.0f; }
        else if (peek_target_x > (float)SCREEN_W - 36.0f) { peek_target_x = (float)SCREEN_W - 36.0f; peek_move_dir = -1.0f; }
    } else {
        peek_target_y += peek_move_dir * dt * 18.0f;
        if (peek_target_y < 36.0f) { peek_target_y = 36.0f; peek_move_dir = 1.0f; }
        else if (peek_target_y > (float)SCREEN_H - 36.0f) { peek_target_y = (float)SCREEN_H - 36.0f; peek_move_dir = -1.0f; }
    }

    // 驱动骨架边缘潜行锚定
    float anchor_x = peek_target_x;
    float anchor_y = peek_target_y;
    // peek_submerge_offset = 0 (缩回), -14 (探头). 探头时负值，让坐标更靠近屏幕内
    if (peek_edge == 0) anchor_y = -26.0f - peek_submerge_offset; // 顶边深潜隐藏在上面
    else if (peek_edge == 1) anchor_x = (float)SCREEN_W + 26.0f + peek_submerge_offset; // 右侧深潜隐藏在右边
    else if (peek_edge == 2) anchor_y = (float)SCREEN_H + 26.0f + peek_submerge_offset; // 底边深潜隐藏在下面
    else if (peek_edge == 3) anchor_x = -26.0f - peek_submerge_offset; // 左侧深潜隐藏在左边

    skeleton.setCreepingTarget(anchor_x, anchor_y, 1.25f);
    tentacles.setCreepMode(true, 0.85f); // 扒边的触手表现更紧绷

    // 探头与缩回反差萌动态 (Peek-a-boo Dynamics)
    peek_raise_timer += dt;
    if (!peek_is_raised) {
        // 平缓缩回状态：只露双眼和头尖 (offset = 0)
        peek_submerge_offset += (0.0f - peek_submerge_offset) * dt * 6.0f;
        if (peek_raise_timer >= peek_raise_interval) {
            // 好奇探头！
            peek_is_raised = true;
            peek_raise_timer = 0.0f;
            expression.triggerExpression(EXPR_CURIOSITY, 2.2f);
        }
        // 缩回时眼神在沿边敏锐扫视
        if (peek_edge == 0 || peek_edge == 2) {
            target_look_x = peek_target_x + peek_move_dir * 25.0f;
            target_look_y = (peek_edge == 0) ? (35.0f + std::sin(millis() * 0.003f) * 20.0f) : (SCREEN_H - 35.0f - std::sin(millis() * 0.003f) * 20.0f);
        } else {
            target_look_y = peek_target_y + peek_move_dir * 25.0f;
            target_look_x = (peek_edge == 3) ? (35.0f + std::sin(millis() * 0.003f) * 20.0f) : (SCREEN_W - 35.0f - std::sin(millis() * 0.003f) * 20.0f);
        }
    } else {
        // 探出身躯状态：向外探出 14px，露出更多头与细小触手
        peek_submerge_offset += (-14.0f - peek_submerge_offset) * dt * 7.0f;
        // 探头时好奇地注视屏幕中央
        target_look_x = SCREEN_W * 0.5f;
        target_look_y = SCREEN_H * 0.5f;
        if (peek_raise_timer >= 1.8f) {
            // 探视完毕或发现风吹草动，迅速机警缩回
            peek_is_raised = false;
            peek_raise_timer = 0.0f;
            peek_raise_interval = 2.8f + (rand() % 25) * 0.1f;
        }
    }

    // 同步给骨架形变系统
    skeleton.setPeekingMode(true, peek_submerge_offset, peek_edge);

    // 时间到或积累无聊后，依据上下文与生物行为链平稳转出
    if (state_timer >= state_duration) {
        skeleton.setPeekingMode(false);
        CreatureState nxt = decideNextState(STATE_PEEK, hx, hy, physiology);
        enterState(nxt, &tentacles, &skeleton, hx, hy);
    }
}

void CreatureAI::requestDemoAction(const String &action_name, SkeletonSystem &skeleton, TentacleRenderer &tentacles, PhysiologySystem &physiology, ExpressionLayer &expression) {
    demo_action_name = action_name;
    demo_action_name.toLowerCase();
    demo_action_name.trim();

    Serial.printf("[AI Demo] Requested Action: %s (initiating natural transition)\n", demo_action_name.c_str());

    // 1. 调谐六维生理心理参数，使其处于触发该动作的自然心智状态
    if (demo_action_name == "peek") {
        demo_target_state = STATE_PEEK;
        physiology.setCuriosity(0.88f);
        physiology.setStress(0.55f);
        physiology.setComfort(0.35f);
        interaction_wake_timer = 35.0f;
        target_look_x = SCREEN_W * 0.5f;
        target_look_y = SCREEN_H - 10.0f;
        expression.triggerExpression(EXPR_CURIOSITY, 3.0f);
    } else if (demo_action_name == "bounce") {
        demo_target_state = STATE_BOUNCE;
        physiology.setEnergy(0.95f);
        physiology.setBoredom(0.75f);
        physiology.setStress(0.05f);
        interaction_wake_timer = 35.0f;
        target_look_x = SCREEN_W * 0.5f;
        target_look_y = 15.0f;
        expression.triggerExpression(EXPR_TRUST, 3.0f);
    } else if (demo_action_name == "ball_play") {
        demo_target_state = STATE_BALL_PLAY;
        physiology.setEnergy(0.90f);
        physiology.setBoredom(0.68f);
        physiology.setComfort(0.80f);
        interaction_wake_timer = 35.0f;
        target_look_x = SCREEN_W * 0.5f;
        target_look_y = 40.0f;
        expression.triggerExpression(EXPR_CURIOSITY, 3.0f);
    } else if (demo_action_name == "swing") {
        demo_target_state = STATE_SWING;
        physiology.setEnergy(0.92f);
        physiology.setCuriosity(0.85f);
        physiology.setBoredom(0.50f);
        interaction_wake_timer = 35.0f;
        target_look_x = SCREEN_W * 0.5f;
        target_look_y = 5.0f; // 抬头望向天花板
        expression.triggerExpression(EXPR_TRUST, 3.0f);
    } else if (demo_action_name == "roll") {
        demo_target_state = STATE_ROLL;
        physiology.setEnergy(0.85f);
        physiology.setBoredom(0.65f);
        interaction_wake_timer = 35.0f;
        expression.triggerExpression(EXPR_TRUST, 2.5f);
    } else if (demo_action_name == "catch_dust") {
        demo_target_state = STATE_CATCH_DUST;
        physiology.setCuriosity(0.92f);
        physiology.setStress(0.20f);
        interaction_wake_timer = 35.0f;
        expression.triggerExpression(EXPR_CURIOSITY, 3.0f);
    } else if (demo_action_name == "crawl") {
        demo_target_state = STATE_CRAWL;
        physiology.setEnergy(0.90f);
        physiology.setCuriosity(0.80f);
        interaction_wake_timer = 35.0f;
        target_look_x = 40.0f + (rand() % (SCREEN_W - 80));
        target_look_y = 20.0f;
    } else if (demo_action_name == "creep") {
        demo_target_state = STATE_CREEP;
        physiology.setEnergy(0.70f);
        physiology.setCuriosity(0.65f);
        interaction_wake_timer = 35.0f;
    } else if (demo_action_name == "sleep") {
        demo_target_state = STATE_SLEEP;
        physiology.setEnergy(0.08f); // 极度困倦
        physiology.setStress(0.02f);
        physiology.setComfort(0.90f);
        interaction_wake_timer = 0.0f; // 释放唤醒锁
        expression.triggerExpression(EXPR_SILENT_OBSERVATION, 3.5f); // 眼神静默安眠
    } else if (demo_action_name == "bat_hang") {
        demo_target_state = STATE_BAT_HANG;
        physiology.setEnergy(0.22f);
        physiology.setComfort(0.85f);
        interaction_wake_timer = 0.0f;
    } else if (demo_action_name == "irritate") {
        demo_target_state = STATE_OBSERVE;
        physiology.setStress(0.95f);
        physiology.triggerShock(0.8f);
        expression.triggerExpression(EXPR_WARNING, 4.0f);
        demo_transitioning = false;
        return;
    } else {
        demo_target_state = STATE_OBSERVE;
    }

    physiology.forceEmotionUpdate();

    // 2. 启动仿生自然过渡期 (0.8s 心理预备与肌肉蓄势，不突兀瞬移)
    demo_transitioning = true;
    demo_transition_timer = 0.8f;

    // 动作收敛：若当前处于剧烈秋千或高速翻滚中，先平滑制动
    if (current_state == STATE_SWING) {
        skeleton.clearHangingAnchor();
    }
    if (current_state == STATE_ROLL) {
        skeleton.setRollingMode(false);
    }
}

void CreatureAI::updateDemoTransition(float dt, SkeletonSystem &skeleton, TentacleRenderer &tentacles, float hx, float hy) {
    demo_transition_timer -= dt;
    if (demo_transition_timer <= 0.0f) {
        demo_transitioning = false;
        demo_transition_timer = 0.0f;
        // 阻尼缓冲完毕，自然正式进入演示动作！
        enterState(demo_target_state, &tentacles, &skeleton, hx, hy);
        Serial.printf("[AI Demo] Smoothly entered target state: %s\n", getStateName());
    }
}
