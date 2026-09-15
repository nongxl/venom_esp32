#include "NeuralDrive.h"

const char* getNeuralEventName(NeuralEventType evt) {
    switch (evt) {
        case EVT_TOUCH:          return "TOUCH";
        case EVT_SHAKE:          return "SHAKE";
        case EVT_LOUD_SOUND:     return "SOUND";
        case EVT_USER_APPROACH:  return "APPROACH";
        case EVT_ESCAPE:         return "ESCAPE";
        case EVT_PROBE_SUCCESS:  return "PROBE_HIT";
        case EVT_PROBE_FAIL:     return "PROBE_MISS";
        case EVT_REST_COMPLETE:  return "REST_DONE";
        default:                 return "NONE";
    }
}

const char* getNeuralBehaviorName(NeuralBehaviorType beh) {
    switch (beh) {
        case BEH_ESCAPE:   return "ESCAPE";
        case BEH_FREEZE:   return "FREEZE";
        case BEH_OBSERVE:  return "OBSERVE";
        case BEH_EXPLORE:  return "EXPLORE";
        case BEH_APPROACH: return "APPROACH";
        case BEH_PROBE:    return "PROBE";
        case BEH_REST:     return "REST";
        case BEH_ATTACH:   return "ATTACH";
        default:           return "UNKNOWN";
    }
}

NeuralDrive::NeuralDrive() {
    init();
}

void NeuralDrive::init() {
    drives.reset();
    current_behavior = BEH_OBSERVE;
    current_behavior_duration = 0.0f;
    current_behavior_min_duration = 1.8f;
    tap_repetition_timer = 0.0f;
    tap_streak_count = 0;
}

void NeuralDrive::recordEvent(NeuralEventType type, float intensity) {
    event_history[event_history_head].type = type;
    event_history[event_history_head].timestamp = global_time_sec;
    event_history[event_history_head].intensity = intensity;
    event_history_head = (event_history_head + 1) % EVENT_HISTORY_CAP;
    event_count_total++;
}

int NeuralDrive::countRecentEvents(NeuralEventType type, float window_sec) const {
    int count = 0;
    for (int i = 0; i < EVENT_HISTORY_CAP; ++i) {
        if (event_history[i].type == type) {
            if (global_time_sec - event_history[i].timestamp <= window_sec) {
                count++;
            }
        }
    }
    return count;
}

const RecentEvent& NeuralDrive::getLatestEvent() const {
    int last_idx = (event_history_head - 1 + EVENT_HISTORY_CAP) % EVENT_HISTORY_CAP;
    return event_history[last_idx];
}

const char* NeuralDrive::getLatestEventName() const {
    return getNeuralEventName(getLatestEvent().type);
}

void NeuralDrive::triggerProbeOutcome(bool success, float reward) {
    if (success) {
        recordEvent(EVT_PROBE_SUCCESS, reward);
        drives.curiosity = std::max(0.15f, drives.curiosity - 0.35f * reward);
        drives.comfort   = std::min(1.0f, drives.comfort + 0.18f * reward);
        drives.energy    = std::min(1.0f, drives.energy + 0.08f * reward);
        drives.arousal   = std::min(1.0f, drives.arousal + 0.12f);
    } else {
        recordEvent(EVT_PROBE_FAIL, reward);
        drives.curiosity = std::min(1.0f, drives.curiosity + 0.08f);
        drives.comfort   = std::max(0.2f, drives.comfort - 0.05f);
    }
}

void NeuralDrive::triggerSuddenMotion(float intensity) {
    recordEvent(EVT_SHAKE, intensity);
    // 剧烈震动/晃动刺激
    float effective_intensity = intensity * (1.0f - drives.habituation * 0.60f);
    drives.threat  = std::min(1.0f, drives.threat + 0.35f * effective_intensity);
    drives.arousal = std::min(1.0f, drives.arousal + 0.40f * intensity);
    drives.comfort = std::max(0.0f, drives.comfort - 0.20f * intensity);

    if (intensity > 1.3f) {
        // 超强震动（猛甩猛摔）：破坏习惯化，重新敏化
        drives.habituation *= 0.4f;
    }
}

