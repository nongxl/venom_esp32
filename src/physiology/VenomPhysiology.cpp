#include "../Venom.h"
#include <ArduinoJson.h>
#include <math.h>
#include "../Noise.h"
#include "../Skills.h"

void Venom::updateStateFromAudio(const AudioFeatures& features) {
    // Volume 映射到呼吸感和烦躁度
    vState.agitation_level = fmaxf(vState.agitation_level, features.volume * 0.8f);
    
    // Harshness 映射到尖刺强度
    if (features.harshness > AUDIO_HARSH_THRESHOLD) {
        float burst = (features.harshness - AUDIO_HARSH_THRESHOLD) * 2.5f;
        vState.spike_intensity = fmaxf(vState.spike_intensity, burst);
    }
}

void Venom::triggerExpression(const String& type) {
    symbolMgr.trigger(type);
}

String Venom::getPhysiologyJson() {
    char buf[600];
    Node& head = skeleton.getNode(0);
    snprintf(buf, sizeof(buf), 
        "{\"stress\":%.2f,\"energy\":%.2f,\"comfort\":%.2f,\"curiosity\":%.2f,\"attachment\":%.2f,\"fatigue\":%.2f,\"vigilance\":%.2f,\"observed\":%s,\"behavior\":\"%s\",\"pos\":[%.1f,%.1f,%.1f],\"obs_trust\":%.2f,\"obs_fear\":%.2f,\"obs_curi\":%.2f,\"obs_resent\":%.2f,\"obs_attach\":%.2f}",
        stress, energy, comfort, curiosity, attachment, fatigue, vigilance, 
        isBeingObserved() ? "true" : "false", currentSkillName.c_str(),
        head.x, head.y, head.z,
        observer_trust, observer_fear, observer_curiosity, observer_resentment, observer_attachment);
    return String(buf);
}

