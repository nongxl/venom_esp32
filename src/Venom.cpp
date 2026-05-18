#include "Venom.h"
#include <ArduinoJson.h>
#include <math.h>
#include "Noise.h"
#include "Skills.h"

Venom::Venom() : state(VenomState::IDLE), stateTimer(0), currentFace(FRONT) {
    targetX = 0; targetY = 0; targetZ = CUBE_D;
    pupilX = 0; pupilY = 0;
    grappleProgress = 0;
    isGrappling = false;
    isLeaping = false;
    lastBlinkTime = 0;
    isBlinking = false;
    
    handLX = handLY = handLZ = 0;
    handRX = handRY = handRZ = 0;
    handProgressL = handProgressR = 1.0f; 

    targetX = 0; targetY = -CUBE_H; targetZ = 0; // 改为向顶部爬行
    skeleton.getNode(0).x = 0; skeleton.getNode(0).y = 0; 
    skeleton.getNode(0).vy = 0; // 移除强力下坠冲量
    state = VenomState::GRAPPLING;
    currentSkillName = "crawl_edge";
    activeHand = 0;
    energy = INIT_ENERGY;
    stress = INIT_STRESS;
    curiosity = INIT_CURIOSITY;
    comfort = INIT_COMFORT;
    attachment = INIT_ATTACHMENT;
    fatigue = INIT_FATIGUE;
    vigilance = INIT_VIGILANCE;
    irritation = INIT_IRRITATION;
    muscleTension = 0.5f;
    overstimulation = 0;
    recoveryProgress = 1.0f;

    // 初始化身体控制参数
    movementSpeed = 1.0f;
    bodyTension = 1.0f;
    bodyDamping = 1.0f;
    movementPause = 150.0f;
    pullForceScale = 1.0f;
    edgeAffinity = 0.5f;
    tentacleActivity = 1.0f;
    eyeFocus = 0.2f;

    breathingPhase = 0;
    breathingIntensity = 1.0f;
    lastTwitchTime = 0;
    twitchX = twitchY = 0;

    currentSkillName = "idle";
    skillTimer = 0;
    skillDuration = 5000;
    crawlCycle = VenomCrawlState::STUCK;
    crawlTimer = 0;
    
    stableTimer = millis();
    observeStartTime = 0;
    perceptionTimer = 0;
    isShy = false;
    showDebug = false;
    lastGX = lastGY = lastGZ = 0;
    lastSoundLevel = 0;
    lastLux = 0;
    startledJumpCount = 0;

    pupilSize = 0.5f;
    targetPupilSize = 0.5f;
    expressionDesire = 0;
    rhythmScore = 0;
    lastRhythmTime = 0;
    rhythmCount = 0;

    // --- 初始化渲染层新增成员 ---
    edgeParticleCount = 0;
    lastEdgeSpawnTime = 0;
    memset(edgeParticles, 0, sizeof(edgeParticles));

    // Voronoi 中心点随机初始化 (在 field 坐标系内 [0, FIELD_W/H])
    for (int i = 0; i < 6; i++) {
        voronoiCX[i] = 5.0f + (float)(i * 12 % (FIELD_W - 10));
        voronoiCY[i] = 5.0f + (float)(i * 7  % (FIELD_H - 10));
        voronoiVX[i] = ((i % 3) - 1) * 0.015f;
        voronoiVY[i] = ((i % 2) == 0 ? 1 : -1) * 0.012f;
    }
    currentFPS = 0;
    lastFrameTime = millis();
    lastLLMResponseTime = 0; // [新增] 初始化
}