void NeuralDrive::triggerLoudSound(float intensity) {
    recordEvent(EVT_LOUD_SOUND, intensity);
    // 突发高分贝爆鸣
    float effective_intensity = intensity * (1.0f - drives.habituation * 0.50f);
    drives.threat  = std::min(1.0f, drives.threat + 0.28f * effective_intensity);
    drives.arousal = std::min(1.0f, drives.arousal + 0.35f * intensity);
    drives.curiosity = std::min(1.0f, drives.curiosity + 0.12f);
}

void NeuralDrive::triggerTouchContact(float intensity) {
    recordEvent(EVT_TOUCH, intensity);
    // 敲击或触摸
    if (tap_repetition_timer > 0.0f) {
        // 短期内重复敲击，累积习惯化
        tap_streak_count++;
        drives.habituation = std::min(0.85f, drives.habituation + 0.14f);
    } else {
        tap_streak_count = 1;
    }
    tap_repetition_timer = 4.0f; // 4 秒内视为连续互动窗口

    // 习惯化减弱受惊反应
    float effective_threat = 0.42f * (1.0f - drives.habituation * 0.75f) * intensity;
    drives.threat  = std::min(1.0f, drives.threat + effective_threat);
    drives.arousal = std::min(1.0f, drives.arousal + 0.25f * intensity);
    drives.curiosity = std::min(1.0f, drives.curiosity + 0.15f * intensity);

    // 随着多次轻度接触与习惯化沉淀，建立更深刻的依恋与安全感
    if (drives.habituation > 0.20f) {
        drives.attachment = std::min(1.0f, drives.attachment + 0.055f * intensity);
        drives.comfort    = std::min(1.0f, drives.comfort + 0.050f * intensity);
    } else {
        // 初次轻微试探也建立微弱依恋萌芽
        drives.attachment = std::min(1.0f, drives.attachment + 0.020f * intensity);
    }
}

void NeuralDrive::triggerPositiveInteraction(float amount) {
    recordEvent(EVT_USER_APPROACH, amount);
    drives.attachment = std::min(1.0f, drives.attachment + amount);
    drives.comfort    = std::min(1.0f, drives.comfort + amount * 0.8f);
    drives.threat     = std::max(0.0f, drives.threat - amount * 0.6f);
}

float NeuralDrive::getMinDurationForBehavior(NeuralBehaviorType beh) const {
    switch (beh) {
        case BEH_ESCAPE:   return 1.2f; // 逃生动作果断，约1.2s后评估新威胁
        case BEH_FREEZE:   return 0.8f; // 警惕定身稍作停顿
        case BEH_OBSERVE:  return 1.8f; // 好奇注视至少停留观察1.8s
        case BEH_EXPLORE:  return 3.0f; // 探索与攀爬具有较强行为惯性 (3.0s)
        case BEH_APPROACH: return 2.2f;
        case BEH_PROBE:    return 1.5f;
        case BEH_REST:     return 3.5f; // 静养与休息不可瞬切
        case BEH_ATTACH:   return 4.0f; // 玩耍动作持续进行
        default:           return 2.0f;
    }
}

