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
    char buf[400];
    Node& head = skeleton.getNode(0);
    snprintf(buf, sizeof(buf), 
        "{\"stress\":%.2f,\"energy\":%.2f,\"comfort\":%.2f,\"curiosity\":%.2f,\"attachment\":%.2f,\"fatigue\":%.2f,\"vigilance\":%.2f,\"observed\":%s,\"behavior\":\"%s\",\"pos\":[%.1f,%.1f,%.1f]}",
        stress, energy, comfort, curiosity, attachment, fatigue, vigilance, 
        isBeingObserved() ? "true" : "false", currentSkillName.c_str(),
        head.x, head.y, head.z);
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
    // 1. 情绪与生理自然回归 (核心：惯性与缓慢变化)
    // 基础衰减逻辑：数值倾向于向初始状态回归 (减缓回归权重，保留行为惯性)
    energy = energy * 0.99995f + (INIT_ENERGY * 0.00005f);
    stress = stress * 0.999f; // 压力快速自然消退
    fatigue = fatigue * 0.9998f; 
    vigilance = vigilance * 0.9995f;
    irritation = irritation * 0.9997f;
    curiosity = curiosity * 0.9999f + (INIT_CURIOSITY * 0.0001f);
    comfort = comfort * 0.9999f + (INIT_COMFORT * 0.0001f);
    overstimulation *= 0.999f;
    expressionDesire = expressionDesire * 0.9995f + (stress * 0.0002f); // 压力越大，表达欲望积压越快
    rhythmScore *= 0.999f;

    // 2. 环境声音响应 (逻辑重构：累积与敏感度)
    if (soundLevel > 92.0f && currentSkillName == "sleep") {
        // [新增] 极高分贝声音直接惊醒毒液
        M5.Log.printf(">>> [Behavior] Venom was startled awake by loud noise! (%.1fdB)\n", soundLevel);
        setStartled();
        return; 
    }

    if (soundLevel > SOUND_THRESHOLD_HIGH) {
        stress += 0.05f;
        vigilance += 0.08f;
        energy += 0.01f;
        irritation += 0.02f;
        memory.lastQuietTime = millis();
    } else if (soundLevel > SOUND_THRESHOLD_LOW) {
        vigilance += 0.01f;
        curiosity += 0.002f;
        memory.lastQuietTime = millis();
    } else {
        // 安静环境有利于恢复
        stress *= 0.995f;
        comfort += 0.0001f;
    }

    // 3. IMU 动态响应 (以地球静止重力 9.8f 为基准，测量外界产生的额外物理惯性晃动)
    float accMag = sqrtf(gx*gx + gy*gy + gz*gz);
    float deltaAcc = abs(accMag - 9.8f); // 物理绝对偏差值，彻底激活灵敏手部交互

    if (deltaAcc < IMU_STARE_THRESHOLD) {
        // 被盯着看 (在无任何明显物理晃动时)
        if (millis() - stableTimer > OBSERVE_TRIGGER_MS) {
            if (observeStartTime == 0) observeStartTime = millis();
            curiosity += 0.0005f;
            vigilance += 0.0001f; // 被注视增加微量警觉
        }
    } else {
        stableTimer = millis();
        observeStartTime = 0;
        isShy = false; 
        if (deltaAcc > IMU_STRESS_THRESHOLD) {
            stress += (deltaAcc - IMU_STRESS_THRESHOLD) * STRESS_GAIN_SCALE * 0.5f; // 仅累加超出阈值的超量部分，且缩减一半系数，日常抚摸把玩极其平滑稳健
            fatigue += (deltaAcc - IMU_STRESS_THRESHOLD) * 0.0003f; 
            memory.lastShakeTime = millis();
            
            // 节奏追踪：检测是否有规律的晃动
            uint32_t dt = millis() - lastRhythmTime;
            if (dt > 200 && dt < 1200) {
                rhythmCount++;
                if (rhythmCount > 3) rhythmScore += 0.1f;
            } else if (dt > 1500) {
                rhythmCount = 0;
            }
            lastRhythmTime = millis();

            // 受惊判定 (即使在睡眠 sleep 状态下遭遇强震动也立刻惊醒跳起！)
            if (deltaAcc > IMU_STARTLED_THRESHOLD) {
                if (currentSkillName != "startled" && currentSkillName != "panic" && (millis() - memory.lastPanicTime > STARTLED_COOLDOWN_MS)) {
                    if (currentSkillName == "sleep") {
                        M5.Log.printf(">>> [Behavior] Venom was startled awake from sleep by violent shake! (deltaAcc: %.2f)\n", deltaAcc);
                    }
                    setStartled(); 
                }
            }
        }
    }

    // --- 环境痛点与高级表达触发器 ---
    // 1. 惊叹信号 (!)：长时间倒置触发 (原 HELP 逻辑映射)
    static uint32_t invertedTimer = 0;
    if (gz > 8.0f && stress > 0.4f) {
        if (invertedTimer == 0) invertedTimer = millis();
        if (millis() - invertedTimer > 4000) { 
            triggerExpression("help"); // 内部映射至 !
            invertedTimer = millis(); 
            stress += 0.1f;
        }
    } else {
        invertedTimer = 0;
    }

    // 2. 拒绝信号 (X)：持续受噪音干扰 (原 STOP/NO 映射)
    if (irritation > 0.85f && random(1000) < 5) {
        triggerExpression(random(100) < 50 ? "stop" : "no"); // 内部映射至 X
        irritation *= 0.5f; 
    }

    // 3. 观察共鸣 (EYE)：长时间注视
    if (observeStartTime > 0 && (millis() - observeStartTime > 8000)) {
        if (random(2000) < 1) {
            triggerExpression("eye");
            observeStartTime = millis();
        }
    }

    // 4. 生理互锁与行为反馈 (核心逻辑：行为消耗能量，休息恢复状态)
    if (currentSkillName == "explore" || currentSkillName == "panic" || currentSkillName == "startled") {
        energy -= 0.00015f; // 活动消耗能量
        fatigue += 0.0001f;
        curiosity -= 0.00005f; // 探索满足好奇心
    } else if (currentSkillName == "sleep") {
        energy += 0.0003f; // 睡觉恢复能量更快
        fatigue -= 0.0008f;
        comfort += 0.0002f;
        vigilance *= 0.999f;
    } else if (currentSkillName == "recovery" || currentSkillName == "idle" || currentSkillName == "grooming") {
        energy += 0.00005f; // 轻微恢复
        fatigue -= 0.0002f;
        comfort += 0.0001f;
        stress *= 0.96f; // 大幅加快压力消退，防止末期反复触发受惊
    } else if (currentSkillName == "observe_user" || currentSkillName == "curious_probe") {
        curiosity += 0.0002f; // 观察进一步激发好奇心
        vigilance += 0.00005f;
    }

    // 压力和疲劳影响能量底线
    if (stress > 0.6f) fatigue += 0.0002f;
    if (fatigue > 0.9f) energy -= 0.0001f;
    
    // 过度刺激逻辑：高频率、高强度的输入导致反应过载
    if (stress > 0.8f && vigilance > 0.8f) overstimulation += 0.005f;

    // 肌肉张力受压力 and 能量共同驱动
    float targetTension = (stress * 0.7f + energy * 0.3f);
    muscleTension = muscleTension * 0.95f + targetTension * 0.05f;

    // 瞳孔动态：恐惧时缩小，好奇时放大
    targetPupilSize = 0.5f - stress * 0.3f + curiosity * 0.4f;
    targetPupilSize = fmaxf(0.2f, fminf(1.0f, targetPupilSize));
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
        attachment += 0.0002f * comfort; // 在舒适时依恋增长更快
    } else {
        attachment -= 0.00005f;
    }
    attachment = fmaxf(0, fminf(1.0f, attachment));
    
    // 6. 呼吸逻辑 (生理惯性增强)
    // 提高步进速度：让呼吸节奏更符合生物特征 (约 3-4 秒一周期)
    float targetBreathSpeed = (BREATH_SPEED_BASE * 3.5f) + stress * 0.12f + energy * 0.02f;
    static float currentBreathSpeed = BREATH_SPEED_BASE;
    currentBreathSpeed = currentBreathSpeed * 0.98f + targetBreathSpeed * 0.02f;
    
    breathingPhase += currentBreathSpeed;
    if (random(100) < 2) breathingPhase += (random(-5, 6) * 0.01f);
    // 严格受 BREATH_RANGE_BASE 控制
    breathingIntensity = 1.0f + sinf(breathingPhase) * (BREATH_RANGE_BASE + stress * 0.15f);

    // 7. 微抽动 (受警觉度和压力驱动)
    if (millis() - lastTwitchTime > (uint32_t)(2500 / (0.1f + stress + vigilance * 0.5f))) {
        lastTwitchTime = millis();
        twitchX = (random(-12, 13)) * 0.2f * (stress + vigilance);
        twitchY = (random(-12, 13)) * 0.2f * (stress + vigilance);
    } else {
        twitchX *= 0.85f;
        twitchY *= 0.85f;
    }

    // 8. 情绪 → 视觉参数映射 (渲染层驱动)
    // ---- 目标值计算 ----
    // 增加 vState.spike_intensity 的权重，让尖刺更突出且更长
    float targetSpike = (currentSkillName == "startled" || currentSkillName == "warning") ? STARTLED_SPIKE_INTENSITY : 0.0f;
    vState.spike_intensity = vState.spike_intensity * 0.94f + targetSpike * 0.06f;

    float tgt_edge  = IDLE_NOISE_EDGE_BASE + stress * 1.0f + vigilance * 0.5f + vState.spike_intensity * 1.2f;
    float tgt_voron = 1.0f - stress * 0.55f - irritation * 0.3f; // Calm→1.0大格, Panic→0.15小格
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

    // Panic / Fear 状态：快速响应
    if (currentSkillName == "panic" || currentSkillName == "startled") {
        vState.edge_activity  = fminf(1.0f, vState.edge_activity  + 0.25f);
        vState.voronoi_scale  = fmaxf(0.15f, vState.voronoi_scale - 0.08f);
        // 受惊时仅极小幅膨胀
        vState.body_scale     = fminf(VISUAL_BODY_SCALE_STARTLED, vState.body_scale + 0.10f); 
    } else {
        float tgt_scale = VISUAL_BODY_SCALE_IDLE; 
        vState.body_scale = vState.body_scale * 0.85f + tgt_scale * 0.15f; // 极大加快收缩速度
    }
}