void Venom::update(float gx, float gy, float gz, float soundLevel, float lux) {
    // --- 最基础的调试日志 (每秒一次) ---
    static uint32_t lastDbgTime = 0;
    /*
    if (millis() - lastDbgTime > 1000) {
        lastDbgTime = millis();
        Node& h = skeleton.getNode(0);
        M5.Log.printf(">>> [V-DBG] SKILL:%s POS:(%.1f,%.1f,%.1f) TGT:(%.1f,%.1f,%.1f)\n", 
                      currentSkillName.c_str(), h.x, h.y, h.z, targetX, targetY, targetZ);
        M5.Log.printf("    PHY -> S:%.2f E:%.2f C:%.2f A:%.2f D:%.2f\n", 
                      stress, energy, curiosity, attachment, expressionDesire);
    }
    */

    lastGX = gx; lastGY = gy; lastGZ = gz;
    lastSoundLevel = soundLevel;
    lastLux = lux;
    
    // 计算帧率
    uint32_t now = millis();
    uint32_t dt = now - lastFrameTime;
    if (dt > 0) currentFPS = currentFPS * 0.9f + (1000.0f / dt) * 0.1f;
    lastFrameTime = now;

    updatePhysiology(soundLevel, gx, gy, gz, lux);
    updateAI();
    
    Node& head = skeleton.getNode(0);
    // 能量与疲劳：运动消耗能量，静止累积疲劳
    if (state == VenomState::GRAPPLING || state == VenomState::LEAP) {
        energy = fmaxf(0, energy - 0.0005f);
        fatigue = fminf(1.0f, fatigue + 0.0003f); // 运动也会累积疲劳
    } else {
        energy = fminf(1.0f, energy + 0.0002f);
        fatigue = fminf(1.0f, fatigue + 0.0006f); // 静止时疲劳累积更快（想睡觉）
    }

    // 压力消退加快，让它更快进入闲置状态
    stress = fmaxf(0, stress * 0.999f - 0.0001f);

    float speedMult = movementSpeed * (0.8f + energy * 0.4f);

    // --- 核心移动逻辑：伸缩拉动循环 ---
    bool isMovingState = (state == VenomState::GRAPPLING || state == VenomState::HIDING || 
                          state == VenomState::OBSERVE || state == VenomState::CURIOUS_PROBE || 
                          state == VenomState::CLING || state == VenomState::TRACK_OBSERVER ||
                          state == VenomState::MIMICRY);

    if (isMovingState) {
        if (crawlCycle == VenomCrawlState::REACHING) {
            // 伸手阶段：手部向目标移动，增加一点张力准备拉扯
            // 伸手阶段：手部向目标移动，增加一点张力准备拉扯
            skeleton.setDynamicPhysics(bodyTension * 1.2f, bodyDamping);
            if (handProgressL >= 1.0f && handProgressR >= 1.0f) {
                crawlCycle = VenomCrawlState::PULLING;
                crawlTimer = millis();
            }
        } 
        else if (crawlCycle == VenomCrawlState::PULLING) {
            // 拉动阶段：利用锚点物理拉扯身体
            float tx = (activeHand == 0) ? handLX : handRX;
            float ty = (activeHand == 0) ? handLY : handRY;
            float tz = (activeHand == 0) ? handLZ : handRZ;
            
            float dx = tx - head.x, dy = ty - head.y, dz = tz - head.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);

            // 【物理拉扯】设置骨骼目标点，由物理引擎计算拉力
            // 【爆发性物理拉扯】大幅提升拉力系数，模拟生物肌肉收缩的力量感
            float pullForce = PULL_FORCE_MAGNITUDE * speedMult * pullForceScale; 
            skeleton.setTargetNode(0, tx, ty, tz, pullForce);
            
            // 拉扯时身体极度紧绷，产生爆发性弹射。提高阻尼 (0.6) 以吸收高拉力带来的能量，防止震荡。
            // 拉扯时身体极度紧绷，产生爆发性弹射。
            // 使用相对于当前技能的增强物理：张力 * 1.8，保留速度率 * 0.7 (即增加阻尼)
            // 【肌肉爆发力重构】爬行时强制进入高张力模式，不再受闲置张力拖累
            // 张力 2.8 (极硬) + 动能保留 0.85 (极滑)，确保拉力瞬间传导
            skeleton.setDynamicPhysics(2.8f, 0.85f);
            vState.lastMoveTime = millis(); // 保持计时器更新

            // 【核心重构】动态完成判定：不再死等时间，而是看尾巴有没有跟上
            bool headArrived = (dist < 6.0f);
            
            // 获取尾部节点
            const Node& tail = skeleton.getNode(MAX_NODES - 1);
            float tdx = head.x - tail.x, tdy = head.y - tail.y, tdz = head.z - tail.z;
            float distTailToHead = sqrtf(tdx*tdx + tdy*tdy + tdz*tdz);
            
            // 判定条件：头到位 且 (尾巴收缩到位 或 强行超时)
            // 尾巴到位的标准是距离【头部】不超过一定距离，代表身体已成功收缩脱离原点
            // 将判定距离大幅缩小至 15.0f，迫使系统保持极高张力直到尾巴完全被“抽”到头部位置
            bool tailFollowed = (distTailToHead < 15.0f); 
            bool forceTimeout = (millis() - vState.lastMoveTime > 1800); // 安全兜底
            
            if (headArrived && (tailFollowed || forceTimeout)) { 
                crawlCycle = VenomCrawlState::STUCK;
                crawlTimer = millis();
                // 撒手瞬间，切换至高阻尼以稳定形态
                skeleton.setDynamicPhysics(bodyTension * 1.5f, bodyDamping * 0.9f); 
            }
        }
        else if (crawlCycle == VenomCrawlState::STUCK) {
            // 撒手延迟：给物理引擎一个缓冲期来重置静摩擦
            if (millis() - crawlTimer > 250) {
                skeleton.clearTargetNode(0); 
            }
            // 就位休止：在墙面上完全放松
            skeleton.setDynamicPhysics(bodyTension * 0.7f, bodyDamping * 0.7f); 
            if (millis() - crawlTimer > (uint32_t)(movementPause / (0.5f + speedMult * 0.5f))) {
                selectNewCrawlTarget();
                crawlCycle = VenomCrawlState::REACHING;
            }
        }
    }

    // --- 跳跃逻辑：液态溅射效果 ---
    // --- [核心重构] 蛇式受惊暴走行为链 ---
    if (currentSkillName == "startled") {
        uint32_t phaseTime = millis() - phaseTimer;
        Node& head = skeleton.getNode(0);
        Node& tail = skeleton.getNode(MAX_NODES - 1);

        if (startledPhase == 0) { // 阶段 0: 用尾巴立起来 (Coiling & Anchoring)
            // [生物感增强] 实施双点锁定：锁定尾巴，同时如果中间节点靠近表面也锁定它，形成支撑腿
            Node& midNode = skeleton.getNode(MAX_NODES / 2);
            bool midNearWall = (abs(midNode.x) > CUBE_W - 8 || abs(midNode.y) > CUBE_H - 8 || abs(midNode.z) > CUBE_D - 8);

            for(int i=0; i<MAX_NODES; i++) {
                if (i == MAX_NODES - 1) skeleton.getNode(i).is_stuck = true; // 锁定尾巴
                else if (i == MAX_NODES / 2 && midNearWall) skeleton.getNode(i).is_stuck = true; // 锁定中间支点
                else skeleton.getNode(i).is_stuck = false;
            }
            
            // 目标点：从尾巴位置向空间中心延伸，拉力稍微增强以产生肌肉紧绷感
            float standLen = 50.0f;
            float dirX = -tail.x, dirY = -tail.y, dirZ = -tail.z;
            float mag = sqrtf(dirX*dirX + dirY*dirY + dirZ*dirZ + 0.01f);
            targetX = tail.x + (dirX/mag) * standLen;
            targetY = tail.y + (dirY/mag) * standLen;
            targetZ = tail.z + (dirZ/mag) * standLen;
            
            skeleton.setDynamicPhysics(3.2f, 0.90f); // 极高张力
            skeleton.setTargetNode(0, targetX, targetY, targetZ, 14.0f); 

            if (phaseTime > 550) { 
                startledPhase = 1; phaseTimer = millis(); 
                // 预选方向
                int axis = random(3);
                if (axis == 0) { leapTargetX = (head.x > 0) ? -CUBE_W-20 : CUBE_W+20; leapTargetY = random(-CUBE_H, CUBE_H); leapTargetZ = random(-CUBE_D, CUBE_D); }
                else if (axis == 1) { leapTargetY = (head.y > 0) ? -CUBE_H-20 : CUBE_H+20; leapTargetX = random(-CUBE_W, CUBE_W); leapTargetZ = random(-CUBE_D, CUBE_D); }
                else { leapTargetZ = (head.z > 0) ? -CUBE_D-20 : CUBE_D+20; leapTargetX = random(-CUBE_W, CUBE_W); leapTargetY = random(-CUBE_H, CUBE_H); }
            }
        } 
        else if (startledPhase == 1) { // 阶段 1: 头部向反方向倾斜 (Leaning Back)
            float dx = leapTargetX - head.x, dy = leapTargetY - head.y, dz = leapTargetZ - head.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz + 0.01f);
            float leanBack = 30.0f;
            targetX = head.x - (dx/dist) * leanBack;
            targetY = head.y - (dy/dist) * leanBack;
            targetZ = head.z - (dz/dist) * leanBack;
            
            skeleton.setTargetNode(0, targetX, targetY, targetZ, 18.0f); // 强化后仰拉力，拉开支架

            if (phaseTime > 400) { 
                startledPhase = 2; phaseTimer = millis(); 
                skeleton.clearTargetNode(0);
                for(int i=0; i<MAX_NODES; i++) skeleton.getNode(i).is_stuck = false; // 瞬间全释放
            }
        }
        else if (startledPhase == 2) { // 阶段 2: 突然爆射 (Strike)
            float dx = leapTargetX - head.x, dy = leapTargetY - head.y, dz = leapTargetZ - head.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz + 0.01f);
            
            // [大幅增强] 弹射初速度从 2.8 提升至 4.5 倍，确保爆发力
            float impulse = LEAP_IMPULSE_BASE * 4.5f; 
            
            head.vx = (dx/dist) * impulse;
            head.vy = (dy/dist) * impulse;
            head.vz = (dz/dist) * impulse;
            
            for(int i=1; i<MAX_NODES; i++) {
                skeleton.getNode(i).is_stuck = false;
                skeleton.getNode(i).vx = head.vx * 0.4f; // 增强跟随初速度，减少拖拽感
                skeleton.getNode(i).vy = head.vy * 0.4f;
                skeleton.getNode(i).vz = head.vz * 0.4f;
            }
            isLeaping = true;
            startledPhase = 3; phaseTimer = millis();
            vState.lastLeapTime = millis();
            vState.body_scale = 2.4f;
            skeleton.forceDetach = true;
            
            // [飞行态物理] 切换到更滑顺的物理参数，减少空气阻力感
            skeleton.setDynamicPhysics(1.2f, 0.98f); 
        }
        else if (startledPhase == 3) { // 阶段 3: 飞行中检测碰撞 (Flying & Bouncing)
            bool hitWall = false;
            bool gracePeriodOver = (phaseTime > 150);
            
            if (gracePeriodOver) {
                if (abs(head.x) > CUBE_W || abs(head.y) > CUBE_H || abs(head.z) > CUBE_D) hitWall = true;
            }
            
            // [优化] 飞行超时从 600ms 延长至 1200ms，确保能跨越屏幕撞墙
            if (hitWall || phaseTime > 1200) {
                if (startledJumpCount > 0) {
                    startledJumpCount--;
                    startledPhase = 0; phaseTimer = millis();
                    isLeaping = false;
                    skeleton.forceDetach = false;
                    // 撞击瞬间给予全方向“震颤”溅射感
                    for(int i=0; i<MAX_NODES; i++) {
                        skeleton.getNode(i).vx += random(-20, 21);
                        skeleton.getNode(i).vy += random(-20, 21);
                    }
                    vState.spike_intensity = 15.0f; // 撞墙瞬间尖刺爆表
                } else {
                    currentSkillName = "recovery";
                    state = VenomState::IDLE;
                    isLeaping = false;
                    skeleton.forceDetach = false;
                }
            }
        }
    } else {
        // --- 原有的常规跳跃/爬行逻辑 ---
        if (state == VenomState::LEAP) {
            Node& head = skeleton.getNode(0);
            if (!isLeaping) {
                float dx = targetX - head.x, dy = targetY - head.y, dz = targetZ - head.z;
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                float impulse = LEAP_IMPULSE_BASE + stress * 10.0f;
                head.vx = (dx / dist) * impulse;
                head.vy = (dy / dist) * impulse;
                head.vz = (dz / dist) * impulse;
                for(int i = 0; i < MAX_NODES; i++) {
                    skeleton.getNode(i).is_stuck = false; 
                }
                vState.lastLeapTime = millis(); 
                isLeaping = true;
            }
            float dx = targetX - head.x, dy = targetY - head.y, dz = targetZ - head.z;
            if (sqrtf(dx*dx + dy*dy + dz*dz) < 25.0f || (millis() - vState.lastLeapTime > 500)) {
                state = VenomState::IDLE;
                isLeaping = false;
            }
        } else {
            skeleton.setDynamicPhysics(bodyTension, bodyDamping);
        }
    }
    
    // --- 动态肢体回收同步 (架构级修复) ---
    if (!isMovingState) {
        handLX = handRX = head.x;
        handLY = handRY = head.y;
        handLZ = handRZ = head.z;
        handProgressL = handProgressR = 1.0f;
    } else {
        if (activeHand == 0) {
            if (handProgressL < 1.0f) handProgressL += 0.22f * speedMult;
            handRX = head.x; handRY = head.y; handRZ = head.z;
            handProgressR = 1.0f;
        } else {
            if (handProgressR < 1.0f) handProgressR += 0.22f * speedMult;
            handLX = head.x; handLY = handRY = head.y;
            handProgressL = 1.0f;
        }
    }
    
    skeleton.forceDetach = (currentSkillName == "startled" && startledPhase >= 2);
    skeleton.update(gx, gy, gz);
    updateEye();

    if (currentSkillName == "startled") {
        vState.spike_intensity = fmaxf(vState.spike_intensity, 4.0f);
    } else {
        vState.spike_intensity *= 0.90f;
    }
    vState.agitation_level = vState.agitation_level * 0.95f + (0.1f + stress * 0.4f) * 0.05f;

    if (state == VenomState::IDLE || state == VenomState::OBSERVE || state == VenomState::RECOVERY) {
        memory.territoryX = memory.territoryX * 0.999f + head.x * 0.001f;
        memory.territoryY = memory.territoryY * 0.999f + head.y * 0.001f;
        memory.territoryZ = memory.territoryZ * 0.999f + head.z * 0.001f;
    }
}

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

    // 3. IMU 动态响应 (惯性累积)
    float accMag = sqrtf(gx*gx + gy*gy + gz*gz);
    static float lastAccMag = 0;
    float deltaAcc = abs(accMag - lastAccMag);
    lastAccMag = accMag;

    if (deltaAcc < IMU_STARE_THRESHOLD) {
        // 被盯着看
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
            stress += deltaAcc * STRESS_GAIN_SCALE; 
            fatigue += deltaAcc * 0.0005f; 
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

            // 受惊判定 (在睡眠状态下屏蔽 IMU 直接惊吓，让压力值攒满再醒)
            if (deltaAcc > IMU_STARTLED_THRESHOLD && currentSkillName != "sleep") {
                if (currentSkillName != "startled" && currentSkillName != "panic" && (millis() - memory.lastPanicTime > STARTLED_COOLDOWN_MS)) {
                    setStartled(); 
                }
            }
        }
    }

    // --- 环境痛点与高级表达触发器 ---
    // 1. 惊叹信号 (!)：长时间倒置触发 (原 HELP 逻辑映射)
    static uint32_t invertedTimer = 0;
    if (gz < -8.0f && stress > 0.4f) {
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

    // 肌肉张力受压力和能量共同驱动
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

    // [新增] 决策日志输出
    M5.Log.printf(">>> [Decision] New State: %s (Duration: %dms, TotalScore: %.1f)\n", 
                  currentSkillName.c_str(), skillDuration, total);
    M5.Log.printf(">>> [Scores] Panic:%.1f Hide:%.1f Obs:%.1f Sleep:%.1f Expl:%.1f Alert:%.1f Warn:%.1f\n",
                  score_panic, score_hide, score_observe, score_sleep, score_explore, score_alert, score_warning);

    // 重置移动循环相关变量
    crawlCycle = VenomCrawlState::STUCK;
    crawlTimer = now;
}