void NeuralDrive::updateDriveDynamics(float dt, const PerceptionSignals &perc) {
    // 1. 瞬态衰减：快变驱动
    // Threat 半衰期约 2.2 秒 (平稳衰减，不突兀归零)
    drives.threat = std::max(0.02f, drives.threat - dt * 0.28f);

    // Arousal 平稳衰减至基线 0.18
    if (drives.arousal > 0.18f) {
        drives.arousal = std::max(0.18f, drives.arousal - dt * 0.16f);
    } else {
        drives.arousal = std::min(0.18f, drives.arousal + dt * 0.05f);
    }

    // 2. 中速演化：Curiosity / Energy / Fatigue / Comfort
    // Curiosity: 环境安静且威胁低时自然蓄积，遇到新刺激增强
    if (drives.threat < 0.25f && perc.movement_activity < 0.3f) {
        drives.curiosity = std::min(0.95f, drives.curiosity + dt * 0.045f);
    }
    if (perc.prey_detected > 0.1f) {
        drives.curiosity = std::min(1.0f, drives.curiosity + dt * 0.18f * perc.prey_detected);
    }

    // Energy 与 Fatigue 动力学
    if (perc.movement_activity > 0.2f) {
        // 机体处于活跃运动中 (CRAWL, BOUNCE, ROLL, SWING)
        float cost = dt * (0.012f + perc.movement_activity * 0.028f);
        drives.energy = std::max(0.05f, drives.energy - cost);
        drives.fatigue = std::min(1.0f, drives.fatigue + dt * (0.015f + perc.movement_activity * 0.035f));
    } else {
        // 静止发呆或休眠
        drives.energy = std::min(1.0f, drives.energy + dt * 0.018f);
        drives.fatigue = std::max(0.0f, drives.fatigue - dt * 0.032f);
    }

    // Comfort 动力学：稳定表面与低威胁增加舒适感，晃动与脱附降低舒适感
    if (perc.stable_surface > 0.5f && drives.threat < 0.20f) {
        drives.comfort = std::min(1.0f, drives.comfort + dt * 0.035f);
    } else if (perc.stable_surface < 0.3f || drives.threat > 0.50f) {
        drives.comfort = std::max(0.0f, drives.comfort - dt * 0.12f);
    }

    // 3. 慢速演化：Attachment 与 Habituation
    // 习惯化计时与缓慢遗忘
    if (tap_repetition_timer > 0.0f) {
        tap_repetition_timer -= dt;
    } else {
        // 无刺激时，习惯化缓慢消退 (半衰期约 25s)
        drives.habituation = std::max(0.0f, drives.habituation - dt * 0.022f);
    }

    // 依恋极慢衰减 (长时间无人理睬，半衰期数分钟)
    drives.attachment = std::max(0.05f, drives.attachment - dt * 0.0015f);

    // 持续温和人声 / 轻度互动增进依恋
    if (perc.user_nearby > 0.2f && drives.threat < 0.30f) {
        drives.attachment = std::min(1.0f, drives.attachment + dt * 0.015f * perc.user_nearby);
    }
}