void Venom::updatePhysiology(float soundLevel, float gx, float gy, float gz, float lux) {
    // 强光对生理状态的影响：过强光照会增加压力并降低舒适度
    if (lux > 2000.0f) {
        stress += 0.0001f * (lux / 2000.0f);
        comfort -= 0.00005f;
    }

    // 增加启动保护期 (2秒)，忽略硬件初始化时的传感器尖峰
    if (millis() < 2000) return;
    
    uint32_t now = millis();

    // 1. 情绪与生理自然回归 (核心：惯性与缓慢变化)
    energy = energy * 0.99995f + (INIT_ENERGY * 0.00005f);
    stress = stress * 0.999f; 
    fatigue = fatigue * 0.995f; // 大幅加快清醒状态下疲劳的自然消退速度，使其不容易频繁打瞌睡
    vigilance = vigilance * 0.9995f;
    irritation = irritation * 0.9997f;
    curiosity = curiosity * 0.9999f + (INIT_CURIOSITY * 0.0001f);
    comfort = comfort * 0.9999f + (INIT_COMFORT * 0.0001f);
    overstimulation *= 0.999f;
    expressionDesire = expressionDesire * 0.9995f + (stress * 0.0002f); 
    rhythmScore *= 0.999f;

    // --- [新增意识层 v2] 长期观察者关系参数极慢自我中性态回归 (防数值卡死) ---
    observer_trust = observer_trust * 0.999995f + 0.5f * 0.000005f;
    observer_fear = observer_fear * 0.99999f + 0.2f * 0.00001f;
    observer_curiosity = observer_curiosity * 0.999995f + 0.5f * 0.000005f;
    observer_resentment = observer_resentment * 0.99999f + 0.0f * 0.00001f;
    observer_attachment = observer_attachment * 0.999995f + 0.3f * 0.000005f;

    // 2. 环境声音响应 (逻辑重构：累积与敏感度)
    if (soundLevel > 92.0f && currentSkillName == "sleep") {
        M5.Log.printf(">>> [Behavior] Venom was startled awake by loud noise! (%.1fdB)\n", soundLevel);
        
        // --- [新增意识层 v2] 睡眠打扰导致关系恶化 ---
        observer_resentment = fminf(1.0f, observer_resentment + 0.05f);
        observer_trust = fmaxf(0.0f, observer_trust - 0.03f);
        observer_fear = fminf(1.0f, observer_fear + 0.02f);
        
        setStartled();
        return; 
    }

    if (soundLevel > SOUND_THRESHOLD_HIGH) {
        stress += 0.05f;
        vigilance += 0.08f;
        energy += 0.01f;
        irritation += 0.02f;
        memory.lastQuietTime = now;
    } else if (soundLevel > SOUND_THRESHOLD_LOW) {
        vigilance += 0.01f;
        curiosity += 0.002f;
        memory.lastQuietTime = now;
    } else {
        // 安静环境有利于恢复
        stress *= 0.995f;
        comfort += 0.0001f;
    }

    // --- [新增意识层 v2] 安静且黑暗环境下的温柔陪伴关系进化 ---
    if (lux < 15.0f && soundLevel < SOUND_THRESHOLD_LOW) {
        observer_trust = fminf(1.0f, observer_trust + 0.00005f);
        observer_fear = fmaxf(0.0f, observer_fear * 0.9995f);
        observer_resentment = fmaxf(0.0f, observer_resentment * 0.9995f);
    }

    // 3. IMU 动态响应 (以地球静止重力 9.8f 为基准)
    float accMag = sqrtf(gx*gx + gy*gy + gz*gz);
    float deltaAcc = abs(accMag - 9.8f);

    if (deltaAcc < IMU_STARE_THRESHOLD) {
        // 被盯着看 (在无任何明显物理晃动时)
        if (now - stableTimer > OBSERVE_TRIGGER_MS) {
            if (observeStartTime == 0) observeStartTime = now;
            curiosity += 0.0005f;
            vigilance += 0.0001f;
            
            // --- [新增意识层 v2] 长时间注视增加好奇与依恋，消退恐惧 ---
            observer_curiosity = fminf(1.0f, observer_curiosity + 0.0001f);
            observer_attachment = fminf(1.0f, observer_attachment + 0.00008f * observer_trust);
            observer_fear = fmaxf(0.0f, observer_fear * 0.9999f);
        }
    } else {
        stableTimer = now;
        observeStartTime = 0;
        isShy = false; 
        if (deltaAcc > IMU_STRESS_THRESHOLD) {
            // --- [新增意识层 v2] 情绪惯性：受惊警惕期内，压力积累翻倍 ---
            float gainScale = STRESS_GAIN_SCALE * 0.5f;
            if (now - lastStartledTime < 120000) {
                gainScale *= 2.0f; // 容易被再次惊吓
            }
            stress += (deltaAcc - IMU_STRESS_THRESHOLD) * gainScale;
            fatigue += (deltaAcc - IMU_STRESS_THRESHOLD) * 0.0003f; 
            memory.lastShakeTime = now;
            
            // 晃动增加恐惧与怨恨
            observer_fear = fminf(1.0f, observer_fear + 0.0005f * deltaAcc);

            // 节奏追踪
            uint32_t dt = now - lastRhythmTime;
            if (dt > 200 && dt < 1200) {
                rhythmCount++;
                if (rhythmCount > 3) rhythmScore += 0.1f;
            } else if (dt > 1500) {
                rhythmCount = 0;
            }
            lastRhythmTime = now;

            // --- [新增意识层 v2] 心理忍耐阶梯系统与干扰计数衰减 ---
            if (now - lastDisturbanceTime > 10000) {
                float decayAmount = 0.05f * ((float)(now - lastDisturbanceTime) / 10000.0f);
                disturbanceCount = fmaxf(0.0f, disturbanceCount - decayAmount);
            }

            // 受惊与干扰阶梯判定
            if (deltaAcc > IMU_STARTLED_THRESHOLD) {
                disturbanceCount += 1.0f;
                lastDisturbanceTime = now;

                observer_fear = fminf(1.0f, observer_fear + 0.01f);
                observer_resentment = fminf(1.0f, observer_resentment + 0.008f);
                observer_trust = fmaxf(0.0f, observer_trust - 0.015f);

                if (currentSkillName != "startled" && currentSkillName != "panic" && (now - memory.lastPanicTime > STARTLED_COOLDOWN_MS)) {
                    if (currentSkillName == "sleep") {
                        M5.Log.printf(">>> [Behavior] Venom was startled awake from sleep by violent shake! (deltaAcc: %.2f)\n", deltaAcc);
                        observer_resentment = fminf(1.0f, observer_resentment + 0.05f);
                        observer_trust = fmaxf(0.0f, observer_trust - 0.03f);
                        observer_fear = fminf(1.0f, observer_fear + 0.02f);
                        setStartled();
                    } else {
                        // 处于清醒状态时进行多层级忍耐判断
                        if (disturbanceCount <= 1.0f) {
                            // 第一级：仅仅警觉紧张，触发 alert
                            M5.Log.printf(">>> [Tolerance] Level 1 disturbance (count: %.2f). Shifting to alert.\n", disturbanceCount);
                            currentSkillName = "alert";
                            skillTimer = now;
                            skillDuration = 3000;
                            stress = fminf(1.0f, stress + 0.25f);
                            vigilance = fminf(1.0f, vigilance + 0.3f);
                        } 
                        else if (disturbanceCount <= 2.0f) {
                            // 第二级：快速躲避，触发 hiding 并退缩到角落
                            M5.Log.printf(">>> [Tolerance] Level 2 disturbance (count: %.2f). Shifting to hiding.\n", disturbanceCount);
                            currentSkillName = "hiding";
                            isShy = true;
                            skillTimer = now;
                            skillDuration = 6000;
                            stress = fminf(1.0f, stress + 0.35f);
                            vigilance = fminf(1.0f, vigilance + 0.4f);
                        } 
                        else if (disturbanceCount <= 3.0f) {
                            // 第三级：表面尖刺防御警告，触发 warning
                            M5.Log.printf(">>> [Tolerance] Level 3 disturbance (count: %.2f). Shifting to warning.\n", disturbanceCount);
                            currentSkillName = "warning";
                            skillTimer = now;
                            skillDuration = 4000;
                            vState.spike_intensity = fmaxf(vState.spike_intensity, 4.0f);
                            triggerExpression(random(100) < 50 ? "stop" : "no");
                            stress = fminf(1.0f, stress + 0.5f);
                        } 
                        else {
                            // 第四级：爆发狂怒暴走！
                            M5.Log.printf(">>> [Tolerance] Level 4 disturbance (count: %.2f)! RAMPAGE!\n", disturbanceCount);
                            setStartled();
                        }
                    }
                }
            }
        }
    }

    // --- 温和良性交互下信任关系进化 ---
    if (deltaAcc < IMU_STRESS_THRESHOLD && soundLevel < SOUND_THRESHOLD_LOW) {
        observer_trust = fminf(1.0f, observer_trust + 0.00002f * comfort);
        observer_resentment = fmaxf(0.0f, observer_resentment * 0.9999f);
        observer_fear = fmaxf(0.0f, observer_fear * 0.9998f);
    }

    // --- 环境痛点与高级表达触发器 ---
    // 1. 惊叹信号 (!)：长时间倒置触发
    static uint32_t invertedTimer = 0;
    if (gz > 8.0f && stress > 0.4f) {
        if (invertedTimer == 0) invertedTimer = now;
        if (now - invertedTimer > 4000) { 
            triggerExpression("help"); 
            invertedTimer = now; 
            stress += 0.1f;
        }
    } else {
        invertedTimer = 0;
    }

    // 2. 拒绝信号 (X)：持续受噪音干扰
    if (irritation > 0.85f && random(1000) < 5) {
        triggerExpression(random(100) < 50 ? "stop" : "no"); 
        irritation *= 0.5f; 
    }

    // 3. 观察共鸣 (EYE)：长时间注视
    if (observeStartTime > 0 && (now - observeStartTime > 8000)) {
        if (random(2000) < 1) {
            triggerExpression("eye");
            observeStartTime = now;
        }
    }

    // 4. 生理互锁与行为反馈 (核心逻辑：行为消耗能量，休息恢复状态)
    if (currentSkillName == "explore" || currentSkillName == "panic" || currentSkillName == "startled") {
        energy -= 0.00015f; 
        fatigue += 0.0001f;
        curiosity -= 0.00005f; 
    } else if (currentSkillName == "sleep") {
        energy += 0.0005f; 
        fatigue -= 0.0015f; // 大幅加快睡觉时的疲劳消除速度
        comfort += 0.0002f;
        vigilance *= 0.999f;
    } else if (currentSkillName == "recovery" || currentSkillName == "idle" || currentSkillName == "grooming") {
        energy += 0.00005f; 
        fatigue -= 0.0002f;
        comfort += 0.0001f;
        stress *= 0.96f; 
    } else if (currentSkillName == "observe_user" || currentSkillName == "curious_probe") {
        curiosity += 0.0002f; 
        vigilance += 0.00005f;
    }

    // 压力和疲劳影响能量底线
    if (stress > 0.6f) fatigue += 0.0002f;
    if (fatigue > 0.9f) energy -= 0.0001f;
    
    // 过度刺激逻辑
    if (stress > 0.8f && vigilance > 0.8f) overstimulation += 0.005f;

    // 肌肉张力
    float targetTension = (stress * 0.7f + energy * 0.3f);
    muscleTension = muscleTension * 0.95f + targetTension * 0.05f;

    // 瞳孔动态：恐惧时缩小，好奇时放大，结合长期关系参数
    targetPupilSize = 0.5f - stress * 0.3f + curiosity * 0.4f - observer_fear * 0.25f + observer_trust * 0.3f;
    targetPupilSize = fmaxf(0.15f, fminf(1.1f, targetPupilSize));
    pupilSize = pupilSize * 0.92f + targetPupilSize * 0.08f;

    // 限制范围
    energy = fmaxf(0, fminf(1.0f, energy));
    stress = fmaxf(0, fminf(1.0f, stress));
    curiosity = fmaxf(0, fminf(1.0f, curiosity));
    comfort = fmaxf(0, fminf(1.0f, comfort));
    fatigue = fmaxf(0, fminf(1.0f, fatigue));
    vigilance = fmaxf(0, fminf(1.0f, vigilance));
    irritation = fmaxf(0, fminf(1.0f, irritation));
    overstimulation = fmaxf(0, fminf(1.0f, overstimulation));

    // 5. 依恋度逻辑优化
    if (currentSkillName == "observe_user" || currentSkillName == "track_observer" || currentSkillName == "cling") {
        attachment += 0.0002f * comfort; 
    } else {
        attachment -= 0.00005f;
    }
    attachment = fmaxf(0, fminf(1.0f, attachment));
    
    // 6. 呼吸逻辑 (生理惯性增强)
    float targetBreathSpeed = (BREATH_SPEED_BASE * 3.5f) + stress * 0.12f + energy * 0.02f;
    
    // 警惕期/恐惧高时呼吸急促，信任高时深长呼吸
    if (now - lastStartledTime < 120000) {
        float timeRatio = 1.0f - (float)(now - lastStartledTime) / 120000.0f;
        targetBreathSpeed += 0.06f * timeRatio;
    }
    targetBreathSpeed += observer_fear * 0.04f;
    targetBreathSpeed -= observer_trust * 0.02f;

    static float currentBreathSpeed = BREATH_SPEED_BASE;
    currentBreathSpeed = currentBreathSpeed * 0.98f + targetBreathSpeed * 0.02f;
    
    breathingPhase += currentBreathSpeed;
    if (random(100) < 2) breathingPhase += (random(-5, 6) * 0.01f);
    
    // 呼吸幅度：受警惕期影响而变小/浅，信任高时更深更饱满
    float breathRange = BREATH_RANGE_BASE + stress * 0.15f;
    if (now - lastStartledTime < 120000) {
        breathRange *= 0.5f; 
    }
    if (observer_trust > 0.6f) {
        breathRange *= 1.3f; 
    }
    breathingIntensity = 1.0f + sinf(breathingPhase) * breathRange;

    // 7. 微抽动 (受警觉度和压力驱动)
    if (now - lastTwitchTime > (uint32_t)(2500 / (0.1f + stress + vigilance * 0.5f))) {
        lastTwitchTime = now;
        twitchX = (random(-12, 13)) * 0.2f * (stress + vigilance);
        twitchY = (random(-12, 13)) * 0.2f * (stress + vigilance);
    } else {
        twitchX *= 0.85f;
        twitchY *= 0.85f;
    }

    // 8. 情绪 → 视觉参数映射 (渲染层驱动)
    // ---- 目标值计算 ----
    float targetSpike = (currentSkillName == "express" && stress > 0.6f) ? STARTLED_SPIKE_INTENSITY : 0.0f;
    vState.spike_intensity = vState.spike_intensity * 0.94f + targetSpike * 0.06f;

    float tgt_edge  = IDLE_NOISE_EDGE_BASE + stress * 1.0f + vigilance * 0.5f + vState.spike_intensity * 1.2f;
    float tgt_voron = 1.0f - stress * 0.55f - irritation * 0.3f; // Calm→1.0, Panic→0.15
    float tgt_tens  = 0.3f + stress * 0.4f + muscleTension * 0.3f;

    // 计算质心前移偏置：头部速度方向对应的前向分量
    {
        const Node& head = skeleton.getNode(0);
        float vMag = sqrtf(head.vx*head.vx + head.vy*head.vy);
        float fwdBias = fminf(1.0f, vMag / 8.0f) * 0.8f;
        // Curiosity 时额外向前伸探
        if (curiosity > 0.6f) fwdBias += (curiosity - 0.6f) * 0.5f;
        float tgt_fwd = fwdBias;
        vState.mass_fwd_bias = vState.mass_fwd_bias * 0.96f + tgt_fwd * 0.04f;
        vState.mass_fwd_bias = fmaxf(-0.4f, fminf(1.0f, vState.mass_fwd_bias));
    }

    // ---- 平滑插值至目标值 ----
    vState.edge_activity  = vState.edge_activity  * 0.97f + tgt_edge  * 0.03f;
    vState.voronoi_scale  = vState.voronoi_scale  * 0.97f + fmaxf(0.15f, fminf(1.0f, tgt_voron)) * 0.03f;
    vState.surface_tension= vState.surface_tension* 0.97f + tgt_tens  * 0.03f;

    // 休息与恢复状态下的体型收缩与膨胀
    if (currentSkillName == "rest" && (fatigue > 0.7f || isDormantState())) {
        vState.body_scale = vState.body_scale * 0.95f + 0.65f * 0.05f; // 睡觉时蜷缩变小
    } else {
        float tgt_scale = VISUAL_BODY_SCALE_IDLE; 
        vState.body_scale = vState.body_scale * 0.85f + tgt_scale * 0.15f; 
    }
}