void Venom::setStartled() {
    if (currentSkillName == "startled") {
        startledJumpCount += random(2, 5); 
        return; 
    }
    
    currentSkillName = "startled";
    state = VenomState::STARTLED; 
    startledPhase = 0; // 初始进入立起阶段
    phaseTimer = millis();
    
    skillTimer = millis();
    memory.lastPanicTime = skillTimer;
    skillDuration = 8000;     
    stress = 1.0f;
    startledJumpCount = random(6, 10); 
    
    bodyTension = 2.8f; 
    bodyDamping = 0.95f;
    
    // 强制海胆形态尖刺
    vState.spike_intensity = STARTLED_SPIKE_INTENSITY;
    vState.edge_activity = 1.0f;
    
    // 初始清空所有目标
    skeleton.clearTargetNode(0);
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

void Venom::selectNewCrawlTarget() {
    vState.lastMoveTime = millis(); // 记录起始时间，用于耐性牵引
    int wall = random(6);
    // 【抗重力动力系统】
    // 根据当前加速度传感器的方向，动态调整爬行目标的倾向性，使其更喜欢“往高处爬”
    float gx = lastGX, gy = lastGY; 
    
    // 如果重力很大，则倾向于选择重力反方向的面
    if (abs(gx) > 5.0f || abs(gy) > 5.0f) {
        if (random(100) < 70) { // 70% 概率尝试向上爬
            if (abs(gx) > abs(gy)) wall = (gx > 0) ? LEFT : RIGHT;
            else wall = (gy > 0) ? TOP : BOTTOM;
        }
    }
    
    // 始终有一定概率尝试贴近屏幕 (FRONT)
    if (random(100) < 40) wall = FRONT;

    // 根据技能覆盖目标
    if (currentSkillName == "observe_user" || currentSkillName == "track_observer" || currentSkillName == "curious_probe" || currentSkillName == "trust_observe") {
        // 增加观察范围，使其在屏幕上移动更明显
        targetX = random(-CUBE_W * 0.7f, CUBE_W * 0.7f);
        targetY = random(-CUBE_H * 0.7f, CUBE_H * 0.7f);
        targetZ = (currentSkillName == "curious_probe") ? CUBE_D * 0.6f : CUBE_D; 
    } else if (currentSkillName == "warning") {
        // 警告：扑向屏幕中心并略微偏移
        targetX = random(-5, 6);
        targetY = random(-5, 6);
        targetZ = CUBE_D + 10.0f; // 冲出屏幕感
    } else if (currentSkillName == "hesitation") {
        // 犹豫：在靠近和远离玻璃之间反复
        static bool push = true;
        targetX = random(-30, 31);
        targetY = random(-30, 31);
        targetZ = push ? CUBE_D : CUBE_D * 0.4f;
        push = !push;
    } else if (currentSkillName == "cling" || currentSkillName == "trust_observe") {
        // 依附或信任：贴近屏幕中央并摊开
        targetX = random(-15, 16);
        targetY = random(-15, 16);
        targetZ = CUBE_D;
    } else if (currentSkillName == "mimicry") {
        // 模仿：在屏幕上做大幅度往复运动
        static float phase = 0; phase += 1.5f;
        targetX = sinf(phase) * CUBE_W * 0.6f;
        targetY = cosf(phase * 0.5f) * CUBE_H * 0.4f;
        targetZ = CUBE_D * 0.8f;
    } else if (currentSkillName == "hiding" || currentSkillName == "camouflage" || isShy) {
        // 躲避或伪装：选择极边缘或背面
        int hideType = random(100);
        float nextX = 0, nextY = 0, nextZ = 0;

        if (hideType < 40) { 
            // 【新增：边缘窥视】躲在屏幕最上/最下边缘，但紧贴玻璃，实现“只露眼睛”的探头感
            wall = (random(100) < 50) ? TOP : BOTTOM;
            nextX = random(-CUBE_W * 0.6f, CUBE_W * 0.6f);
            nextY = (wall == TOP) ? -CUBE_H : CUBE_H;
            nextZ = CUBE_D + 2.0f; // 紧贴并压在玻璃边缘
        } else if (hideType < 75 || currentSkillName == "camouflage") {
            wall = BACK;
            nextX = random(-CUBE_W, CUBE_W); nextY = random(-CUBE_H, CUBE_H); nextZ = -CUBE_D;
        } else {
            wall = (random(100) < 50) ? LEFT : RIGHT;
            nextX = (wall == LEFT) ? -CUBE_W : CUBE_W; nextY = random(-CUBE_H, CUBE_H); nextZ = random(-CUBE_D, CUBE_D);
        }
        
        targetX = nextX; targetY = nextY; targetZ = nextZ;
    } else if (currentSkillName == "explore") {
        // 探索：倾向于移动到从未去过的边缘或角落，或者回到领地
        if (random(100) < 30) {
            targetX = memory.territoryX + random(-20, 21);
            targetY = memory.territoryY + random(-20, 21);
            targetZ = memory.territoryZ + random(-5, 6);
        } else {
            wall = random(6);
            float nextX = 0, nextY = 0, nextZ = 0;
            if (wall == LEFT) { nextX = -CUBE_W; nextY = random(-CUBE_H, CUBE_H); nextZ = random(-CUBE_D, CUBE_D); }
            else if (wall == RIGHT) { nextX = CUBE_W; nextY = random(-CUBE_H, CUBE_H); nextZ = random(-CUBE_D, CUBE_D); }
            else if (wall == TOP) { nextX = random(-CUBE_W, CUBE_W); nextY = -CUBE_H; nextZ = random(-CUBE_D, CUBE_D); }
            else if (wall == BOTTOM) { nextX = random(-CUBE_W, CUBE_W); nextY = CUBE_H; nextZ = random(-CUBE_D, CUBE_D); }
            else if (wall == BACK) { nextX = random(-CUBE_W, CUBE_W); nextY = random(-CUBE_H, CUBE_H); nextZ = -CUBE_D; }
            else { nextX = random(-CUBE_W, CUBE_W); nextY = random(-CUBE_H, CUBE_H); nextZ = CUBE_D; } 
            targetX = nextX; targetY = nextY; targetZ = nextZ;
        }
    } else {
        float nextX = 0, nextY = 0, nextZ = 0;
        if (wall == LEFT) { nextX = -CUBE_W; nextY = random(-CUBE_H, CUBE_H); nextZ = random(-CUBE_D, CUBE_D); }
        else if (wall == RIGHT) { nextX = CUBE_W; nextY = random(-CUBE_H, CUBE_H); nextZ = random(-CUBE_D, CUBE_D); }
        else if (wall == TOP) { nextX = random(-CUBE_W, CUBE_W); nextY = -CUBE_H; nextZ = random(-CUBE_D, CUBE_D); }
        else if (wall == BOTTOM) { nextX = random(-CUBE_W, CUBE_W); nextY = CUBE_H; nextZ = random(-CUBE_D, CUBE_D); }
        else { nextX = random(-CUBE_W, CUBE_W); nextY = random(-CUBE_H, CUBE_H); nextZ = CUBE_D; } 
        targetX = nextX; targetY = nextY; targetZ = nextZ;
    }

    // 【架构级修复】活动手重置进度，不活跃手标记为“已回收”
    if (random(100) < 50) {
        handLX = targetX; handLY = targetY; handLZ = targetZ;
        handProgressL = 0;
        activeHand = 0;
        handProgressR = 1.0f; // 右手立即标记为已收回
    } else {
        handRX = targetX; handRY = targetY; handRZ = targetZ;
        handProgressR = 0;
        activeHand = 1;
        handProgressL = 1.0f; // 左手立即标记为已收回
    }
}

void Venom::updateEye() {
    if (currentSkillName == "sleep") {
        pupilX = 0.0f;
        pupilY = 0.0f;
        isBlinking = false;
        return;
    }

    uint32_t now = millis();
    
    // 好奇/观察时的眼部扫描逻辑
    bool isScanning = (currentSkillName == "observe_user" || currentSkillName == "curious_probe" || currentSkillName == "trust_observe");
    int scanChance = isScanning ? 12 : 5; // 增加扫描频率

    if (random(100) < scanChance) {
        if (isScanning) {
            // 主动扫描：在视野内大幅度跳跃
            pupilX = (float)random(-100, 101) * 0.01f * 2.0f;
            pupilY = (float)random(-100, 101) * 0.01f * 1.5f;
        } else {
            pupilX = (random(-8, 9)) / 5.0f;
            pupilY = (random(-8, 9)) / 5.0f;
        }
    }
    if (!isBlinking && now - lastBlinkTime > 3000 + random(5000)) {
        isBlinking = true;
        lastBlinkTime = now;
    }
    if (isBlinking && now - lastBlinkTime > 150) {
        isBlinking = false;
    }
}

void Venom::projectToFace(const Node& n, Face face, float& outX, float& outY, float& outZ, float ax, float ay) {
    projectPoint(n.x, n.y, n.z, face, outX, outY, outZ, ax, ay);
}

void Venom::projectPoint(float x, float y, float z, Face face, float& outX, float& outY, float& outZ, float ax, float ay) {
    float u = 0, v = 0, depth = 0;
    const float baseScale = (float)SCREEN_W / (CUBE_W * 2.5f); 
    const float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;

    switch (face) {
        case FRONT:  u = x;  v = y;  depth = CUBE_D - z; break;
        case BACK:   u = -x; v = y;  depth = z + CUBE_D; break;
        case LEFT:   u = z;  v = y;  depth = x + CUBE_W; break;
        case RIGHT:  u = -z; v = y;  depth = CUBE_W - x; break;
        case TOP:    u = x;  v = -z; depth = CUBE_D - y; break;
        case BOTTOM: u = x;  v = z;  depth = y + CUBE_H; break;
    }
    
    // 【核心增强】显著提升透视位移系数与基础深度感
    const float tiltFactor = 4.5f; 
    u += depth * (ay * tiltFactor); 
    v -= depth * (ax * tiltFactor); 

    // 增强近大远小效果：将透视基准从 250 降至 180
    float perspective = 180.0f / (180.0f + depth); 
    outX = cx + u * baseScale * perspective;
    outY = cy + v * baseScale * perspective;
    outZ = depth;
}

void Venom::calculateField(Face face, float ax, float ay) {
    memset(field, 0, sizeof(field));
    float gMag = sqrtf(ax*ax + ay*ay);
    bool isStartled = (currentSkillName == "startled");
    
    auto drawBlob = [&](float mx, float my, float mz, float nodeRadius, float vx, float vy, float vz) {
        float px, py, pz;
        projectPoint(mx, my, mz, face, px, py, pz, ax, ay);
        if (pz > 150.0f) return;
        
        float perspective = 200.0f / (200.0f + pz);
        
        // 【物理对齐修复】获取投影后的 2D 速度矢量
        float px2, py2, pz2;
        projectPoint(mx + vx * 0.1f, my + vy * 0.1f, mz + vz * 0.1f, face, px2, py2, pz2, ax, ay);
        float v2dx = px2 - px;
        float v2dy = py2 - py;
        float vMag = sqrtf(v2dx*v2dx + v2dy*v2dy) * 10.0f;
        
        // 计算拉伸长度 (Trail Length)
        float trailScale = isStartled ? fmaxf(4.0f, fminf(15.0f, 1.0f + 0.6f * vMag)) : fminf(3.0f, 1.0f + 0.1f * vMag);
        // 动态瘦身
        // 动态瘦身：受惊时稍微变细以增加速度感，但保持基础厚度
        float thinning = 1.0f / sqrtf(1.0f + vMag * (isStartled ? 0.25f : 0.12f));
        if (isStartled) thinning = fmaxf(0.75f, thinning); // 确保即便高速下也不要细过 0.75
        
        // 引入呼吸起伏 (Breathing effect)
        float r = (nodeRadius * VISUAL_RADIUS_MULT + VISUAL_RADIUS_OFFSET) * vState.body_scale * thinning * breathingIntensity * powf(perspective, 0.8f) / FIELD_SCALE;

        float nx1 = px / FIELD_SCALE, ny1 = py / FIELD_SCALE;
        float nx0 = (px - v2dx * trailScale * 0.5f) / FIELD_SCALE;
        float ny0 = (py - v2dy * trailScale * 0.5f) / FIELD_SCALE;

        // 计算包围盒：包含整个线段及其半径
        int rx_min = (int)fminf(nx0, nx1) - (int)r - 4;
        int rx_max = (int)fmaxf(nx0, nx1) + (int)r + 4;
        int ry_min = (int)fminf(ny0, ny1) - (int)r - 4;
        int ry_max = (int)fmaxf(ny0, ny1) + (int)r + 4;

        float line_dx = nx1 - nx0;
        float line_dy = ny1 - ny0;
        float l2 = line_dx * line_dx + line_dy * line_dy;

        for (int y = fmaxf(0, ry_min); y < fminf(FIELD_H, ry_max); y++) {
            for (int x = fmaxf(0, rx_min); x < fminf(FIELD_W, rx_max); x++) {
                // 计算点到线段 (Capsule) 的距离
                float dist_px;
                if (l2 < 0.001f) {
                    float dx = x - nx1, dy = y - ny1;
                    dist_px = sqrtf(dx*dx + dy*dy);
                } else {
                    float t = fmaxf(0, fminf(1, ((x - nx0) * line_dx + (y - ny0) * line_dy) / l2));
                    float proj_x = nx0 + t * line_dx;
                    float proj_y = ny0 + t * line_dy;
                    float dx = x - proj_x, dy = y - proj_y;
                    dist_px = sqrtf(dx*dx + dy*dy);
                }

                float dist_ratio = dist_px / r;
                if (dist_ratio < 1.0f) {
                    float inv_ratio = 1.0f - dist_ratio * dist_ratio;
                    float val = inv_ratio * inv_ratio * 210.0f;
                    field[y * FIELD_W + x] = fminf(255, field[y * FIELD_W + x] + (int)val);
                }
            }
        }
    };

    // --- 1. 核心节点与插值路径 (主体场) ---
    for (int i = 0; i < MAX_NODES; i++) {
        const Node& curr = skeleton.getNode(i);
        drawBlob(curr.x, curr.y, curr.z, curr.radius, curr.vx, curr.vy, curr.vz);

        // 始终开启插值，确保在任何状态下身体都不会断裂成散点
        if (i > 0) {
            const Node& prev = skeleton.getNode(i-1);
            float dx = curr.x - prev.x, dy = curr.y - prev.y, dz = curr.z - prev.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            
            // 暴走时大幅增加插值密度，确保身体是一根连贯的针，而不是断开的球
            float stepSize = (isStartled) ? 2.5f : 4.5f;
            int steps = (int)(dist / stepSize); 
            
            for (int s = 1; s < steps; s++) {
                float t = (float)s / steps;
                float mx = prev.x + dx * t, my = prev.y + dy * t, mz = prev.z + dz * t;
                float interpolatedRadius = prev.radius * (1.0f - t) + curr.radius * t;
                float interpolatedVx = prev.vx * (1.0f - t) + curr.vx * t;
                float interpolatedVy = prev.vy * (1.0f - t) + curr.vy * t;
                float interpolatedVz = prev.vz * (1.0f - t) + curr.vz * t;
                drawBlob(mx, my, mz, interpolatedRadius, interpolatedVx, interpolatedVy, interpolatedVz);
            }
        }
    }
    // --- 7. 触手与爪部系统 (Cubic Bezier Arms) ---
    float gravMag = sqrtf(ax*ax + ay*ay);
    auto bezier = [](float p0, float p1, float p2, float p3, float t) -> float {
        float u = 1.0f - t;
        return u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3;
    };

    const Node& head = skeleton.getNode(0);
    float hpx, hpy, hpz;
    projectToFace(head, face, hpx, hpy, hpz, ax, ay);
    float hnx = hpx / FIELD_SCALE, hny = hpy / FIELD_SCALE;

    for (int hand = 0; hand < 2; hand++) {
        float prog = (hand == 0) ? handProgressL : handProgressR;
        if (prog >= 1.0f && hand != activeHand) continue;

        float hx = (hand == 0) ? handLX : handRX;
        float hy = (hand == 0) ? handLY : handRY;
        float hz = (hand == 0) ? handLZ : handRZ;

        float tx, ty, tz;
        projectPoint(hx, hy, hz, face, tx, ty, tz, ax, ay);
        float tnx = tx/FIELD_SCALE, tny = ty/FIELD_SCALE;
        float endX = hnx + (tnx-hnx)*prog, endY = hny + (tny-hny)*prog;

        // 计算重力下垂
        float midX = (hnx+endX)*0.5f, midY = (hny+endY)*0.5f;
        float sagX = ax * gravMag * 1.2f, sagY = ay * gravMag * 1.2f;
        float sagScale = 1.0f + (1.0f-bodyTension) * 2.5f;
        float cp1x = hnx*0.6f + midX*0.4f + sagX*sagScale;
        float cp1y = hny*0.6f + midY*0.4f + sagY*sagScale;
        float cp2x = endX*0.6f + midX*0.4f + sagX*sagScale;
        float cp2y = endY*0.6f + midY*0.4f + sagY*sagScale;

        float rootR = 2.0f + vState.surface_tension * 0.5f;
        float tipR  = 0.8f;
        int BEZ_STEPS = 14;
        float pbx = hnx, pby = hny;
        for (int s = 1; s <= BEZ_STEPS; s++) {
            float bt = (float)s / BEZ_STEPS;
            float bx = bezier(hnx, cp1x, cp2x, endX, bt);
            float by = bezier(hny, cp1y, cp2y, endY, bt);
            float r0 = rootR + (tipR-rootR)*(bt-1.0f/BEZ_STEPS);
            float r1 = rootR + (tipR-rootR)*bt;
            
            // 简单的场线段绘制
            float dist = sqrtf((bx-pbx)*(bx-pbx)+(by-pby)*(by-pby));
            int steps = (int)(dist/0.6f)+1;
            for(int k=0; k<=steps; k++) {
                float kt = (float)k/steps;
                float fx = pbx+(bx-pbx)*kt, fy = pby+(by-pby)*kt;
                float fr = r0+(r1-r0)*kt;
                int ir = (int)fmaxf(1, fr);
                for(int dy=-ir; dy<=ir; dy++) {
                    for(int dx=-ir; dx<=ir; dx++) {
                        if(dx*dx+dy*dy > fr*fr) continue;
                        int gx = (int)fx+dx, gy = (int)fy+dy;
                        if(gx>=0&&gx<FIELD_W&&gy>=0&&gy<FIELD_H) {
                            field[gy*FIELD_W+gx] = fminf(255, field[gy*FIELD_W+gx]+(int)(190*(1.0f-sqrtf(dx*dx+dy*dy)/(fr+0.1f))));
                        }
                    }
                }
            }
            pbx = bx; pby = by;
        }

        if (prog > 0.4f) {
            // 吸盘逻辑 (减小掌心半径，使其不那么明显)
            float padR = 1.2f + curiosity * 0.8f;
            for(int dy=-(int)padR; dy<=(int)padR; dy++) {
                for(int dx=-(int)padR; dx<=(int)padR; dx++) {
                    if(dx*dx+dy*dy > padR*padR) continue;
                    int gx=(int)endX+dx, gy=(int)endY+dy;
                    if(gx>=0&&gx<FIELD_W&&gy>=0&&gy<FIELD_H) {
                        field[gy*FIELD_W+gx] = fminf(255, field[gy*FIELD_W+gx]+(int)(195*(1.0f-sqrtf(dx*dx+dy*dy)/(padR+0.1f))));
                    }
                }
            }
            // 绘制生物爪指 (新爪形系统)
            drawTendrils(endX, endY, prog, hand, ax, ay);
        }
    }

    // --- 7.5 尾部装饰胡须 (Decorative whiskers) ---
    for (int i = 2; i < MAX_NODES; i += 2) {
        const Node& n = skeleton.getNode(i);
        float px, py, pz;
        projectToFace(n, face, px, py, pz, ax, ay);
        if (pz > 120.0f) continue;
        float nx = px/FIELD_SCALE, ny = py/FIELD_SCALE;
        float tailDim = 1.0f - (float)i / (MAX_NODES * 1.2f);
        if (tailDim < 0.1f) continue;

        float baseAng = millis()*0.003f + i*0.9f;
        float gravAng = atan2f(ay, ax) + 3.14159f/2.0f;
        baseAng = baseAng*(1.0f-gravMag*0.5f) + gravAng*(gravMag*0.5f);
        float wLen = (8.0f + sinf(millis()*0.007f+i)*5.0f) * tailDim * (0.7f + vState.edge_activity*0.5f);

        float wx = nx, wy = ny;
        float wvx = cosf(baseAng), wvy = sinf(baseAng);
        int wsteps = (int)(wLen / 0.8f);
        for (int s = 0; s < wsteps; s++) {
            float st = (float)s / wsteps;
            wvx += ax * 0.05f; wvy += ay * 0.05f;
            float vm = sqrtf(wvx*wvx+wvy*wvy);
            if(vm>0.01f) { wvx/=vm; wvy/=vm; }
            wx += wvx*0.8f; wy += wvy*0.8f;
            int gx=(int)wx, gy=(int)wy;
            if(gx>=0&&gx<FIELD_W&&gy>=0&&gy<FIELD_H) {
                field[gy*FIELD_W+gx] = fminf(255, field[gy*FIELD_W+gx]+(int)(115*(1.0f-st)*tailDim));
            }
        }
    }
    // --- 7.5 边缘尖刺与纤维场贡献 ---
    drawEdgeActivity(ax, ay);

    // --- 8. 液态符号表达层渲染 (置于顶层，具有最强的生命感) ---
    int symCount = symbolMgr.getPointCount();
    for (int i = 0; i < symCount; i++) {
        const auto& sp = symbolMgr.getPoint(i);
        float nx = sp.x / FIELD_SCALE, ny = sp.y / FIELD_SCALE;
        float r = sp.radius / FIELD_SCALE;
        
        // 符号点同样应用变形，纠正方向
        float gx = 0, gy = 0, gMag = sqrtf(ax*ax + ay*ay);
        if (gMag > 0.1f) { gx = ax / gMag; gy = ay / gMag; }
        float stretch = fminf(2.6f, 1.0f + 0.5f * gMag);
        int r_int = (int)(r * stretch) + 8;

        for (int y = fmaxf(0, ny-r_int); y < fminf(FIELD_H, ny+r_int); y++) {
            for (int x = fmaxf(0, nx-r_int); x < fminf(FIELD_W, nx+r_int); x++) {
                float dx = x - nx, dy = y - ny;
                float dist_px = sqrtf(dx*dx + dy*dy);
                float dotG = (dx * gx + dy * gy) / (dist_px + 0.001f);
                
                float scaledDist = dist_px;
                if (dotG > 0) scaledDist /= (1.0f + dotG * (stretch - 1.0f));
                else scaledDist *= (1.0f - dotG * 0.15f);

                float dist_ratio = scaledDist / r;
                if (dist_ratio < 1.0f) {
                    float val = (0.5f + 0.5f * cosf(dist_ratio * 3.14159f)) * 200.0f * sp.life * (1.0f + (dx*gx + dy*gy)/r * 0.5f);
                    field[y * FIELD_W + x] = fminf(255, field[y * FIELD_W + x] + (int)val);
                }
            }
        }
    }
}

// ============================================================
// drawTendrils - Biological tendrils with cubic Bezier arms
//   Arms: thick-to-thin cubic Bezier curves with gravity sag
//   Tail whiskers: curved droop tendrils
// ============================================================
void Venom::drawTendrils(float endX, float endY, float prog, int handIdx, float ax, float ay) {
    // 【生物级爪形系统】实现 5 根非对称、随机长度的纤细手指
    const int numFingers = 5;
    bool isGripping = (prog > 0.9f);
    
    // 爪部扇面分布：集中在 120 度左右的“爪状”扇区
    float baseAngle = (handIdx == 0 ? 3.14f + 0.8f : -0.8f) + sinf(millis() * 0.004f) * 0.2f;
    
    for (int f = 0; f < numFingers; f++) {
        // 非均匀角度分布
        float relativeAng = (f - 2) * 0.4f + sinf(f * 1.5f + millis()*0.001f) * 0.1f; 
        float fAng = baseAngle + relativeAng;

        // 随机且动态的长度 (缩减 1/3)
        float fLenBase = isGripping ? 7.0f : (4.0f + sinf(millis() * 0.008f + f) * 2.0f);
        float fLenVar = (float)((millis() + f*100) % 27) * 0.1f; 
        float fLen = fmaxf(4.0f, (fLenBase + fLenVar) * (0.7f + vState.edge_activity * 0.5f));

        for (float fl = 0; fl < fLen; fl += 0.8f) {
            float t = fl / fLen;
            float fx = endX + cosf(fAng) * fl;
            float fy = endY + sinf(fAng) * fl;
            
            // 极致纤细处理
            float fr = fmaxf(0.2f, 1.0f * (1.0f - t * 0.9f));
            int ir = (int)fmaxf(1, fr);
            
            for (int dy = -ir; dy <= ir; dy++) {
                for (int dx = -ir; dx <= ir; dx++) {
                    int ifx = (int)fx + dx, ify = (int)fy + dy;
                    if (ifx >= 0 && ifx < FIELD_W && ify >= 0 && ify < FIELD_H) {
                        float dist = sqrtf(dx * dx + dy * dy);
                        if (dist <= fr) {
                            float opacity = (1.0f - dist / (fr + 0.1f)) * (1.0f - t * 0.5f);
                            field[ify * FIELD_W + ifx] = fminf(255, field[ify * FIELD_W + ifx] + (int)(180 * opacity));
                        }
                    }
                }
            }
        }
    }
}


void Venom::draw(M5Canvas* canvas, float ax, float ay, float az) {
    float cax = fmaxf(-0.85f, fminf(0.85f, ax * 0.45f));
    float cay = fmaxf(-0.85f, fminf(0.85f, ay * 0.45f));

    currentFace = FRONT;

    calculateField(currentFace, cax, cay);
    float px = cay * 0.1f, py = -cax * 0.1f;
    drawContainer(canvas, cax, cay);
    drawBackground(canvas, px * 0.5f, py * 0.5f);
    drawBody(canvas, px, py, cax, cay);
    drawGloss(canvas, px * 1.1f, py * 1.1f, cax, cay);
    drawEye(canvas, px * 1.1f, py * 1.1f, cax, cay);

    if (showDebug) drawDebug(canvas);
}


void Venom::drawContainer(M5Canvas* canvas, float ax, float ay) {
    static const float inner_v[4][3] = {
        {-CUBE_W, -CUBE_H, -CUBE_D}, {CUBE_W, -CUBE_H, -CUBE_D},
        {CUBE_W, CUBE_H, -CUBE_D}, {-CUBE_W, CUBE_H, -CUBE_D}
    };
    static const float screen_v[4][2] = {
        {0, 0}, {SCREEN_W, 0}, {SCREEN_W, SCREEN_H}, {0, SCREEN_H}
    };
    float px[4], py[4], pz[4];
    for (int i = 0; i < 4; i++) {
        projectPoint(inner_v[i][0], inner_v[i][1], inner_v[i][2], currentFace, px[i], py[i], pz[i], ax, ay);
    }
    
    // --- 高亮所有与毒液有接触的 3D 面 ---
    bool touched[6] = {false};
    float threshold = 12.0f; // 接触判定阈值
    for (int i = 0; i < MAX_NODES; i++) {
        const Node& n = skeleton.getNode(i);
        if (abs(n.x - (-CUBE_W)) < threshold) touched[LEFT] = true;
        if (abs(n.x - CUBE_W) < threshold) touched[RIGHT] = true;
        if (abs(n.y - (-CUBE_H)) < threshold) touched[TOP] = true;
        if (abs(n.y - CUBE_H) < threshold) touched[BOTTOM] = true;
        if (abs(n.z - (-CUBE_D)) < threshold) touched[BACK] = true;
        if (abs(n.z - CUBE_D) < threshold) touched[FRONT] = true;
    }

    uint16_t highlightColor = 0x0115;
    if (touched[FRONT]) {
        canvas->drawRect(0, 0, SCREEN_W, SCREEN_H, highlightColor);
        canvas->drawRect(1, 1, SCREEN_W-2, SCREEN_H-2, highlightColor);
    }
    if (touched[BACK]) {
        canvas->fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], highlightColor);
        canvas->fillTriangle(px[0], py[0], px[2], py[2], px[3], py[3], highlightColor);
    }
    // 侧面高亮
    for (int f = 0; f < 4; f++) {
        Face sideFace;
        if (f == 0) sideFace = TOP;
        else if (f == 1) sideFace = RIGHT;
        else if (f == 2) sideFace = BOTTOM;
        else sideFace = LEFT;

        if (touched[sideFace]) {
            int v1 = f;
            int v2 = (f + 1) % 4;
            canvas->fillTriangle(px[v1], py[v1], px[v2], py[v2], screen_v[v2][0], screen_v[v2][1], highlightColor);
            canvas->fillTriangle(px[v1], py[v1], screen_v[v2][0], screen_v[v2][1], screen_v[v1][0], screen_v[v1][1], highlightColor);
        }
    }

    // 绘制容器线框
    for (int i = 0; i < 4; i++) {
        canvas->drawLine(px[i], py[i], px[(i+1)%4], py[(i+1)%4], 0x5AEB); // 使用更亮一点的深青色作为线框
    }
    for (int i = 0; i < 4; i++) {
        canvas->drawLine(screen_v[i][0], screen_v[i][1], px[i], py[i], 0x5AEB);
    }
}

