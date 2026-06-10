#include "Venom.h"
#include <ArduinoJson.h>
#include <math.h>
#include "Noise.h"
#include "Skills.h"

const int FastNoise::p[] = { 151,160,137,91,90,15,
   131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
   190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
   88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
   77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
   102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
   135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
   5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
   223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
   129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
   251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
   49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
   138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
   151,160,137,91,90,15,
   131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
   190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
   88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
   77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
   102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
   135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
   5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
   223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
   129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
   251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
   49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
   138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};


Venom::Venom() : state(VenomState::IDLE), stateTimer(0) {
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
    lastLLMResponseTime = 0;
    currentAISyncInterval = 120000; // 默认自适应心跳间隔: 120 秒
    isAIPendingSync = false;
    startledJumpCount = 0;

    pupilSize = 0.5f;
    targetPupilSize = 0.5f;
    headLowerProgress = 0.0f; // [初始化] 贴屏窥视进度
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
    lastLLMResponseTime = 0;

    // V3 意识层初值初始化
    lState_emotional_shift = "calm";
    lState_primary_intent = "watch_observer";
    lState_secondary_intent = "";
    lState_focus_target = "observer";
    lState_impulse_strength = 0.5f;
    lState_expression_urge = 0.3f;
    lState_social_openness = 0.4f;
    lState_resentment_delta = 0.0f;
    lState_trust_delta = 0.0f;
    lState_notes = "";

    notes_test_boundary = false;
    notes_watch_observer = false;
    notes_seek_exit = false;
    notes_seek_shadow = false;

    isConsciousnessLeak = false;
    leakStartTime = 0;
    leakDuration = 0;

    hesitationStep = -1;
    hesitationTimer = 0;
}