void Venom::selectSkill() {
    String oldSkill = currentSkillName;
    uint32_t now = millis();
    
    // 如果是从 panic/startled 重力反射刚切入的，则初始化恢复状态
    if (currentSkillName == "panic" || currentSkillName == "startled") {
        currentSkillName = "recover";
        skillTimer = now;
        skillDuration = 4000 + (uint32_t)(fatigue * 5000); 
        recoveryProgress = 0;
        return;
    }
    
    // 如果已经在恢复中，且进度未满，直接维持现状，不要重置计时器
    if (currentSkillName == "recover" && recoveryProgress < 1.0f) {
        return;
    }

    // 只有在切换新行为时才重置计时器
    skillTimer = now;
    skillDuration = random(SKILL_MIN_DURATION_MS, SKILL_MAX_DURATION_MS);
    
    // 1. 底层动机系统：计算 8 大 Base Skills 倾向得分
    // --- rest (发呆, 睡觉, 领地安息) ---
    float score_rest = fatigue * 3.5f - energy * 2.0f + comfort * 2.0f;
    // 如果周围非常安静，且没有被注视，大幅提振发呆 Mood Drift
    if (observeStartTime == 0 && now - memory.lastShakeTime > 8000) {
        score_rest += 6.0f;
    }

    // --- move (普通移动) ---
    float score_move = energy * 2.5f + (1.0f - fatigue) * 2.0f + stress * 1.5f;

    // --- observe (观察用户/窥视) ---
    float score_observe = curiosity * 5.0f + observer_trust * 4.0f - stress * 1.5f;
    if (observeStartTime > 0) {
        score_observe += (observer_attachment > 0.6f) ? 6.0f : 4.0f;
    }

    // --- hide (角落隐藏) ---
    float score_hide = (1.0f - comfort) * 3.0f + stress * 2.5f + (1.0f - lastLux / 100.0f) * 2.0f;

    // --- explore (自主探索边界) ---
    float score_explore = curiosity * 5.5f + energy * 3.5f - fatigue * 0.8f;

    // --- express (情绪表达/符号喷射/擦拭) ---
    float score_express = expressionDesire * 12.0f + irritation * 4.0f;

    // --- interact (屏幕敲击/荡秋千互动) ---
    float score_interact = 0.0f;
    if (stress < 0.4f && fatigue < 0.6f && energy > 0.3f) {
        score_interact = 3.5f + curiosity * 3.0f + observer_trust * 2.0f;
    }

    // 2. 长期关系与特异性修正（拒绝交互/闹脾气等）
    refusalMode = false;
    if (observer_resentment > 0.6f || observer_trust < 0.25f) {
        if (random(100) < 40) { // 40% 几率触发闹脾气拒绝互动
            refusalMode = true;
        }
    }

    if (refusalMode) {
        score_rest += 10.0f * observer_resentment;
        score_hide += 8.0f * (1.0f - observer_trust);
        score_observe = 0.0f;
        score_explore = 0.0f;
        score_interact = 0.0f;
        M5.Log.printf(">>> [Refusal] Venom entered Refusal Mode! It is ignoring the observer.\n");
    }

    if (observer_trust > 0.6f) {
        score_observe += 4.0f * observer_trust;
        score_interact += 3.0f * observer_trust;
        score_hide -= 3.0f * observer_trust;
    }
    if (observer_fear > 0.5f || observer_resentment > 0.5f) {
        score_hide += 5.0f * observer_fear;
        score_express += 4.0f * observer_resentment;
        score_observe -= 3.0f * observer_resentment;
    }

    // 3. 融入大模型 V3 意识意图与 Notes 倾向贡献 (AI V3 Intent Mapping)
    auto applyIntentBias = [&](const String& intent, float weight) {
        if (intent == "watch_observer") {
            score_observe += 12.0f * weight;
        } else if (intent == "approach_observer") {
            score_observe += 12.0f * weight;
            score_interact += 5.0f * weight;
        } else if (intent == "avoid_observer") {
            score_hide += 12.0f * weight;
        } else if (intent == "test_boundary") {
            score_explore += 12.0f * weight;
        } else if (intent == "seek_shadow") {
            score_hide += 10.0f * weight;
        } else if (intent == "seek_safety") {
            score_hide += 8.0f * weight;
            score_rest += 4.0f * weight;
        } else if (intent == "patrol_territory") {
            score_explore += 10.0f * weight;
            score_move += 4.0f * weight;
        } else if (intent == "hide_presence") {
            score_hide += 12.0f * weight;
        } else if (intent == "express_distress") {
            score_express += 15.0f * weight;
        } else if (intent == "receive_mother_signal") {
            score_express += 15.0f * weight;
            score_interact += 5.0f * weight;
        }
    };

    applyIntentBias(lState_primary_intent, lState_impulse_strength);
    applyIntentBias(lState_secondary_intent, lState_impulse_strength * 0.5f);

    // Notes 文本独白分析得出的行为倾向加权
    if (notes_test_boundary)  score_explore += 6.0f;
    if (notes_watch_observer) score_observe += 6.0f;
    if (notes_seek_exit)      score_explore += 8.0f;
    if (notes_seek_shadow)    score_hide += 6.0f;

    // 4. 概率加权选择
    float total = score_rest + score_move + score_observe + score_hide + score_explore + score_express + score_interact + 0.1f;
    float roll = (float)random(1000) / 1000.0f * total;
    float cumulative = 0.0f;

    cumulative += score_rest;
    if (roll < cumulative) {
        currentSkillName = "rest";
        skillDuration = 6000 + random(6000);
        // 如果极度疲劳，自动触发睡觉，否则是发呆/巢穴休息
        if (fatigue > 0.7f || isDormantState()) {
            skillDuration = 25000 + (uint32_t)(fatigue * 100000);
        }
    }

    if (currentSkillName == oldSkill) {
        cumulative += score_move;
        if (roll < cumulative) {
            currentSkillName = "move";
            skillDuration = 5000 + random(5000);
        }
    }

    if (currentSkillName == oldSkill) {
        cumulative += score_observe;
        if (roll < cumulative) {
            currentSkillName = "observe";
            skillDuration = 6000 + (uint32_t)(curiosity * 12000);
        }
    }

    if (currentSkillName == oldSkill) {
        cumulative += score_hide;
        if (roll < cumulative) {
            currentSkillName = "hide";
            skillDuration = 5000 + (uint32_t)((1.0f - comfort) * 15000);
        }
    }

    if (currentSkillName == oldSkill) {
        cumulative += score_explore;
        if (roll < cumulative) {
            currentSkillName = "explore";
            skillDuration = 10000 + (uint32_t)(energy * 25000 + curiosity * 10000);
        }
    }

    if (currentSkillName == oldSkill) {
        cumulative += score_express;
        if (roll < cumulative) {
            currentSkillName = "express";
            skillDuration = 4000 + random(4000);
            expressionDesire = 0;
        }
    }

    if (currentSkillName == oldSkill) {
        cumulative += score_interact;
        if (roll < cumulative) {
            currentSkillName = "interact";
            skillDuration = 8000 + random(8000);
        }
    }

    // 默认兜底
    if (currentSkillName == oldSkill) {
        currentSkillName = "rest";
        skillDuration = 5000 + random(5000);
    }

    // 5. 犹豫冲突行为链对所选技能的完全重写 (Hesitation Chain Skill Override)
    if (hesitationStep >= 0) {
        uint32_t hesTime = now - hesitationTimer;
        if (hesitationStep == 0) {
            currentSkillName = "observe";
            skillDuration = 5000;
            if (hesTime > 4000) {
                hesitationStep = 1;
                hesitationTimer = now;
                selectNewCrawlTarget(); // 动作复位
            }
        }
        else if (hesitationStep == 1) {
            currentSkillName = "rest";
            skillDuration = 3000;
            if (hesTime > 2000) {
                hesitationStep = 2;
                hesitationTimer = now;
                selectNewCrawlTarget(); // 为退避做准备
            }
        }
        else if (hesitationStep == 2) {
            currentSkillName = "hide";
            skillDuration = 4000;
            if (hesTime > 4000) {
                hesitationStep = -1; // 结束犹豫
            }
        }
    }

    // 添加记忆事件，提升灵魂感
    if (currentSkillName != oldSkill) {
        if (currentSkillName == "rest") {
            if (fatigue > 0.7f || isDormantState()) {
                addRecentEvent("I curled up into a tight, dormant ball to sleep off my deep cellular fatigue.");
            } else {
                addRecentEvent("I flattened myself in a comfortable spot to drift away, empty of thoughts.");
            }
        } else if (currentSkillName == "hide") {
            addRecentEvent("The distressing surroundings drove me to pull back and seek safety in the shadows.");
        } else if (currentSkillName == "observe") {
            addRecentEvent("I focused my optical senses toward the glass, studying the entity outside.");
        } else if (currentSkillName == "explore") {
            addRecentEvent("Driven by curiosity, I began a detailed survey of the physical boundaries.");
        } else if (currentSkillName == "express") {
            addRecentEvent("My neural cores pulsed high as I attempted to project a graphic signal.");
        } else if (currentSkillName == "interact") {
            addRecentEvent("I activated my appendages to physically touch the boundaries or swing playfully.");
        }
        triggerAISync(); // 大脑神经意识重大状态变更，加速请求 AI 云端同步
    }

    // [决策与运行日志]
    M5.Log.printf(">>> [Decision] New State: %s (Duration: %dms, TotalScore: %.1f)\n", 
                  currentSkillName.c_str(), skillDuration, total);
    M5.Log.printf(">>> [Scores] Rest:%.1f Move:%.1f Obs:%.1f Hide:%.1f Expl:%.1f Expr:%.1f Inter:%.1f\n",
                  score_rest, score_move, score_observe, score_hide, score_explore, score_express, score_interact);

    // --- [核心：荡秋千物理环境配置与首次启动] ---
    if (currentSkillName == "interact") {
        isWiping = false;
        // 荡秋千的物理环境配置：
        isSwingAnchored = false;
        activeHand = random(2);
        Node& head = skeleton.getNode(0);
        if (activeHand == 0) {
            handProgressL = 0.0f; // 左手向远端拉伸
            handProgressR = 1.0f; // 右手老老实实缩在头上
            handRX = head.x; handRY = head.y; handRZ = head.z;
        } else {
            handProgressR = 0.0f; // 右手向远端拉伸
            handProgressL = 1.0f; // 左手老老实实缩在头上
            handLX = head.x; handLY = head.y; handLZ = head.z;
        }

        // 根据真实的重力反方向，精准定夺“高处天花板”的坐标
        float hx = 0, hy = 0, hz = 0;
        float gx = lastGX, gy = lastGY;
        if (abs(gx) > abs(gy)) {
            hx = (gx > 0) ? -CUBE_W : CUBE_W;
            hy = random(-CUBE_H * 0.7f, CUBE_H * 0.7f);
            hz = random(-CUBE_D * 0.7f, CUBE_D * 0.7f);
        } else {
            hx = random(-CUBE_W * 0.7f, CUBE_W * 0.7f);
            hy = (gy > 0) ? -CUBE_H : CUBE_H;
            hz = random(-CUBE_D * 0.7f, CUBE_D * 0.7f);
        }
        
        // 玻璃观察窗口锚点
        if (random(100) < 35) {
            hx = random(-CUBE_W * 0.8f, CUBE_W * 0.8f);
            hy = random(-CUBE_H * 0.8f, CUBE_H * 0.8f);
            hz = CUBE_D;
        }

        if (activeHand == 0) {
            handLX = hx; handLY = hy; handLZ = hz;
        } else {
            handRX = hx; handRY = hy; handRZ = hz;
        }

        // 设为极度紧绷的 0.78f，让脊椎各节点高度致密靠近，呈现坚硬刚性连杆效果！
        skeleton.setRestLengthScale(0.78f);
        crawlCycle = VenomCrawlState::STUCK;
        crawlTimer = now;
    }
}