void NeuralDrive::evaluateBehaviorCompetition(float dt) {
    // ─────────────────────────────────────────────────────────────
    // 步骤 1：基于 8 维 Drive 计算各行为激活分 (Raw Activation Score)
    // ─────────────────────────────────────────────────────────────
    float t = drives.threat;
    float c = drives.curiosity;
    float a = drives.attachment;
    float r = drives.arousal;
    float e = drives.energy;
    float m = drives.comfort;
    float f = drives.fatigue;
    float h = drives.habituation;

    // 行为激活基础分计算：
    // 【BEH_ATTACH 玩耍与共生】：在高积极参数下拥有强劲激活权（不仅包含依恋信任，更大幅提升自娱自乐权重）！
    float playful_vigor = (c * 0.55f + e * 0.45f); // 活力自娱自乐因子
    scores.raw_score[BEH_ESCAPE]   = t * 1.45f + r * 0.30f - a * 0.20f - h * 0.35f;
    scores.raw_score[BEH_FREEZE]   = t * 0.95f + r * 0.15f - e * 0.20f;
    scores.raw_score[BEH_OBSERVE]  = c * 0.75f + r * 0.20f - t * 0.50f + h * 0.15f;
    scores.raw_score[BEH_EXPLORE]  = c * 0.85f + e * 0.35f - t * 0.85f - f * 0.55f;
    scores.raw_score[BEH_APPROACH] = c * 0.60f + a * 0.95f - t * 0.80f;
    scores.raw_score[BEH_PROBE]    = c * 0.85f + r * 0.25f - t * 0.60f;
    scores.raw_score[BEH_REST]     = f * 1.25f + (1.0f - e) * 0.85f - r * 0.45f;
    // 当 a, c, e, m 都很高且 t 很低时，ATTACH 得分将轻松超越 1.15，稳居主导胜出者！
    scores.raw_score[BEH_ATTACH]   = a * 0.90f + playful_vigor * 0.95f + m * 0.45f - t * 0.95f - f * 0.45f;

    for (int i = 0; i < BEH_COUNT; ++i) {
        if (scores.raw_score[i] < 0.0f) scores.raw_score[i] = 0.0f;
    }

    // ─────────────────────────────────────────────────────────────
    // 步骤 2：横向抑制矩阵 (Lateral Inhibition)
    // 强行为压制冲突行为，凸显主导行为
    // ─────────────────────────────────────────────────────────────
    float inhibited[BEH_COUNT];
    for (int i = 0; i < BEH_COUNT; ++i) inhibited[i] = scores.raw_score[i];

    // ESCAPE 高激活时强力压制 EXPLORE 与 APPROACH
    if (scores.raw_score[BEH_ESCAPE] > 0.40f) {
        float esc_sup = scores.raw_score[BEH_ESCAPE] * 0.50f;
        inhibited[BEH_EXPLORE]  = std::max(0.0f, inhibited[BEH_EXPLORE] - esc_sup);
        inhibited[BEH_APPROACH] = std::max(0.0f, inhibited[BEH_APPROACH] - esc_sup);
        inhibited[BEH_ATTACH]   = std::max(0.0f, inhibited[BEH_ATTACH] - esc_sup * 0.8f);
    }

    // REST 高激活时压制剧烈运动行为
    if (scores.raw_score[BEH_REST] > 0.40f) {
        float rest_sup = scores.raw_score[BEH_REST] * 0.45f;
        inhibited[BEH_EXPLORE] = std::max(0.0f, inhibited[BEH_EXPLORE] - rest_sup);
        inhibited[BEH_ATTACH]  = std::max(0.0f, inhibited[BEH_ATTACH] - rest_sup * 0.7f);
        inhibited[BEH_PROBE]   = std::max(0.0f, inhibited[BEH_PROBE] - rest_sup);
    }

    // APPROACH 激活时轻度安抚减弱 ESCAPE 冲动
    if (scores.raw_score[BEH_APPROACH] > 0.40f) {
        inhibited[BEH_ESCAPE] = std::max(0.0f, inhibited[BEH_ESCAPE] - scores.raw_score[BEH_APPROACH] * 0.15f);
    }

    // FREEZE 抑制主动位移
    if (scores.raw_score[BEH_FREEZE] > 0.40f) {
        inhibited[BEH_EXPLORE] = std::max(0.0f, inhibited[BEH_EXPLORE] - scores.raw_score[BEH_FREEZE] * 0.35f);
    }

    // ─────────────────────────────────────────────────────────────
    // 步骤 3：微量平局噪声扰动 (Stochastic Perturbation)
    // 仅注入 ±0.025 伪随机，绝不颠覆因果关系
    // ─────────────────────────────────────────────────────────────
    for (int i = 0; i < BEH_COUNT; ++i) {
        float noise = ((rand() % 50) - 25) * 0.001f; // -0.025 ~ +0.025
        scores.final_score[i] = std::max(0.0f, inhibited[i] + noise);
    }

    // ─────────────────────────────────────────────────────────────
    // 步骤 4：寻找最高分 (Winner) 与次高分 (Runner-up)
    // ─────────────────────────────────────────────────────────────
    NeuralBehaviorType best_beh = BEH_REST;
    float best_val = -1.0f;
    NeuralBehaviorType second_beh = BEH_OBSERVE;
    float second_val = -1.0f;

    for (int i = 0; i < BEH_COUNT; ++i) {
        NeuralBehaviorType b = (NeuralBehaviorType)i;
        float s = scores.final_score[i];
        if (s > best_val) {
            second_val = best_val;
            second_beh = best_beh;
            best_val = s;
            best_beh = b;
        } else if (s > second_val) {
            second_val = s;
            second_beh = b;
        }
    }

    scores.winner = best_beh;
    scores.winner_score = best_val;
    scores.runner_up = second_beh;
    scores.runner_up_score = second_val;

    // ─────────────────────────────────────────────────────────────
    // 步骤 5：行为惯性与迟滞仲裁 (Commitment & Hysteresis)
    // 杜绝帧间行为抖动
    // ─────────────────────────────────────────────────────────────
    current_behavior_duration += dt;

    bool allow_switch = false;
    float cur_score = scores.final_score[current_behavior];

    if (scores.winner == BEH_ESCAPE && drives.threat > 0.60f) {
        // 突发紧急威胁，豁免惯性立即避险！
        allow_switch = true;
    } else if (current_behavior_duration >= current_behavior_min_duration) {
        // 达到最小持续时间，且新行为有明确优势 (+0.08)
        if (scores.winner_score > cur_score + 0.08f) {
            allow_switch = true;
        }
    } else {
        // 未达最小持续时间，只有新行为显著碾压当前行为 (+0.25) 才允许强制打断
        if (scores.winner_score > cur_score + 0.25f) {
            allow_switch = true;
        }
    }

    if (allow_switch && scores.winner != current_behavior) {
        current_behavior = scores.winner;
        current_behavior_duration = 0.0f;
        current_behavior_min_duration = getMinDurationForBehavior(current_behavior);
    }
}