void Venom::drawBackground(M5Canvas* canvas, float px, float py) {
    uint32_t t = millis() / 80;
    for (int y = 0; y < FIELD_H; y++) {
        for (int x = 0; x < FIELD_W; x++) {
            int val = field[y * FIELD_W + x];
            if (val > 30) {
                float jx = (float)((x * 17 + y * 31 + t) % 7 - 3) * 0.15f;
                float jy = (float)((x * 23 + y * 13 + t) % 7 - 3) * 0.15f;
                // 稍微增加阴影半径确保融合
                float r = (FIELD_SCALE * 0.6f) + (val / 255.0f) * 2.2f;
                canvas->fillCircle((int)(x * FIELD_SCALE + px + 4 + jx), 
                                   (int)(y * FIELD_SCALE + py + 4 + jy), 
                                   (int)r, COLOR_VENOM_SHADOW);
            }
        }
    }
}

void Venom::drawBody(M5Canvas* canvas, float px, float py, float ax, float ay) {
    uint32_t t = millis() / 60;
    static const int THRESHOLD = 45; 

    for (int fy = 0; fy < FIELD_H; fy++) {
        int seg_start = -1;
        for (int fx = 0; fx < FIELD_W; fx++) {
            bool active = (field[fy * FIELD_W + fx] > THRESHOLD);
            
            if (active && seg_start < 0) {
                seg_start = fx;
            } else if (!active && seg_start >= 0) {
                // 绘制当前识别到的连续线段
                int x_min = seg_start, x_max = fx - 1;
                int sy = (int)(fy * FIELD_SCALE + py);
                for (int dy = 0; dy < FIELD_SCALE; dy++) {
                    int screen_y = sy + dy;
                    if (screen_y < 0 || screen_y >= SCREEN_H) continue;
                    int sx_start = (int)(x_min * FIELD_SCALE + px);
                    int sx_end   = (int)(x_max * FIELD_SCALE + px) + FIELD_SCALE - 1;
                    canvas->drawFastHLine(sx_start, screen_y, sx_end - sx_start + 1, COLOR_VENOM_BODY);
                }
                seg_start = -1;
            }
        }
        // 处理行尾未闭合的线段
        if (seg_start >= 0) {
            int x_min = seg_start, x_max = FIELD_W - 1;
            int sy = (int)(fy * FIELD_SCALE + py);
            for (int dy = 0; dy < FIELD_SCALE; dy++) {
                int screen_y = sy + dy;
                if (screen_y < 0 || screen_y >= SCREEN_H) continue;
                int sx_start = (int)(x_min * FIELD_SCALE + px);
                int sx_end   = (int)(x_max * FIELD_SCALE + px) + FIELD_SCALE - 1;
                canvas->drawFastHLine(sx_start, screen_y, sx_end - sx_start + 1, COLOR_VENOM_BODY);
            }
        }
    }

    // --- 表面次生光泽与细节 (TA 层) ---
    float lightX = -0.7f, lightY = -0.7f;
    for (int fy = 1; fy < FIELD_H - 1; fy++) {
        for (int fx = 1; fx < FIELD_W - 1; fx++) {
            int val = field[fy * FIELD_W + fx];
            if (val < 65) continue;
            float nx = (float)(field[fy * FIELD_W + fx + 1] - field[fy * FIELD_W + fx - 1]);
            float ny = (float)(field[(fy + 1) * FIELD_W + fx] - field[(fy - 1) * FIELD_W + fx]);
            float mag = sqrtf(nx*nx + ny*ny + 0.01f);
            nx /= mag; ny /= mag;
            float dot = -(nx * lightX + ny * lightY);
            int sx = (int)(fx * FIELD_SCALE + px) + 1;
            int sy = (int)(fy * FIELD_SCALE + py) + 1;
            if (dot > 0.85f && val > 160) {
                canvas->drawPixel(sx, sy, COLOR_VENOM_GLOSS);
            } else if (dot > 0.70f && (fx+fy+t/8)%2==0) {
                canvas->drawPixel(sx, sy, 0xBDF7);
            } else if (dot < -0.80f && (fx+fy)%2==0) {
                canvas->drawPixel(sx, sy, 0x4208);
            }
        }
    }
}