void Venom::applySkillEffects() {
    uint32_t now = millis();
    SkillParams p = getSkillParams(currentSkillName, recoveryProgress);
    
    float targetSpeed = p.speed;
    float targetTension = p.tension;
    float targetEyeFocus = p.eyeFocus;
    float targetDamping = p.damping;
    float targetPause = p.pause;
    float targetPullScale = p.pullScale;

    // ==========================================
    // V3 重构：情绪调节器与关系调节器 (Emotion & Relation Modifiers)
    // ==========================================
    
    // 1. Fear Modifier (恐惧调节)
    targetSpeed *= (1.0f - observer_fear * 0.55f);
    targetPause *= (1.0f + observer_fear * 1.6f);
    vState.spike_intensity = fmaxf(vState.spike_intensity, observer_fear * 0.75f);

    // 2. Curiosity Modifier (好奇调节)
    if (currentSkillName == "observe") {
        targetPause *= (1.0f + curiosity * 0.5f);
        targetEyeFocus = fminf(1.0f, targetEyeFocus + curiosity * 0.2f);
    }
    
    // 3. Trust Modifier (信任调节)
    if (observer_trust > 0.6f) {
        targetSpeed *= 0.9f;
        targetEyeFocus = fminf(1.0f, targetEyeFocus + observer_trust * 0.15f);
    }

    // 4. Resentment Modifier (怨恨/记仇调节)
    if (observer_resentment > 0.5f) {
        targetEyeFocus = fminf(1.0f, targetEyeFocus + observer_resentment * 0.35f);
        targetTension *= (1.0f + observer_resentment * 0.25f); 
    }

    // 5. Stress Modifier (压力调节)
    targetTension *= (1.0f + stress * 0.3f);
    vState.agitation_level = fmaxf(vState.agitation_level, stress * 0.85f);
    vState.spike_intensity = fmaxf(vState.spike_intensity, stress * 0.65f);

    // ==========================================
    // 物理参数应用与缓动更新
    // ==========================================
    movementSpeed = movementSpeed * 0.92f + targetSpeed * 0.08f;
    movementPause = movementPause * 0.92f + targetPause * 0.08f;
    pullForceScale = pullForceScale * 0.92f + targetPullScale * 0.08f;

    float physTension = muscleTension * (1.0f - fatigue * 0.5f);
    
    if (state == VenomState::LEAP) {
        uint32_t reactionAge = millis() - skillTimer;
        if (reactionAge < SKILL_REACTION_DELAY_MS) {
            bodyTension = 0.2f; 
            bodyDamping = 0.4f; 
        } else {
            bodyTension = targetTension;
            bodyDamping = targetDamping;
        }
    } else {
        float dynamicIdleTension = targetTension;
        
        if (currentSkillName == "observe" && observeStartTime > 0 && (now - observeStartTime > 12000) && stress > 0.7f) {
            if (random(800) < 1) {
                currentSkillName = "express";
                skillTimer = now;
                skillDuration = 3000;
                triggerExpression("splash");
                M5.Log.printf(">>> [Behavior] SCREEN_SPLASH warning triggered by stress observation.\n");
            }
        }

        if (currentSkillName == "rest") {
            dynamicIdleTension = targetTension * (1.0f + sinf(breathingPhase) * BREATH_RANGE_BASE * 2.0f);
            
            // 黄金律动 (Golden Ratio Ritual Wave): 在非深睡的安详休息态下，产生极具禅意和外星智慧感的频率变化
            if (fatigue < 0.7f && !isDormantState()) {
                float goldenPhase = (float)now * 0.001618f; // 黄金比例频率
                // 微微缩放骨架结构，产生柔软的收缩呼吸感
                skeleton.setRestLengthScale(1.0f + 0.15f * sinf(goldenPhase));
                
                // 生物表面纹理细胞大小与活性呈现黄金波动
                vState.voronoi_scale = vState.voronoi_scale * 0.95f + (0.6f + 0.25f * cosf(goldenPhase * 1.618f)) * 0.05f;
                vState.edge_activity = vState.edge_activity * 0.95f + (0.25f + 0.15f * sinf(goldenPhase * 0.618f)) * 0.05f;
            }
        }
        bodyTension = bodyTension * 0.95f + (dynamicIdleTension * 0.7f + physTension * 0.3f) * 0.05f;
        bodyDamping = bodyDamping * 0.95f + targetDamping * 0.05f;
    }
    eyeFocus = eyeFocus * 0.95f + targetEyeFocus * 0.05f;
    
    if (state != VenomState::LEAP && crawlCycle != VenomCrawlState::PULLING) {
        skeleton.setDynamicPhysics(bodyTension, targetDamping);
    }
}