void Venom::update(float gx, float gy, float gz, float soundLevel, float lux) {
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
                          state == VenomState::OBSERVE || state == VenomState::CLING || 
                          state == VenomState::CRAWL || state == VenomState::SWING);

    if (isMovingState) {
        Node& head = skeleton.getNode(0);
        if (crawlCycle == VenomCrawlState::REACHING) {
            // 伸手阶段：手部向目标移动，增加一点张力准备拉扯
            skeleton.setDynamicPhysics(bodyTension * 1.2f, bodyDamping);
            skeleton.setRestLengthScale(1.0f); // 保证常态
            if (handProgressL >= 1.0f && handProgressR >= 1.0f) {
                crawlCycle = VenomCrawlState::FLOWING;
                crawlTimer = millis();
            }
        } 
        else if (crawlCycle == VenomCrawlState::FLOWING) {
            // 【新物理阶段：FLOWING 粘性流动】
            // 伸手锚定之后，头部首先温和拉伸，内部骨架刚度降低至20%，骨骼间距拉大至 1.8倍
            // 这让身体产生极其惊艳优雅的液态细长拉丝流动，而不是僵硬的一体挪移！
            float tx = (activeHand == 0) ? handLX : handRX;
            float ty = (activeHand == 0) ? handLY : handRY;
            float tz = (activeHand == 0) ? handLZ : handRZ;
            
            float pullForce = PULL_FORCE_MAGNITUDE * speedMult * pullForceScale * 0.45f; 
            skeleton.setTargetNode(0, tx, ty, tz, pullForce);
            
            skeleton.setDynamicPhysics(bodyTension * 0.20f, 0.85f);
            skeleton.setRestLengthScale(1.8f);
            
            vState.lastMoveTime = millis();

            // 流变流动 600ms 后，全身收缩紧绷进入 PULLING 收尾
            if (millis() - crawlTimer > 600) {
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
            float pullForce = PULL_FORCE_MAGNITUDE * speedMult * pullForceScale; 
            skeleton.setTargetNode(0, tx, ty, tz, pullForce);
            
            // 【肌肉爆发力重构】收缩收尾时刚度大幅提升 (2.8)，同时恢复 restLengthScale 为常态 1.0 迫使尾部回弹跟上
            skeleton.setDynamicPhysics(2.8f, 0.85f);
            skeleton.setRestLengthScale(1.0f);
            vState.lastMoveTime = millis(); // 保持计时器更新

            // 【核心重构】动态完成判定：不再死等时间，而是看尾巴有没有跟上
            bool headArrived = (dist < 6.0f);
            
            // 获取尾部节点
            const Node& tail = skeleton.getNode(MAX_NODES - 1);
            float tdx = head.x - tail.x, tdy = head.y - tail.y, tdz = head.z - tail.z;
            float distTailToHead = sqrtf(tdx*tdx + tdy*tdy + tdz*tdz);
            
            // 判定条件：头到位 且 (尾巴收缩到位 或 强行超时)
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
            skeleton.setRestLengthScale(1.0f); // 保证常态
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
    
    // --- 动态肢体回收同步 ---
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

void Venom::updateEye() {
    // V3: 睡觉时眼睛闭上或处于休眠状态
    if (currentSkillName == "rest" && (fatigue > 0.7f || isDormantState())) {
        pupilX = 0.0f;
        pupilY = 0.0f;
        isBlinking = false;
        return;
    }

    uint32_t now = millis();
    
    // 好奇/观察时的眼部扫描逻辑
    bool isScanning = (currentSkillName == "observe");
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

    // 贴屏低头窥视物理进度动态计算 (平滑插值)
    // 只有在主动观察 (observe) 或 交互 (interact) 或 探索 (explore，需好奇心较高) 时，才会在贴屏时触发低头张望
    float targetLower = 0.0f;
    if (currentSkillName == "observe" || currentSkillName == "interact" || (currentSkillName == "explore" && curiosity > 0.6f)) {
        // 呼吸式或者随机式的低头张望 [0.4 ~ 0.95]
        targetLower = 0.4f + 0.55f * (0.5f + 0.5f * sinf(now * 0.002f)); 
    }
    headLowerProgress = headLowerProgress * 0.9f + targetLower * 0.1f;
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
    addRecentEvent("I was startled by a sudden, intense disturbance and entered a frantic leap-and-run state.");
    triggerAISync(); // 立即触发大脑主动同步以感知受惊
    triggerExpression("help"); // 剧烈惊慌时，立即喷出感叹号 ! 以产生震撼反馈
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

void Venom::selectNewCrawlTarget() {
    vState.lastMoveTime = millis(); // 记录起始时间，用于耐性牵引
    int wall = random(6);
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

    // --- V3 领地与偏好巢穴机制 ---
    // 领地 Nesting: 如果有高依恋或在此处休息，高概率回到固定巢穴区域
    bool useTerritory = false;
    if (currentSkillName == "rest" && (observer_attachment > 0.5f || random(100) < 35)) {
        if (memory.territoryX != 0.0f || memory.territoryY != 0.0f || memory.territoryZ != 0.0f) {
            useTerritory = true;
        }
    }

    if (useTerritory) {
        targetX = memory.territoryX + random(-8, 9);
        targetY = memory.territoryY + random(-8, 9);
        targetZ = memory.territoryZ + random(-4, 5);
        targetX = fmaxf(-CUBE_W + 2, fminf(CUBE_W - 2, targetX));
        targetY = fmaxf(-CUBE_H + 2, fminf(CUBE_H - 2, targetY));
        targetZ = fmaxf(-CUBE_D + 2, fminf(CUBE_D - 2, targetZ));
    }
    // --- 基于 8 个 Base Skills 与 Emotion / Desire Modifiers 进行目标选择 ---
    else if (currentSkillName == "observe") {
        // 观察：尝试贴近 Front 屏幕
        targetX = random(-CUBE_W * 0.7f, CUBE_W * 0.7f);
        targetY = random(-CUBE_H * 0.7f, CUBE_H * 0.7f);
        
        // Trust vs Fear Modifier:
        // 信任高更贴近前面，恐惧高更往后缩离得远一点
        float zOffset = observer_trust * 10.0f - observer_fear * 15.0f;
        targetZ = CUBE_D + zOffset;
        targetZ = fmaxf(-CUBE_D + 2, fminf(CUBE_D - 2, targetZ));
        
    } else if (currentSkillName == "hide" || isShy) {
        // 隐藏：退到后壁 (BACK) 或角落边缘
        int hideType = random(100);
        float nextX = 0, nextY = 0, nextZ = 0;

        if (hideType < 40) { 
            wall = (random(100) < 50) ? TOP : BOTTOM;
            nextX = random(-CUBE_W * 0.6f, CUBE_W * 0.6f);
            nextY = (wall == TOP) ? -CUBE_H : CUBE_H;
            nextZ = CUBE_D + 2.0f; // 压在玻璃边缘
        } else if (hideType < 75) {
            wall = BACK;
            nextX = random(-CUBE_W, CUBE_W); nextY = random(-CUBE_H, CUBE_H); nextZ = -CUBE_D;
        } else {
            wall = (random(100) < 50) ? LEFT : RIGHT;
            nextX = (wall == LEFT) ? -CUBE_W : CUBE_W; nextY = random(-CUBE_H, CUBE_H); nextZ = random(-CUBE_D, CUBE_D);
        }
        targetX = nextX; targetY = nextY; targetZ = nextZ;
        
    } else if (currentSkillName == "explore") {
        // 探索：游走，Curiosity Modifiers 好奇心高探索范围更大
        float rScale = 0.5f + curiosity * 0.5f; 
        targetX = random(-CUBE_W * rScale, CUBE_W * rScale);
        targetY = random(-CUBE_H * rScale, CUBE_H * rScale);
        targetZ = random(-CUBE_D * rScale, CUBE_D * rScale);
        
    } else if (currentSkillName == "interact") {
        // 交互：荡秋千挂载或拍前玻璃
        if (random(100) < 40 && observer_trust > 0.4f) {
            targetX = random(-CUBE_W * 0.5f, CUBE_W * 0.5f);
            targetY = random(-CUBE_H * 0.5f, CUBE_H * 0.5f);
            targetZ = CUBE_D;
        } else {
            // 抓天花板
            if (abs(gx) > abs(gy)) wall = (gx > 0) ? LEFT : RIGHT;
            else wall = (gy > 0) ? TOP : BOTTOM;
            
            float nextX = 0, nextY = 0, nextZ = 0;
            if (wall == LEFT) { nextX = -CUBE_W; nextY = random(-CUBE_H*0.5f, CUBE_H*0.5f); nextZ = random(-CUBE_D*0.5f, CUBE_D*0.5f); }
            else if (wall == RIGHT) { nextX = CUBE_W; nextY = random(-CUBE_H*0.5f, CUBE_H*0.5f); nextZ = random(-CUBE_D*0.5f, CUBE_D*0.5f); }
            else if (wall == TOP) { nextX = random(-CUBE_W*0.5f, CUBE_W*0.5f); nextY = -CUBE_H; nextZ = random(-CUBE_D*0.5f, CUBE_D*0.5f); }
            else if (wall == BOTTOM) { nextX = random(-CUBE_W*0.5f, CUBE_W*0.5f); nextY = CUBE_H; nextZ = random(-CUBE_D*0.5f, CUBE_D*0.5f); }
            targetX = nextX; targetY = nextY; targetZ = nextZ;
        }
    } else {
        // 默认通用目标（REST, MOVE, EXPRESS, RECOVER 等的兜底）
        float nextX = 0, nextY = 0, nextZ = 0;
        if (wall == LEFT) { nextX = -CUBE_W; nextY = random(-CUBE_H, CUBE_H); nextZ = random(-CUBE_D, CUBE_D); }
        else if (wall == RIGHT) { nextX = CUBE_W; nextY = random(-CUBE_H, CUBE_H); nextZ = random(-CUBE_D, CUBE_D); }
        else if (wall == TOP) { nextX = random(-CUBE_W, CUBE_W); nextY = -CUBE_H; nextZ = random(-CUBE_D, CUBE_D); }
        else if (wall == BOTTOM) { nextX = random(-CUBE_W, CUBE_W); nextY = CUBE_H; nextZ = random(-CUBE_D, CUBE_D); }
        else { nextX = random(-CUBE_W, CUBE_W); nextY = random(-CUBE_H, CUBE_H); nextZ = CUBE_D; } 
        targetX = nextX; targetY = nextY; targetZ = nextZ;
    }

    // 活动手和进度重置
    if (random(100) < 50) {
        handLX = targetX; handLY = targetY; handLZ = targetZ;
        handProgressL = 0;
        activeHand = 0;
        handProgressR = 1.0f; 
    } else {
        handRX = targetX; handRY = targetY; handRZ = targetZ;
        handProgressR = 0;
        activeHand = 1;
        handProgressL = 1.0f;
    }
}

bool Venom::isDormantState() const {
    return (lState_emotional_shift == "exhausted" || (lState_primary_intent == "seek_safety" && fatigue > 0.6f));
}