void NeuralDrive::applyMotorImpulse(float ix, float iy, float strength) {
    motor_intent.impulse_x = ix;
    motor_intent.impulse_y = iy;
    motor_intent.impulse_strength = std::min(3.0f, motor_intent.impulse_strength + strength);
}

void NeuralDrive::computeMotorIntent(float dt) {
    float target_elongation = 1.0f;
    float target_squash = 1.0f;
    float target_instability = 0.0f;
    float target_spring_stiff = 1.0f;
    float target_wall_adhes = 1.0f;
    float target_head_lead = 1.0f;
    float target_speed = 0.0f;
    float target_eye_att = 0.8f;

    switch (current_behavior) {
        case BEH_ESCAPE:
            target_elongation = 1.15f + drives.arousal * 0.08f; // 温和流线型，拒绝竹笋尖拱
            target_squash = 0.88f;
            target_instability = std::min(1.0f, drives.threat * 0.85f + 0.15f);
            target_spring_stiff = 1.45f;
            target_wall_adhes = 0.40f;
            target_head_lead = 1.4f;
            target_speed = 1.8f;
            target_eye_att = 1.0f;
            break;

        case BEH_FREEZE:
            target_elongation = 0.94f; // 从容略微收缩，绝不塌缩成薄饼
            target_squash = 0.96f;
            target_instability = 0.0f;
            target_spring_stiff = 1.35f;
            target_wall_adhes = 1.30f;
            target_head_lead = 0.85f;
            target_speed = 0.0f;
            target_eye_att = 0.95f;
            break;

        case BEH_OBSERVE:
            target_elongation = 1.08f;
            target_squash = 0.96f;
            target_instability = 0.03f;
            target_spring_stiff = 1.0f;
            target_wall_adhes = 1.05f;
            target_head_lead = 1.15f;
            target_speed = 0.0f;
            target_eye_att = 0.95f;
            break;

        case BEH_EXPLORE:
            target_elongation = 1.05f;
            target_squash = 0.97f;
            target_instability = 0.05f;
            target_spring_stiff = 1.05f;
            target_wall_adhes = 1.15f;
            target_head_lead = 1.2f;
            target_speed = 1.0f + drives.energy * 0.35f;
            target_eye_att = 0.70f;
            break;

        case BEH_APPROACH:
            target_elongation = 1.12f;
            target_squash = 0.92f;
            target_instability = 0.04f;
            target_spring_stiff = 1.15f;
            target_wall_adhes = 1.10f;
            target_head_lead = 1.3f;
            target_speed = 1.25f;
            target_eye_att = 1.0f;
            break;

        case BEH_PROBE:
            target_elongation = 1.18f; // 探身轻抚，饱满圆顶，不过度拔尖
            target_squash = 0.88f;
            target_instability = 0.12f;
            target_spring_stiff = 1.20f;
            target_wall_adhes = 0.90f;
            target_head_lead = 1.35f;
            target_speed = 1.4f;
            target_eye_att = 1.0f;
            break;

        case BEH_REST:
            target_elongation = 0.92f;
            target_squash = 1.08f; // 舒展微扁，自然安睡
            target_instability = 0.0f;
            target_spring_stiff = 0.70f;
            target_wall_adhes = 1.15f;
            target_head_lead = 0.8f;
            target_speed = 0.0f;
            target_eye_att = (drives.energy < 0.15f) ? 0.0f : 0.25f;
            break;

        default:
            target_elongation = 1.0f;
            target_squash = 1.0f;
            target_speed = 0.5f;
            break;
    }

    // 引入流体肌肉粘滞平滑 (时间常数约 0.20 秒，如丝顺滑消除断崖跳变)
    float blend = std::min(1.0f, dt * 5.0f);
    motor_intent.elongation           += (target_elongation - motor_intent.elongation) * blend;
    motor_intent.squash               += (target_squash - motor_intent.squash) * blend;
    motor_intent.instability          += (target_instability - motor_intent.instability) * blend;
    motor_intent.spring_stiffness_mod += (target_spring_stiff - motor_intent.spring_stiffness_mod) * blend;
    motor_intent.wall_adhesion_mod    += (target_wall_adhes - motor_intent.wall_adhesion_mod) * blend;
    motor_intent.head_lead_bias       += (target_head_lead - motor_intent.head_lead_bias) * blend;
    motor_intent.movement_speed       += (target_speed - motor_intent.movement_speed) * blend;
    motor_intent.eye_attention        += (target_eye_att - motor_intent.eye_attention) * blend;

    // 冲量随时间平滑衰减
    if (motor_intent.impulse_strength > 0.0f) {
        motor_intent.impulse_strength = std::max(0.0f, motor_intent.impulse_strength - dt * 3.5f);
        motor_intent.impulse_x *= 0.82f;
        motor_intent.impulse_y *= 0.82f;
    }
}