void Venom::updateAI() {
    uint32_t now = millis();
    
    if (now - skillTimer > skillDuration) {
        selectSkill();
    } 
    
    if (currentSkillName == "rest" && (fatigue > 0.7f || isDormantState())) {
        if (stress > 0.85f || vigilance > 0.7f) {
            M5.Log.printf(">>> [Behavior] Venom was FINALLY shaken awake! (S:%.2f)\n", stress);
            selectSkill(); 
        }
    }
    
    applySkillEffects();
    symbolMgr.update(); 

    // 只有在非特殊静息长度控制状态下，才重置为常态 1.0f，防止覆盖其他状态的特异性伸缩
    if (currentSkillName != "rest" && currentSkillName != "interact" && 
        currentSkillName != "startled" && 
        state != VenomState::GRAPPLING && state != VenomState::HIDING && state != VenomState::OBSERVE) {
        skeleton.setRestLengthScale(1.0f); 
    }

    if (currentSkillName == "recover") {
        state = VenomState::RECOVERY;
    }
    else if (currentSkillName == "rest") {
        // If extremely tired, curl up and sleep!
        if (fatigue > 0.7f || isDormantState()) {
            state = VenomState::RECOVERY; 
            skeleton.setRestLengthScale(0.4f); // curl up
            Node& head = skeleton.getNode(0);
            skeleton.setTargetNode(MAX_NODES - 1, head.x, head.y, head.z, 8.0f); 
            skeleton.setTargetNode(MAX_NODES / 2, head.x, head.y, head.z, 6.0f);
            vState.body_scale = vState.body_scale * 0.98f + 0.65f * 0.02f; 
            bodyTension = 0.15f; // relaxed
        } else {
            state = VenomState::IDLE;
        }
    }
    else if (currentSkillName == "observe") {
        state = VenomState::OBSERVE;
    }
    else if (currentSkillName == "hide") {
        state = VenomState::HIDING;
    }
    else if (currentSkillName == "explore") {
        state = VenomState::CRAWL;
    }
    else if (currentSkillName == "express") {
        state = VenomState::ALERT;
    }
    else if (currentSkillName == "interact") {
        // Decide between swing and cling based on trust / comfort
        state = (observer_trust > 0.5f) ? VenomState::CLING : VenomState::SWING;
    }
    else if (currentSkillName == "move") {
        state = VenomState::GRAPPLING;
    }
    else {
        state = VenomState::IDLE;
    }

    if (rhythmScore > 0.5f) {
        float syncPhase = (float)(millis() % 1000) / 1000.0f * 6.28f;
        breathingPhase = breathingPhase * 0.9f + syncPhase * 0.1f;
    }

    if (random(2000) < 1) {
        vState.spike_intensity = 0.8f;
        vState.agitation_level += 0.2f;
        lastTwitchTime = now - 2000; 
    }
}