void Venom::drawEdgeActivity(float ax, float ay) {
    uint32_t now = millis();
    float gMag = sqrtf(ax*ax + ay*ay);
    int aliveCount = 0;
    
    // 1. 处理存量粒子并渲染到场中
    for (int i = 0; i < edgeParticleCount; i++) {
        EdgeParticle& ep = edgeParticles[i];
        if (ep.life <= 0) continue;
        ep.life -= (ep.type == 1) ? 0.024f : 0.034f; 
        if (ep.life <= 0) continue;

        if (ep.type == 0 || ep.type == 2) {
            // 尖刺/纤维：在 field 中生成连贯的能量点
            float spLen = ep.len * ep.life * (NOISE_SPIKE_SCALE / 80.0f);
            for (float l = 0; l < spLen; l += 0.5f) {
                float t = l / fmaxf(0.1f, spLen);
                float fx = ep.x + ep.nx * l;
                float fy = ep.y + ep.ny * l;
                if (ep.type == 2) { 
                    float wob = sinf(l * 2.0f + now * 0.012f) * 0.4f;
                    fx += ep.ny * wob; fy -= ep.nx * wob;
                }
                int ix = (int)fx, iy = (int)fy;
                if (ix >= 0 && ix < FIELD_W && iy >= 0 && iy < FIELD_H) {
                    int val = (int)(220.0f * ep.life * (1.0f - t * 0.65f));
                    field[iy * FIELD_W + ix] = fminf(255, field[iy * FIELD_W + ix] + val);
                }
            }
        } else if (ep.type == 1) {
            // 飞溅的水滴：同样写入 field，产生靠近身体时的粘连效果
            ep.vx += ax * 0.015f; ep.vy += ay * 0.015f;
            ep.x += ep.vx; ep.y += ep.vy;
            int ix = (int)ep.x, iy = (int)ep.y;
            if (ix >= 0 && ix < FIELD_W && iy >= 0 && iy < FIELD_H) {
                int val = (int)(165.0f * ep.life);
                field[iy * FIELD_W + ix] = fminf(255, field[iy * FIELD_W + ix] + val);
            }
        }
        edgeParticles[aliveCount++] = ep;
    }
    edgeParticleCount = aliveCount;

    // 2. 生成新粒子 (从 field 边缘激发出尖刺)
    if (now - lastEdgeSpawnTime > (uint32_t)(100.0f / (0.1f + vState.edge_activity)) && edgeParticleCount < 25) {
        lastEdgeSpawnTime = now;
        int cx = -1, cy = -1;
        for (int a = 0; a < 20; a++) {
            int tx = random(1, FIELD_W - 1), ty = random(1, FIELD_H - 1);
            int v = field[ty * FIELD_W + tx];
            if (v >= 40 && v <= 80) { cx = tx; cy = ty; break; }
        }
        if (cx >= 0) {
            float gnx = -(float)(field[cy * FIELD_W + (cx + 1)] - field[cy * FIELD_W + (cx - 1)]);
            float gny = -(float)(field[(cy + 1) * FIELD_W + cx] - field[(cy - 1) * FIELD_W + cx]);
            float m = sqrtf(gnx * gnx + gny * gny);
            if (m > 0.1f) {
                EdgeParticle& ep = edgeParticles[edgeParticleCount++];
                ep.x = cx; ep.y = cy; ep.nx = gnx / m; ep.ny = gny / m; ep.vx = 0; ep.vy = 0; ep.life = 1.0f;
                float roll = (float)random(100) / 100.0f;
                if (roll < 0.45f + vState.spike_intensity * 0.35f) {
                    ep.type = 0; ep.len = 1.8f + vState.spike_intensity * 4.0f + vState.edge_activity * 2.5f;
                } else if (roll < 0.75f + vState.spike_intensity * 0.3f + gMag * 0.15f) {
                    ep.type = 1; ep.vx = ep.nx * 0.05f; ep.vy = ep.ny * 0.05f; ep.len = 0;
                } else {
                    ep.type = 2; ep.len = 2.5f + vState.edge_activity * 3.5f;
                }
            }
        }
    }
}