void Venom::selectSkill() {
    String oldSkill = currentSkillName;
    uint32_t now = millis();
    
    // 强制行为链条 (如果是由于 panic/startled 刚切入的，则初始化恢复状态)
    if (currentSkillName == "panic" || currentSkillName == "startled") {
        currentSkillName = "recovery";
        skillTimer = now;
        skillDuration = 5000 + (uint32_t)(fatigue * 5000); // 增加恢复期，防止状态抖动
        recoveryProgress = 0;
        return;
    }
    
    // 如果已经在恢复中，且进度未满，直接维持现状，不要重置计时器
    if (currentSkillName == "recovery" && recoveryProgress < 1.0f) {
        return;
    }

    // 只有在切换新行为时才重置计时器
    skillTimer = now;
    // 基础冷却与持续时间
    skillDuration = random(SKILL_MIN_DURATION_MS, SKILL_MAX_DURATION_MS);
    
    // 1. 动机系统：计算各行为倾向得分 (调整权重使行为更多样)
    float score_panic = stress * 2.5f + overstimulation * 2.5f;
    float score_hide = (1.0f - comfort) * 2.5f + (1.0f - lastLux / 100.0f) * 3.0f; // 增加光照对躲藏的影响
    float score_observe = curiosity * 4.0f + attachment * 3.0f - stress * 1.5f; // 增加好奇驱动
    float score_sleep = fatigue * 6.0f - energy * 1.5f; // 提高疲劳对睡眠的影响
    float score_explore = energy * 2.5f + curiosity * 1.5f - fatigue * 1.2f; 
    float score_groom = comfort * 2.5f + (1.0f - muscleTension) * 1.2f; // 增加理毛频率
    float score_alert = vigilance * 2.5f + irritation * 1.0f;

    // 2. 环境修正
    if (observeStartTime > 0) {
        if (attachment > 0.7f && stress < 0.2f) score_observe += 5.0f; 
        else score_observe += 3.0f;
    }
    if (millis() - memory.lastShakeTime < 5000) score_alert += 2.0f;
    
    // 3. 非语言表达驱动
    float score_warning = expressionDesire * 10.0f + irritation * 5.0f;
    float score_mimicry = rhythmScore * 8.0f + curiosity * 2.0f;
    float score_hesitation = (stress > 0.4f && curiosity > 0.6f) ? 5.0f : 0;

    // --- [NEW] 融入大模型高维内在神经欲望加权得分 ---
    if (lState_desire == "retreat_from_light") {
        score_hide += 12.0f * lState_impulse_strength;
    } else if (lState_desire == "approach_observer") {
        score_observe += 12.0f * lState_impulse_strength;
        score_groom += 5.0f * lState_impulse_strength;
    } else if (lState_desire == "explore_boundaries") {
        score_explore += 12.0f * lState_impulse_strength;
    } else if (lState_desire == "conserve_energy") {
        score_sleep += 8.0f * lState_impulse_strength;
        score_hide += 4.0f * lState_impulse_strength;
    } else if (lState_desire == "express_warning") {
        score_warning += 15.0f * lState_impulse_strength;
        score_alert += 6.0f * lState_impulse_strength;
    } else if (lState_desire == "dormancy") {
        score_sleep += 20.0f * lState_impulse_strength;
    } else if (lState_desire == "distant_resonance") {
        score_mimicry += 20.0f * lState_impulse_strength;
    }

    // 4. 概率选择 (加权随机)
    float total = score_panic + score_hide + score_observe + score_sleep + score_explore + score_groom + score_alert + score_warning + score_mimicry + score_hesitation + 0.1f;
    float roll = (float)random(1000) / 1000.0f * total;

    if (roll < score_warning) {
        currentSkillName = "warning";
        skillDuration = 3000;
        expressionDesire = 0; 
    } else if (roll < score_warning + score_mimicry) {
        currentSkillName = "mimicry";
        skillDuration = 5000 + (uint32_t)(rhythmScore * 10000);
    } else if (roll < score_warning + score_mimicry + score_hesitation) {
        currentSkillName = "hesitation";
        skillDuration = 4000 + random(4000);
    } else if (roll < score_warning + score_mimicry + score_hesitation + score_panic) {
        currentSkillName = "panic";
        skillDuration = 3000 + (uint32_t)(stress * 5000);
    } else if (roll < score_warning + score_mimicry + score_hesitation + score_panic + score_hide) {
        currentSkillName = "hiding";
        skillDuration = 5000 + (uint32_t)((1.0f - comfort) * 15000);
    } else if (roll < score_warning + score_mimicry + score_hesitation + score_panic + score_hide + score_alert) {
        currentSkillName = "alert";
        skillDuration = 3000 + (uint32_t)(vigilance * 12000);
    } else if (roll < score_warning + score_mimicry + score_hesitation + score_panic + score_hide + score_alert + score_observe) {
        if (attachment > 0.7f && comfort > 0.6f) {
            currentSkillName = "trust_observe"; 
            skillDuration = 10000 + random(10000);
        } else if (curiosity > 0.7f && stress < 0.2f) {
            currentSkillName = "curious_probe";
            skillDuration = 4000 + (uint32_t)(curiosity * 10000);
        } else if (curiosity > 0.3f && stress < 0.1f) {
            currentSkillName = "silent_watch"; 
            skillDuration = 15000 + random(15000);
        } else {
            currentSkillName = "observe_user";
            skillDuration = 6000 + (uint32_t)(curiosity * 15000);
        }
    } else if (roll < score_warning + score_mimicry + score_hesitation + score_panic + score_hide + score_alert + score_observe + score_sleep) {
        currentSkillName = "sleep";
        skillDuration = 20000 + (uint32_t)(fatigue * 120000); 
    } else if (roll < score_panic + score_hide + score_alert + score_observe + score_sleep + score_explore) {
        currentSkillName = "explore";
        skillDuration = 10000 + (uint32_t)(energy * 30000 + curiosity * 10000);
    } else if (roll < score_panic + score_hide + score_alert + score_observe + score_sleep + score_explore + score_groom) {
        currentSkillName = "grooming";
        skillDuration = 6000 + (uint32_t)(comfort * 10000);
    } else {
        currentSkillName = "idle";
        skillDuration = 5000 + random(5000);
    }

    if (currentSkillName != oldSkill) {
        if (currentSkillName == "sleep") {
            addRecentEvent("I initiated a long dormancy sequence to repair my structures and restore vital energy reserves.");
        } else if (currentSkillName == "hiding") {
            addRecentEvent("The intense brightness forced me to search for dark corners to contract and hide myself.");
        } else if (currentSkillName == "trust_observe" || currentSkillName == "observe_user" || currentSkillName == "silent_watch") {
            addRecentEvent("I established static visual resonance to closely watch the observer.");
        } else if (currentSkillName == "mimicry") {
            addRecentEvent("I experienced a distant collectively resonant rhythm from my homeworld.");
        } else if (currentSkillName == "explore") {
            addRecentEvent("With stable energy, I extended tendrils to explore the physical boundaries of my prison.");
        } else if (currentSkillName == "panic") {
            addRecentEvent("My internal tension surged, forcing me to retreat rapidly to recover.");
        }
        triggerAISync(); // 大脑神经意识重大状态变更，加速请求 AI 云端同步
    }

    // [新增] 决策日志输出
    M5.Log.printf(">>> [Decision] New State: %s (Duration: %dms, TotalScore: %.1f)\n", 
                  currentSkillName.c_str(), skillDuration, total);
    M5.Log.printf(">>> [Scores] Panic:%.1f Hide:%.1f Obs:%.1f Sleep:%.1f Expl:%.1f Alert:%.1f Warn:%.1f\n",
                  score_panic, score_hide, score_observe, score_sleep, score_explore, score_alert, score_warning);

    // 重置移动循环相关变量
    crawlCycle = VenomCrawlState::STUCK;
    crawlTimer = now;
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

    movementSpeed = movementSpeed * 0.92f + targetSpeed * 0.08f;
    movementPause = movementPause * 0.92f + targetPause * 0.08f;
    pullForceScale = pullForceScale * 0.92f + targetPullScale * 0.08f;

    float physTension = muscleTension * (1.0f - fatigue * 0.5f);
    
    // LEAP 状态（如受惊、恐慌）下，立即应用目标张力
    if (state == VenomState::LEAP) {
        // 【关键改动：反应延迟感】受惊后的前延迟期内保持极低张力，允许物理上的“剥离”与“摊开”发生
        uint32_t reactionAge = millis() - skillTimer;
        if ((currentSkillName == "startled" || currentSkillName == "panic") && reactionAge < SKILL_REACTION_DELAY_MS) {
            bodyTension = 0.2f; // 极致柔软，允许剥离
            bodyDamping = 0.4f; // 高阻尼吸收冲击
        } else {
            bodyTension = targetTension;
            bodyDamping = targetDamping;
        }
    } else {
        // 为 Idle 状态增加动态呼吸波动，使其形态在 0.15 和 0.6 之间起伏，打破圆球感
        float dynamicIdleTension = targetTension;
        // [新增] 扑屏判定：长期被注视且压力大，试图遮挡用户视线
        if (observeStartTime > 0 && (now - observeStartTime > 12000) && stress > 0.6f) {
            if (random(500) < 1) {
                currentSkillName = "warning";
                skillTimer = now;
                skillDuration = 3000;
                triggerExpression("splash");
                M5.Log.printf(">>> [Behavior] SCREEN_SPLASH triggered by over-observation.\n");
            }
        }

        if (currentSkillName == "idle") {
            // 将张力呼吸同步到统一的幅度控制
            dynamicIdleTension = targetTension * (1.0f + sinf(breathingPhase) * BREATH_RANGE_BASE * 2.0f);
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
    } else if (currentSkillName == "sleep") {
        // [调整] 摇醒逻辑：声音已在 updatePhysiology 处理，此处仅处理压力累积唤醒
        // 只有当压力值攒得比较满（0.85）时才会被摇醒
        if (stress > 0.85f || vigilance > 0.7f) {
            M5.Log.printf(">>> [Behavior] Venom was FINALLY shaken awake! (S:%.2f)\n", stress);
            selectSkill(); 
        }
    }
    
    if (currentSkillName == "recovery") {
        recoveryProgress = (float)(now - skillTimer) / skillDuration;
        if (recoveryProgress > 1.0f) recoveryProgress = 1.0f;
    }

    applySkillEffects();
    symbolMgr.update(); 

    // 默认恢复正常骨骼体长
    skeleton.setRestLengthScale(1.0f); 

    if (currentSkillName == "panic" || currentSkillName == "startled") state = VenomState::LEAP;
    else if (currentSkillName == "observe_user" || currentSkillName == "track_observer") state = VenomState::OBSERVE;
    else if (currentSkillName == "curious_probe") state = VenomState::CURIOUS_PROBE;
    else if (currentSkillName == "crawl_edge" || currentSkillName == "explore") state = VenomState::GRAPPLING;
    else if (currentSkillName == "hiding" || currentSkillName == "camouflage") state = VenomState::HIDING;
    else if (currentSkillName == "recovery") state = VenomState::RECOVERY;
    else if (currentSkillName == "sleep") {
        state = VenomState::RECOVERY; 
        // [关键修复] 大幅缩短骨骼静止长度，让身体能真正缩成一团
        skeleton.setRestLengthScale(0.4f); 
        
        Node& head = skeleton.getNode(0);
        // 强制多点向头部聚拢
        skeleton.setTargetNode(MAX_NODES - 1, head.x, head.y, head.z, 8.0f); 
        skeleton.setTargetNode(MAX_NODES / 2, head.x, head.y, head.z, 6.0f);
        
        vState.body_scale = vState.body_scale * 0.98f + 0.65f * 0.02f; 
        bodyTension = 0.15f; // 极度松弛
    }
    else if (currentSkillName == "alert") state = VenomState::ALERT;
    else if (currentSkillName == "cling" || currentSkillName == "trust_observe") state = VenomState::CLING;
    else if (currentSkillName == "grooming") state = VenomState::GROOMING;
    else if (currentSkillName == "warning") {
        state = VenomState::WARNING_POUNCE;
        if (now - skillTimer < 100) symbolMgr.trigger("warning"); 
    }
    else if (currentSkillName == "sleep") {
        state = VenomState::RECOVERY;
        // [新增] 睡眠梦境：偶尔喷出碎片符号
        if (random(8000) < 1) {
            triggerExpression(random(100) < 50 ? "question" : "eye");
        }
    }
    else if (currentSkillName == "mimicry") state = VenomState::MIMICRY;
    else if (currentSkillName == "silent_watch") state = VenomState::SILENT_WATCH;
    else state = VenomState::IDLE;

    if (currentSkillName == "mimicry" && rhythmScore > 0.5f) {
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
    if (doc.containsKey("focus_target")) lState_focus_target = doc["focus_target"].as<String>();
    if (doc.containsKey("desire")) lState_desire = doc["desire"].as<String>();
    
    if (doc.containsKey("surface_instability")) lState_surface_instability = doc["surface_instability"];
    if (doc.containsKey("impulse_strength")) lState_impulse_strength = doc["impulse_strength"];
    if (doc.containsKey("social_openness")) lState_social_openness = doc["social_openness"];
    if (doc.containsKey("curiosity_drift")) lState_curiosity_drift = doc["curiosity_drift"];
    if (doc.containsKey("notes")) lState_notes = doc["notes"].as<String>();

    // 2. 融合并平滑调节原本的生理情绪参数（心智映射到生理）
    curiosity = curiosity * 0.4f + lState_curiosity_drift * 0.6f;
    comfort = comfort * 0.4f + lState_social_openness * 0.6f;
    stress = stress * 0.4f + lState_surface_instability * 0.6f;
    attachment = attachment * 0.3f + lState_social_openness * 0.7f;
    vigilance = vigilance * 0.5f + lState_surface_instability * 0.5f;

    // 3. 根据心理欲望，自动在接收时触发相关的肢体反应符号，让爱心、问号、闪电、圈叉欢快互动
    if (lState_desire == "express_warning") {
        triggerExpression(random(100) < 50 ? "warning" : "stop");
    } else if (lState_desire == "distant_resonance") {
        triggerExpression(random(100) < 50 ? "question" : "eye");
    } else if (lState_desire == "approach_observer") {
        // 主动亲近示好主人：有概率大喷特喷极其温柔美丽的液态爱心！
        triggerExpression(random(100) < 60 ? "heart" : "yes");
    } else if (lState_desire == "explore_boundaries") {
        // 探索周围边界：喷出好奇的问号 [?] 或者是观察眼
        triggerExpression(random(100) < 50 ? "question" : "eye");
    } else if (lState_desire == "conserve_energy" || lState_desire == "dormancy") {
        // 进入休眠节能：吐出安静的休止符号 [X]
        if (random(100) < 30) triggerExpression("stop");
    }

    lastLLMResponseTime = millis(); // 刷新活跃时间
    M5.Log.printf(">>> [AI] Inner consciousness parameters updated from neural core.\n");
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