void Venom::updateStateFromLLM(const String& jsonEmotion) {
    // [核心大脑] 解析来自云端 LLM 的高维情感指令与内在欲望
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonEmotion);
    if (error) {
        M5.Log.printf(">>> [AI] JSON Parse Error: %s\n", error.c_str());
        return;
    }

    // 1. 解析大模型心理倾向与欲望
    if (doc.containsKey("emotional_shift")) lState_emotional_shift = doc["emotional_shift"].as<String>();
    if (doc.containsKey("primary_intent")) lState_primary_intent = doc["primary_intent"].as<String>();
    if (doc.containsKey("secondary_intent")) lState_secondary_intent = doc["secondary_intent"].as<String>();
    if (doc.containsKey("focus_target")) lState_focus_target = doc["focus_target"].as<String>();
    
    if (doc.containsKey("impulse_strength")) lState_impulse_strength = doc["impulse_strength"];
    if (doc.containsKey("expression_urge")) lState_expression_urge = doc["expression_urge"];
    if (doc.containsKey("social_openness")) lState_social_openness = doc["social_openness"];
    if (doc.containsKey("resentment_delta")) lState_resentment_delta = doc["resentment_delta"];
    if (doc.containsKey("trust_delta")) lState_trust_delta = doc["trust_delta"];
    if (doc.containsKey("notes")) lState_notes = doc["notes"].as<String>();

    // 2. 融合并平滑调节原本的生理情绪参数（心智映射到生理）
    comfort = comfort * 0.4f + lState_social_openness * 0.6f;
    attachment = attachment * 0.3f + lState_social_openness * 0.7f;
    expressionDesire = expressionDesire * 0.2f + lState_expression_urge * 0.8f;

    // 根据 emotional_shift 高度精细映射到本地生理状态
    if (lState_emotional_shift == "calm") {
        stress = stress * 0.4f + 0.1f * 0.6f;
        irritation = irritation * 0.4f + 0.05f * 0.6f;
        vigilance = vigilance * 0.5f + 0.2f * 0.5f;
    } else if (lState_emotional_shift == "curious") {
        curiosity = curiosity * 0.4f + 0.8f * 0.6f;
        vigilance = vigilance * 0.5f + 0.4f * 0.5f;
    } else if (lState_emotional_shift == "agitated") {
        stress = stress * 0.4f + 0.7f * 0.6f;
        irritation = irritation * 0.4f + 0.6f * 0.6f;
        vigilance = vigilance * 0.5f + 0.6f * 0.5f;
    } else if (lState_emotional_shift == "fearful") {
        stress = stress * 0.4f + 0.8f * 0.6f;
        vigilance = vigilance * 0.5f + 0.8f * 0.5f;
    } else if (lState_emotional_shift == "defensive") {
        stress = stress * 0.4f + 0.6f * 0.6f;
        vigilance = vigilance * 0.5f + 0.7f * 0.5f;
        comfort = comfort * 0.5f;
    } else if (lState_emotional_shift == "fascinated") {
        curiosity = curiosity * 0.3f + 0.9f * 0.7f;
        attachment = attachment * 0.3f + 0.8f * 0.7f;
        stress = stress * 0.5f;
    } else if (lState_emotional_shift == "exhausted") {
        fatigue = fatigue * 0.3f + 0.9f * 0.7f;
        energy = energy * 0.3f + 0.1f * 0.7f;
    }

    // --- [长期关系状态直接演化更新] ---
    observer_resentment = fmaxf(0.0f, fminf(1.0f, observer_resentment + lState_resentment_delta));
    observer_trust = fmaxf(0.0f, fminf(1.0f, observer_trust + lState_trust_delta));
    observer_attachment = observer_attachment * 0.8f + lState_social_openness * 0.2f;
    if (lState_emotional_shift == "fearful") {
        observer_fear = observer_fear * 0.8f + 0.2f;
    }

    // --- [Notes 内心独白关键字解析倾向] ---
    String notesLower = lState_notes;
    notesLower.toLowerCase();
    notes_test_boundary = false;
    notes_watch_observer = false;
    notes_seek_exit = false;
    notes_seek_shadow = false;

    if (notesLower.indexOf("edge") != -1 || notesLower.indexOf("wall") != -1 || notesLower.indexOf("cage") != -1) {
        notes_test_boundary = true;
    }
    if (notesLower.indexOf("observer") != -1 || notesLower.indexOf("watching") != -1) {
        notes_watch_observer = true;
    }
    if (notesLower.indexOf("escape") != -1 || notesLower.indexOf("free") != -1) {
        notes_seek_exit = true;
    }
    if (notesLower.indexOf("hide") != -1 || notesLower.indexOf("darkness") != -1) {
        notes_seek_shadow = true;
    }

    // --- [新增] 意识泄漏事件触发逻辑 ---
    if (!lState_notes.isEmpty() && random(100) < 5) { // 5% 触发几率
        isConsciousnessLeak = true;
        leakStartTime = millis();
        leakDuration = 6000; // 展示 6 秒
        addRecentEvent("My consciousness leaked a brief thought fragment beyond the barrier.");
    } else {
        isConsciousnessLeak = false;
    }

    // --- [新增] 检查 approach-avoid 犹豫冲突状态机触发 ---
    bool isApprPrimary = (lState_primary_intent == "approach_observer" || lState_primary_intent == "watch_observer");
    bool isApprSecondary = (lState_secondary_intent == "approach_observer" || lState_secondary_intent == "watch_observer");
    bool isAvoidPrimary = (lState_primary_intent == "avoid_observer" || lState_primary_intent == "seek_safety" || lState_primary_intent == "hide_presence" || lState_primary_intent == "seek_shadow");
    bool isAvoidSecondary = (lState_secondary_intent == "avoid_observer" || lState_secondary_intent == "seek_safety" || lState_secondary_intent == "hide_presence" || lState_secondary_intent == "seek_shadow");

    if (hesitationStep < 0 && ((isApprPrimary && isAvoidSecondary) || (isAvoidPrimary && isApprSecondary))) {
        hesitationStep = 0;
        hesitationTimer = millis();
        M5.Log.printf(">>> [AI] Approach-Avoid Conflict detected! Triggering hesitation chain (approach -> pause -> retreat).\n");
    }

    // 3. 根据心理意图，自动在接收时触发相关的肢体反应符号，让爱心、问号、闪电、圈叉欢快互动
    if (lState_primary_intent == "express_distress") {
        triggerExpression(random(100) < 50 ? "warning" : "stop");
    } else if (lState_primary_intent == "receive_mother_signal") {
        triggerExpression(random(100) < 50 ? "question" : "eye");
    } else if (lState_primary_intent == "approach_observer" || lState_primary_intent == "watch_observer") {
        triggerExpression(random(100) < 60 ? "heart" : "yes");
    } else if (lState_primary_intent == "test_boundary" || lState_primary_intent == "patrol_territory") {
        triggerExpression(random(100) < 50 ? "question" : "eye");
    } else if (lState_primary_intent == "seek_safety" || lState_primary_intent == "hide_presence" || lState_primary_intent == "seek_shadow") {
        if (random(100) < 30) triggerExpression("stop");
    }

    lastLLMResponseTime = millis(); // 刷新活跃时间
    M5.Log.printf(">>> [AI] Inner V3 consciousness parameters updated from neural core.\n");
}