void Venom::drawGloss(M5Canvas* canvas, float px, float py, float ax, float ay) {
    uint32_t t = millis();
    for (int y = 5; y < FIELD_H - 5; y += 4) {
        for (int x = 5; x < FIELD_W - 5; x += 4) {
            if (field[y * FIELD_W + x] > 200 && (x * 7 + y * 13 + t / 200) % 17 == 0) {
                float jx = sinf(t * 0.005f + x) * 2.0f;
                float jy = cosf(t * 0.005f + y) * 2.0f;
                canvas->fillCircle((int)(x * FIELD_SCALE + px + jx), (int)(y * FIELD_SCALE + py + jy), 1, COLOR_VENOM_GLOSS);
            }
        }
    }
}

void Venom::drawEye(M5Canvas* canvas, float px, float py, float ax, float ay) {
    if (currentSkillName != "sleep" && isBlinking && millis() - lastBlinkTime < 120) return;

    // 直接用骨骼头节点 (Node 0) 的屏幕投影坐标作为眼睛位置
    const Node& head = skeleton.getNode(0);
    float hx, hy, hz;
    projectToFace(head, currentFace, hx, hy, hz, ax, ay);

    // 眼睛偏移：跟随瞳孔视线方向
    float eyeX = hx + pupilX * 8.0f;
    float eyeY = hy + pupilY * 6.0f;

    if (currentSkillName == "sleep") {
        // [修复] 锁定眼部位置在头部中心，完全忽略 pupilX/Y 的偏移
        int ix = (int)hx, iy = (int)hy;
        int sw = 10 * vState.body_scale;
        // 绘制可爱的向下弯曲弧线 (眯眯眼)
        canvas->drawArc(ix, iy - 2, sw, sw, 30, 150, TFT_DARKGREY);
        canvas->drawArc(ix, iy - 1, sw-1, sw-1, 35, 145, 0x3186); 
        return;
    }

    // 纯白色眼白 (修正颜色，移除偏青灰的高光色)
    canvas->fillEllipse((int)eyeX, (int)eyeY, 14, 10, COLOR_EYE_WHITE);
    canvas->drawEllipse((int)eyeX, (int)eyeY, 14, 10, COLOR_VENOM_BODY);

    // 瞳孔
    float pSize = 3.5f * pupilSize;
    if (pSize < 1.0f) pSize = 1.0f;
    canvas->fillCircle((int)(eyeX + pupilX * 3.5f),
                       (int)(eyeY + pupilY * 2.5f),
                       (int)pSize, COLOR_VENOM_BODY);

    // 高压反光点
    canvas->drawPixel((int)(eyeX - 4), (int)(eyeY - 3), 0xFFFF);

    // 紧张状态：极轻微血丝 (仅保留随机点状感，消除线条感)
    if (stress > 0.72f) { 
        int dotCount = (int)((stress - 0.72f) * 15.0f) + 2;
        for (int i = 0; i < dotCount; i++) {
            float ang = (float)random(360) * 0.0174f;
            float dist = 7.0f + (float)random(5);
            canvas->drawPixel((int)(eyeX + cosf(ang) * dist),
                             (int)(eyeY + sinf(ang) * dist * 0.75f), 0xF800);
        }
    }
}
void Venom::drawDebug(M5Canvas* canvas) {
    if (!showDebug) return;

    // --- 1. 面板定位 (加宽以实现左右边距对称) ---
    int bw = 230, bh = 115;
    int bx = (SCREEN_W - bw) / 2; // 居中定位: bx = 5
    int by = 6;
    
    // 恢复全透明样式，但保留柔和配色以减少残影
    uint16_t cyan = 0x059d; 
    uint16_t glow = 0x028A; 
    uint16_t red  = 0x9800;
    
    // --- 2. 绘制装饰角标 (HUD Frame Corners) ---
    // 左上
    canvas->drawLine(bx, by, bx+10, by, cyan);
    canvas->drawLine(bx, by, bx, by+10, cyan);
    // 右上
    canvas->drawLine(bx+bw, by, bx+bw-10, by, cyan);
    canvas->drawLine(bx+bw, by, bx+bw, by+10, cyan);
    // 左下
    canvas->drawLine(bx, by+bh, bx+10, by+bh, cyan);
    canvas->drawLine(bx, by+bh, bx, by+bh-10, cyan);
    // 右下
    canvas->drawLine(bx+bw, by+bh, bx+bw-10, by+bh, cyan);
    canvas->drawLine(bx+bw, by+bh, bx+bw, by+bh-10, cyan);

    // --- 3. 辅助绘制函数 ---
    char buf[64];
    auto drawGlowText = [&](const char* text, int x, int y, uint16_t color) {
        canvas->setTextColor(glow);
        canvas->setCursor(x+1, y+1); canvas->print(text);
        canvas->setTextColor(color);
        canvas->setCursor(x, y); canvas->print(text);
    };

    // --- 增强型 HUD 诊断信息 (已整合) ---
    char diagBuf[64];
    
    // 计算物理抖动度 (各节点速度的标准差)
    float v_sum = 0, v_sq_sum = 0;
    for(int i=0; i<MAX_NODES; i++) {
        float v_mag = sqrtf(skeleton.getNode(i).vx*skeleton.getNode(i).vx + skeleton.getNode(i).vy*skeleton.getNode(i).vy);
        v_sum += v_mag;
        v_sq_sum += v_mag * v_mag;
    }
    float v_mean = v_sum / MAX_NODES;
    float jitter = sqrtf(fmaxf(0, v_sq_sum / MAX_NODES - v_mean * v_mean));

    snprintf(diagBuf, sizeof(diagBuf), "BEHAVIOR: %s", currentSkillName.c_str());
    drawGlowText(diagBuf, bx+12, by+10, cyan); 
    
    snprintf(diagBuf, sizeof(diagBuf), "FPS: %d", (int)currentFPS);
    drawGlowText(diagBuf, bx+bw-85, by+10, TFT_WHITE); 
    
    snprintf(diagBuf, sizeof(diagBuf), "JIT: %.3f", jitter);
    drawGlowText(diagBuf, bx+bw-85, by+22, (jitter < 0.5f) ? 0x07E0 : red); 

    // 如果抖动过大且处于闲置，在串口输出预警
    if (jitter > 1.5f && currentSkillName == "idle") {
        M5.Log.printf(">>> [Diag] High Jitter: %.3f\n", jitter);
    }


    // 核心参数
    // 已将 BEHAVIOR 移动至顶部标题栏

    auto drawDataBar = [&](int x, int y, float val, uint16_t color) {
        int barW = 50;
        canvas->drawRect(x, y, barW, 4, glow);
        canvas->fillRect(x, y, (int)(val * barW), 4, color);
    };

    // --- 4. 渲染数据项 ---
    canvas->setTextSize(1);
    int x = bx + 6, y = by + 8;
    int xr = bx + bw - 12; // 右侧列右边界对齐坐标
    int x2 = bx + bw - 85; // 右侧列左起始 (保持左对齐)

    // 删除会导致重叠的 SYSTEM_LOG 打印
    // (已在顶部打印了 BEHAVIOR)
    
    // --- 左列：核心生理参数 ---
    int ly = y + 16;
    drawGlowText("NRG", x, ly, cyan); drawDataBar(x + 30, ly + 2, energy, cyan);
    
    ly += 10; drawGlowText("STR", x, ly, (stress > 0.6f) ? red : cyan);
    drawDataBar(x + 30, ly + 2, stress, (stress > 0.6f) ? red : cyan);
    
    ly += 10; drawGlowText("FAT", x, ly, (fatigue > 0.6f) ? red : cyan);
    drawDataBar(x + 30, ly + 2, fatigue, (fatigue > 0.6f) ? red : cyan);
    
    ly += 10; drawGlowText("VIG", x, ly, cyan);
    drawDataBar(x + 30, ly + 2, vigilance, cyan);

    ly += 10; drawGlowText("COM", x, ly, cyan);
    drawDataBar(x + 30, ly + 2, comfort, cyan);

    ly += 10; drawGlowText("TNS", x, ly, cyan);
    drawDataBar(x + 30, ly + 2, muscleTension, cyan);

    ly += 10; drawGlowText("OVR", x, ly, (overstimulation > 0.5f) ? red : cyan);
    drawDataBar(x + 30, ly + 2, overstimulation, (overstimulation > 0.5f) ? red : cyan);

    // --- 右列：传感器与系统 ID (整列居右，内容左对齐) ---
    int ry = y + 24;
    float roll = atan2f(lastGY, lastGZ) * 57.2958f;
    float pitch = atan2f(-lastGX, sqrtf(lastGY*lastGY + lastGZ*lastGZ)) * 57.2958f;
    
    sprintf(diagBuf, "IMU R:%dP:%d", (int)roll, (int)pitch); 
    drawGlowText(diagBuf, x2, ry, 0x9630);
    
    ry += 12;
    sprintf(buf, "MIC %.1fdB", lastSoundLevel); 
    drawGlowText(buf, x2, ry, 0x9630);
    
    ry += 12;
    sprintf(buf, "LUX %.1f", lastLux);
    drawGlowText(buf, x2, ry, (lastLux > 1000) ? 0xF800 : 0xFFE0); 

    // --- [新增] AI 驱动指示器 (黑海胆动态图标) ---
    uint32_t now = millis();
    // 判定逻辑：最近 5 分钟内收到过 AI 指令则显示
    if (lastLLMResponseTime > 0 && (now - lastLLMResponseTime < 300000)) {
        int ux = x2 + 55, uy = ry + 25; // 居右下方
        float time = now * 0.003f;
        
        // 1. 绘制核心
        canvas->fillCircle(ux, uy, 4, COLOR_VENOM_BODY);
        
        // 2. 绘制动态尖刺 (12 根不规则尖刺)
        for(int i=0; i<12; i++) {
            float angle = (i * 30 + (int)(sinf(time + i) * 12)) * 0.0174f;
            float len = 6 + sinf(time * 2.0f + i * 1.5f) * 5; // 长度伸缩
            int x1 = ux + cosf(angle) * 3;
            int y1 = uy + sinf(angle) * 3;
            int x2_p = ux + cosf(angle) * len;
            int y2_p = uy + sinf(angle) * len;
            canvas->drawLine(x1, y1, x2_p, y2_p, COLOR_VENOM_BODY);
            // 给尖刺末端加点暗光
            canvas->drawPixel(x2_p, y2_p, 0x4208); 
        }
        
        // 3. 标注 AI 状态
        drawGlowText("NEURAL_LINK", ux - 35, uy + 12, 0x4208);
    }

    // --- 4. 动态装饰: 扫描线 ---
    static float scanY = 0;
    scanY += 0.8f;
    if (scanY > bh) scanY = 0;
    canvas->drawFastHLine(bx + 2, by + (int)scanY, bw - 4, 0x18C3); // 极淡的扫描线
}