void NeuralDrive::applyBehaviorFeedback(float dt) {
    switch (current_behavior) {
        case BEH_ESCAPE:
            drives.energy  = std::max(0.05f, drives.energy - dt * 0.08f);
            drives.fatigue = std::min(1.0f, drives.fatigue + dt * 0.12f);
            drives.arousal = std::min(1.0f, drives.arousal + dt * 0.15f);
            drives.comfort = std::max(0.1f, drives.comfort - dt * 0.06f);
            break;

        case BEH_FREEZE:
            drives.arousal = std::max(0.35f, drives.arousal - dt * 0.04f);
            break;

        case BEH_OBSERVE:
            if (drives.threat < 0.35f) {
                drives.threat     = std::max(0.02f, drives.threat - dt * 0.06f);
                drives.comfort    = std::min(1.0f, drives.comfort + dt * 0.025f);
                drives.attachment = std::min(1.0f, drives.attachment + dt * 0.008f);
            }
            break;

        case BEH_EXPLORE:
        case BEH_APPROACH:
            drives.energy    = std::max(0.05f, drives.energy - dt * 0.022f);
            drives.fatigue   = std::min(1.0f, drives.fatigue + dt * 0.018f);
            drives.curiosity = std::max(0.15f, drives.curiosity - dt * 0.015f);
            if (current_behavior == BEH_APPROACH) {
                drives.attachment = std::min(1.0f, drives.attachment + dt * 0.012f);
            }
            break;

        case BEH_PROBE:
            drives.energy  = std::max(0.05f, drives.energy - dt * 0.035f);
            drives.arousal = std::min(1.0f, drives.arousal + dt * 0.05f);
            break;

        case BEH_REST:
            drives.energy  = std::min(1.0f, drives.energy + dt * 0.055f);
            drives.fatigue = std::max(0.0f, drives.fatigue - dt * 0.085f);
            drives.arousal = std::max(0.08f, drives.arousal - dt * 0.07f);
            drives.threat  = std::max(0.02f, drives.threat - dt * 0.08f);
            drives.comfort = std::min(1.0f, drives.comfort + dt * 0.045f);

            if (drives.energy > 0.90f && drives.fatigue < 0.08f && countRecentEvents(EVT_REST_COMPLETE, 10.0f) == 0) {
                recordEvent(EVT_REST_COMPLETE, 1.0f);
            }
            break;

        case BEH_ATTACH:
            drives.energy     = std::max(0.10f, drives.energy - dt * 0.028f);
            drives.attachment = std::min(1.0f, drives.attachment + dt * 0.020f);
            drives.comfort    = std::min(1.0f, drives.comfort + dt * 0.030f);
            break;

        default:
            break;
    }
}