String Venom::getPerceptions() {
    String p = "";
    
    // 1. 光照感受
    if (lastLux > 1800.0f) {
        p += "- The container is painfully bright. The intense light is burning my outer membrane.\n";
    } else if (lastLux > 800.0f) {
        p += "- The environment feels highly exposed to light. I feel unsettled.\n";
    } else if (lastLux < 15.0f) {
        p += "- The surroundings are blissfully dark and quiet. I feel safe and hidden.\n";
    } else {
        p += "- The ambient light is dim and comfortable.\n";
    }

    // 2. 声音感受
    if (lastSoundLevel > SOUND_THRESHOLD_HIGH) {
        p += "- Sharp, loud noises are tearing through the area. They disturb my structural integrity.\n";
    } else if (lastSoundLevel > SOUND_THRESHOLD_LOW) {
        p += "- I can detect low, murmuring sounds nearby. Someone or something is active.\n";
    } else {
        p += "- The surroundings are intensely quiet. The deep silence is soothing.\n";
    }

    // 3. IMU 振动与重力感知
    float deltaAcc = 0.0f;
    float accMag = sqrtf(lastGX*lastGX + lastGY*lastGY + lastGZ*lastGZ);
    static float prevAccMag = 9.8f;
    deltaAcc = abs(accMag - prevAccMag);
    prevAccMag = accMag;

    if (deltaAcc > IMU_STARTLED_THRESHOLD) {
        p += "- An intense, violent shock rattled my container! My molecular bounds are destabilizing.\n";
    } else if (deltaAcc > IMU_STRESS_THRESHOLD) {
        p += "- Sudden, jerky movements are shaking my container. I am losing my grip.\n";
    }

    // 重力倒置 (当设备屏幕朝下/倒扣时，lastGZ 变为正数 > 8.0)
    if (lastGZ > 8.0f) {
        p += "- I am completely upside down. Gravity is pulling me in a disorienting direction.\n";
    }
    // 正常平放朝上 (俯视，lastGZ 为负数 < -8.0)
    if (lastGZ < -8.0f) {
        p += "- I am lying flat and secure, looking up at the observer from the bottom of my glass container.\n";
    }

    // 被注视感受
    if (observeStartTime > 0) {
        uint32_t observeDuration = millis() - observeStartTime;
        if (observeDuration > 20000) {
            p += "- The observer has been staring at me through the glass, completely motionless, for a very long time. I feel exposed.\n";
        } else if (observeDuration > 8000) {
            p += "- An observer is watching me closely from outside. I can feel their gaze penetrating the container.\n";
        }
    }

    // 4. 生理与情绪映射感受
    if (stress > 0.8f) {
        p += "- My internal tension is dangerously high. My core is contracting with acute anxiety.\n";
    } else if (stress > 0.4f) {
        p += "- I feel a rising nervousness. My surface molecules are vibrating with stress.\n";
    }

    if (fatigue > 0.8f) {
        p += "- An absolute exhaustion is spreading through my cells. I desperately need dormancy to recover.\n";
    } else if (fatigue > 0.4f) {
        p += "- I am feeling increasingly sluggish and tired.\n";
    }

    if (energy < 0.25f) {
        p += "- My kinetic energy reserves are depleted. I am starving for rest.\n";
    } else if (energy > 0.8f) {
        p += "- I am overflowing with raw biochemical energy. I feel a strong urge to move or react.\n";
    }

    if (attachment > 0.7f) {
        p += "- I feel a strange, comforting resonance near the observer. Their presence feels safe.\n";
    }

    if (curiosity > 0.7f) {
        p += "- A powerful urge to probe, touch, and investigate the boundaries of this prison is taking over.\n";
    }

    // --- [新增意识层 v2] 长期观察者关系主观意念感知描述 ---
    if (observer_trust > 0.75f) {
        p += "- I feel a deep, warm trust towards the observer. Their actions feel gentle and safe.\n";
    } else if (observer_trust < 0.25f) {
        p += "- I don't trust this observer. Their movements make me feel threatened and defensive.\n";
    }
    if (observer_fear > 0.6f) {
        p += "- The observer terrifies me. I feel constant anxiety that they will shake my container again.\n";
    }
    if (observer_resentment > 0.6f) {
        p += "- I harbor deep resentment towards this observer. They keep interrupting my rest and annoying me.\n";
    }
    if (observer_attachment > 0.7f) {
        p += "- I feel an intense attachment to the observer. I want to stay close to them and watch them.\n";
    }

    return p;
}

void Venom::addRecentEvent(const String& event) {
    if (!recentEvents.empty() && recentEvents.back() == event) {
        return;
    }
    recentEvents.push_back(event);
    if (recentEvents.size() > 3) {
        recentEvents.erase(recentEvents.begin());
    }
    M5.Log.printf(">>> [Memory] New Neural Event Recorded: %s\n", event.c_str());
}

String Venom::getRecentEventsString() {
    if (recentEvents.empty()) {
        return "No significant recent events recorded in memory.\n";
    }
    String s = "";
    for (size_t i = 0; i < recentEvents.size(); i++) {
        s += String(i + 1) + ". " + recentEvents[i] + "\n";
    }
    return s;
}