void Venom::drawBox(M5Canvas* canvas, float ax, float ay) {
    // 绘制 3D 盒子的线框以增强空间感
    static const float corners[8][3] = {
        {-CUBE_W, -CUBE_H, -CUBE_D}, {CUBE_W, -CUBE_H, -CUBE_D},
        {CUBE_W, CUBE_H, -CUBE_D}, {-CUBE_W, CUBE_H, -CUBE_D},
        {-CUBE_W, -CUBE_H, CUBE_D}, {CUBE_W, -CUBE_H, CUBE_D},
        {CUBE_W, CUBE_H, CUBE_D}, {-CUBE_W, CUBE_H, CUBE_D}
    };
    
    float px[8], py[8], pz[8];
    for (int i = 0; i < 8; i++) {
        projectPoint(corners[i][0], corners[i][1], corners[i][2], currentFace, px[i], py[i], pz[i], ax, ay);
    }
    
    uint16_t boxColor = 0x4208; // 暗灰色线框
    // 绘制 12 条棱 (px/py 已经是绝对屏幕坐标)
    for (int i = 0; i < 4; i++) {
        canvas->drawLine(px[i], py[i], px[(i+1)%4], py[(i+1)%4], boxColor);
        canvas->drawLine(px[i+4], py[i+4], px[((i+1)%4)+4], py[((i+1)%4)+4], boxColor);
        canvas->drawLine(px[i], py[i], px[i+4], py[i+4], boxColor);
    }
}