void NeuralDrive::update(float dt, const PerceptionSignals &perc) {
    global_time_sec += dt;
    perception = perc;
    updateDriveDynamics(dt, perc);
    applyBehaviorFeedback(dt);
    evaluateBehaviorCompetition(dt);
    computeMotorIntent(dt);
}

void NeuralDrive::formatDriveBar(char *buf, size_t buf_size, const char *label, float val, int bar_len) const {
    if (!buf || buf_size == 0) return;
    float clamped = std::max(0.0f, std::min(1.0f, val));
    int filled = (int)(clamped * bar_len + 0.5f);
    if (filled > bar_len) filled = bar_len;

    char bar_str[32] = "";
    int pos = 0;
    for (int i = 0; i < bar_len && pos < 30; ++i) {
        bar_str[pos++] = (i < filled) ? '=' : '-';
    }
    bar_str[pos] = '\0';

    snprintf(buf, buf_size, "%-6s [%s] %.2f", label, bar_str, clamped);
}

void NeuralDrive::printDebugSummary() const {
    Serial.println("\n========== [NEURAL DRIVE CORE MONITOR] ==========");
    char line[64];
    formatDriveBar(line, sizeof(line), "THREAT",  drives.threat);      Serial.println(line);
    formatDriveBar(line, sizeof(line), "CURIOS",  drives.curiosity);   Serial.println(line);
    formatDriveBar(line, sizeof(line), "ATTACH",  drives.attachment);  Serial.println(line);
    formatDriveBar(line, sizeof(line), "AROUS",   drives.arousal);     Serial.println(line);
    formatDriveBar(line, sizeof(line), "ENERGY",  drives.energy);      Serial.println(line);
    formatDriveBar(line, sizeof(line), "FATIG",   drives.fatigue);     Serial.println(line);
    formatDriveBar(line, sizeof(line), "COMFORT", drives.comfort);     Serial.println(line);
    formatDriveBar(line, sizeof(line), "HABIT",   drives.habituation); Serial.println(line);

    Serial.println("-------------------------------------------------");
    Serial.printf("CURRENT : %-8s (age: %.1fs, min: %.1fs)\n",
                  getNeuralBehaviorName(current_behavior),
                  current_behavior_duration,
                  current_behavior_min_duration);
    Serial.printf("WINNER  : %-8s (score: %.2f) | 2ND: %-8s (score: %.2f)\n",
                  getNeuralBehaviorName(scores.winner), scores.winner_score,
                  getNeuralBehaviorName(scores.runner_up), scores.runner_up_score);
    Serial.printf("MOTOR   : ELONG=%.2f SQUASH=%.2f STIFF=%.2f INSTAB=%.2f ADHES=%.2f EYE=%.2f\n",
                  motor_intent.elongation, motor_intent.squash,
                  motor_intent.spring_stiffness_mod, motor_intent.instability,
                  motor_intent.wall_adhesion_mod, motor_intent.eye_attention);
    Serial.printf("EVENT   : LAST=%s (total: %d) | HABIT_DAMP=%.2fx\n",
                  getLatestEventName(), event_count_total, getHabituationDampingFactor());

    Serial.println("SCORES  :");
    for (int i = 0; i < BEH_COUNT; ++i) {
        NeuralBehaviorType b = (NeuralBehaviorType)i;
        Serial.printf("  %-8s raw: %4.2f  final: %4.2f %s\n",
                      getNeuralBehaviorName(b),
                      scores.raw_score[i],
                      scores.final_score[i],
                      (b == current_behavior) ? "<-- [ACTIVE]" : (b == scores.winner) ? "<-- [WINNER]" : "");
    }
    Serial.println("=================================================");
}