void Venom::updateStateFromLLM(const String& jsonEmotion) {
    // [核心大脑] 解析来自云端 LLM 的高维情感指令
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonEmotion);
    if (error) {
        M5.Log.printf(">>> [AI] JSON Parse Error: %s\n", error.c_str());
        return;
    }

    // 1. 调整长期情绪基调
    if (doc.containsKey("curiosity")) curiosity = doc["curiosity"];
    if (doc.containsKey("comfort")) comfort = doc["comfort"];
    if (doc.containsKey("attachment")) attachment = doc["attachment"];
    if (doc.containsKey("energy")) energy = doc["energy"];
    
    // 2. 行为暗示：如果云端给出了明确的动机，则大幅提升该行为的分数
    if (doc.containsKey("suggested_behavior")) {
        String suggest = doc["suggested_behavior"];
        M5.Log.printf(">>> [AI] Received cloud behavioral suggestion: %s\n", suggest.c_str());
    }

    // 3. 表情符号：由云端决定触发特定的视觉符号
    if (doc.containsKey("expression")) {
        String expr = doc["expression"];
        triggerExpression(expr);
        M5.Log.printf(">>> [AI] Triggering cloud-driven expression: %s\n", expr.c_str());
    }

    lastLLMResponseTime = millis(); // 刷新活跃时间
    M5.Log.printf(">>> [AI] Personality profile updated from cloud.\n");
}




