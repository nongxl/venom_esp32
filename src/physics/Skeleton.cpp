#include "Skeleton.h"
#include <math.h>

Skeleton::Skeleton() {
    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].x = 0;
        nodes[i].y = 0;
        nodes[i].z = 0; 
        nodes[i].vx = nodes[i].vy = nodes[i].vz = 0;
        if (i == 0) {
            nodes[i].radius = NODE_RADIUS_BASE; 
        } else {
            // 将衰减系数从 0.65 下调至 0.55，使“头大尾尖”的生物感更加极端明显
            nodes[i].radius = NODE_RADIUS_BASE * powf(0.55f, i);
        }
        nodes[i].is_stuck = false;
        nodes[i].tension = 0; 
        nodes[i].impactGrace = 0; 
        hasTarget[i] = false;
        targetStrength[i] = 0;
    }
    stiffnessMultiplier = 1.0f;
    dampingMultiplier = 1.0f;
    restLengthScale = 1.0f; // [新增] 初始化
    lastGx = lastGy = lastGz = 0;
    fallingTimer = 0;
    isFalling = false;
}

void Skeleton::update(float gx, float gy, float gz) {
    // 【物理防火墙】过滤非法传感器数据
    if (isnan(gx) || isnan(gy) || isnan(gz)) { gx = 0; gy = 9.8f; gz = 0; }
    gx = fminf(20.0f, fmaxf(-20.0f, gx));
    gy = fminf(20.0f, fmaxf(-20.0f, gy));
    gz = fminf(20.0f, fmaxf(-20.0f, gz));

    // 1. 每帧清零外部牵引力
    for (int i = 0; i < MAX_NODES; i++) nodes[i].externalForce = 0;

    // 2. 计算内部弹簧力和外部触手牵引力 (填充 externalForce)
    applyInternalForces(); 

    // 3. 执行核心物理计算与位移积分
    applyAntigravityBehavior(gx, gy, gz);

    // 4. 强制执行边界约束
    applyConstraints(); 

    // 5. 【最终审判】确保没有任何节点能逃脱物理法则
    float hardLimit = (forceDetach) ? 100.0f : 30.0f;
    if (dampingMultiplier < 0.6f) hardLimit = 10.0f; // 闲置态下绝对不允许超过 10.0

    for (int i = 0; i < MAX_NODES; i++) {
        Node& n = nodes[i];
        // 修复 NaN
        if (isnan(n.vx)) n.vx = 0;
        if (isnan(n.vy)) n.vy = 0;
        if (isnan(n.vz)) n.vz = 0;
        if (isnan(n.x)) n.x = 0;
        if (isnan(n.y)) n.y = 0;

        float vMagSq = n.vx*n.vx + n.vy*n.vy + n.vz*n.vz;
        if (vMagSq > hardLimit * hardLimit) {
            float vMag = sqrtf(vMagSq);
            n.vx = (n.vx / vMag) * hardLimit;
            n.vy = (n.vy / vMag) * hardLimit;
            n.vz = (n.vz / vMag) * hardLimit;
        }
    }
}

void Skeleton::setDynamicPhysics(float stiffnessMult, float dampingMult) {
    stiffnessMultiplier = stiffnessMult;
    dampingMultiplier = dampingMult;
}

void Skeleton::setTargetNode(int index, float x, float y, float z, float strength) {
    if (index < 0 || index >= MAX_NODES) return;
    targetX[index] = x;
    targetY[index] = y;
    targetZ[index] = z;
    targetStrength[index] = strength;
    hasTarget[index] = true;
}

void Skeleton::clearTargetNode(int index) {
    if (index < 0 || index >= MAX_NODES) return;
    hasTarget[index] = false;
}

void Skeleton::applyAntigravityBehavior(float gx, float gy, float gz) {
    // 检测重力方向是否突变（翻转）
    float dot = gx*lastGx + gy*lastGy + gz*lastGz;
    float magCurrent = sqrtf(gx*gx + gy*gy + gz*gz);
    float magLast = sqrtf(lastGx*lastGx + lastGy*lastGy + lastGz*lastGz);
    
    bool flipped = (magCurrent > 1.0f && magLast > 1.0f && dot / (magCurrent * magLast) < -0.5f); 
    
    if (flipped && !isFalling) {
        isFalling = true;
        fallingTimer = millis();
    }

    for (int i = 0; i < MAX_NODES; i++) {
        Node& n = nodes[i];
        
        // --- 1. 环境感应与应力计算 ---
        bool onWall = (abs(n.x) >= CUBE_W - STICK_DISTANCE || abs(n.y) >= CUBE_H - STICK_DISTANCE || abs(n.z) >= CUBE_D - STICK_DISTANCE);

        // 计算物理应力
        float stressMag = n.externalForce; 
        if (i > 0) {
            float dx = nodes[i-1].x - n.x, dy = nodes[i-1].y - n.y, dz = nodes[i-1].z - n.z;
            float dist = sqrtf(dx*dx+dy*dy+dz*dz);
            float rest = (i == 1) ? 9.0f : 6.0f;
            
            // 必须与内部弹簧逻辑保持一致的缩放，否则会在弹簧还有拉力时提前判定为无拉伸导致假死锁死！
            if (forceDetach) rest *= 2.2f;
            else if (stiffnessMultiplier > 1.5f) rest *= 0.9f;
            else if (stiffnessMultiplier < 0.5f) rest *= 0.7f;
            
            stressMag += fmaxf(0, dist - rest) * 8.5f; // 进一步增加拉伸应力权重，使得一旦拉伸立刻剥离
            
            // 【强制剥离逻辑】如果与父节点距离拉长过大，直接产生巨大应力，彻底拔起尾巴
            if (dist > rest * 1.5f && !nodes[i-1].is_stuck) {
                stressMag += 45.0f; 
            }
        }

        // 【核心力学重构：头重尾轻】
        // 1. 质量估算：半径越大，质量越大，受重力拉扯越猛
        float massFactor = n.radius / NODE_RADIUS_BASE; 
        
        // 2. 粘附门槛：越细的节点粘得越牢 (反比关系)
        float baseThreshold = UNSTICK_THRESHOLD * (1.5f - massFactor * 0.8f);
        float dynamicThreshold = baseThreshold;

        // 3. 滞后效应：如果已经粘住了，需要 5 倍的力量才能撕下来 (模拟胶带)
        if (n.is_stuck) dynamicThreshold *= 5.0f;

        // 3. 剥离波：如果父节点已撕开，本节点门槛骤降
        if (i > 0 && !nodes[i-1].is_stuck) dynamicThreshold *= 0.15f; 
        
        bool underHeavyStress = (stressMag > dynamicThreshold); 
        
        float gMag = sqrtf(gx*gx + gy*gy + gz*gz);
        bool externalShock = (gMag > IMU_DETACH_FORCE); 

        // --- 2. 剥离与吸附逻辑 (Peeling Logic) ---
        float suctionX = 0, suctionY = 0, suctionZ = 0;
        float nodeWeightFactor = 1.0f - ((float)i / MAX_NODES) * 0.7f; 
        float currentSuctionK = BASE_SUCTION_FORCE * nodeWeightFactor;
        
        if (i > 0 && !nodes[i-1].is_stuck) currentSuctionK *= (PEELING_SOFTEN_RATIO); 

        // [整合] 强力吸附逻辑：使用 forceMag = 15.5 进行贴合
        float dX = CUBE_W - abs(n.x), dY = CUBE_H - abs(n.y), dZ = CUBE_D - abs(n.z);
        float minDist = fminf(dX, fminf(dY, dZ));
        if (minDist < 15.0f && !forceDetach) {
            float velMag = sqrtf(n.vx*n.vx + n.vy*n.vy + n.vz*n.vz);
            float vAttractionScale = fmaxf(0.1f, 1.0f - velMag / 10.0f);
            float softZoneScale = (minDist < 0.8f) ? (minDist / 0.8f) : 1.0f;
            float strength = (1.0f - minDist/15.0f) * 15.5f * vAttractionScale * softZoneScale;
            
            if (dX == minDist) suctionX = (n.x > 0 ? 1 : -1) * strength;
            else if (dY == minDist) suctionY = (n.y > 0 ? 1 : -1) * strength;
            else if (dZ == minDist) suctionZ = (n.z > 0 ? 1 : -1) * strength;
        } else if (forceDetach) {
            // 剥离态下仅保留极微弱回弹
            if (abs(n.x) > CUBE_W - 5.0f) suctionX = (n.x > 0 ? (CUBE_W - n.x) : (-CUBE_W - n.x)) * currentSuctionK;
            if (abs(n.y) > CUBE_H - 5.0f) suctionY = (n.y > 0 ? (CUBE_H - n.y) : (-CUBE_H - n.y)) * currentSuctionK;
            if (abs(n.z) > CUBE_D - 5.0f) suctionZ = (n.z > 0 ? (CUBE_D - n.z) : (-CUBE_D - n.z)) * currentSuctionK;
        }

        if (underHeavyStress || forceDetach) {
            suctionX *= PEELING_FORCE_DECAY; suctionY *= PEELING_FORCE_DECAY; suctionZ *= PEELING_FORCE_DECAY;
        }
        n.vx += suctionX; n.vy += suctionY; n.vz += suctionZ;

        // --- 3. 状态更新与物理积分 ---
        bool canStickState = (dampingMultiplier < 0.7f || dampingMultiplier > 0.9f);
        if (onWall && !underHeavyStress && !externalShock && canStickState) {
            n.is_stuck = true; 
            n.lastStuckTime = millis();
            n.vx = 0; n.vy = 0; n.vz = 0; // 粘住状态下严格归零，杜绝任何位移
        } else {
            // 【修正破冰逻辑】降低胶带撕开瞬间的冲量
            if (n.is_stuck) {
                if (i > 0) {
                    float dx = nodes[i-1].x - n.x, dy = nodes[i-1].y - n.y, dz = nodes[i-1].z - n.z;
                    float d = sqrtf(dx*dx+dy*dy+dz*dz);
                    if (d > 0.1f) {
                        n.vx += (dx/d) * 1.5f; n.vy += (dy/d) * 1.5f; n.vz += (dz/d) * 1.5f;
                    }
                }
            }
            n.is_stuck = false;
            // 自由运动：计算抗重力和阻尼
            uint32_t timeSinceStuck = millis() - n.lastStuckTime;
            float viscousResist = (timeSinceStuck < 1500) ? 0.98f : ADHESION_THRESHOLD;
            float gravityResist = (dampingMultiplier < 0.6f) ? 0.98f : viscousResist;
            
            float resist = (isFalling) ? ((millis() - fallingTimer < FALLING_STRUGGLE_MS) ? 0.99f : 0.4f) : gravityResist;
            
            // 质量力学应用
            float gPull = (1.0f - resist) * (0.2f + massFactor * 1.6f);
            n.vx += gx * gPull; n.vy += gy * gPull; n.vz += gz * gPull;
            
            n.vx *= SPRING_DAMPING * dampingMultiplier;
            n.vy *= SPRING_DAMPING * dampingMultiplier;
            n.vz *= SPRING_DAMPING * dampingMultiplier;

            // 【核心修正：速度死区】
            if (dampingMultiplier < 0.6f && (n.vx*n.vx + n.vy*n.vy + n.vz*n.vz) < 0.02f) {
                n.vx = 0; n.vy = 0; n.vz = 0;
            }
        }

        // 【核心修正：速度上限重构】
        // 闲置状态 (Damping ~0.45) 限制在 8.0
        // 移动状态 (Damping ~0.8) 限制在 25.0
        // 暴走状态 (ForceDetach) 允许到 100.0
        float maxVel = 25.0f;
        if (forceDetach) maxVel = 100.0f;
        else if (dampingMultiplier < 0.6f) maxVel = 8.0f; 

        float velSq = n.vx*n.vx + n.vy*n.vy + n.vz*n.vz;
        if (velSq > maxVel * maxVel) {
            float vMag = sqrtf(velSq);
            n.vx = (n.vx / vMag) * maxVel; n.vy = (n.vy / vMag) * maxVel; n.vz = (n.vz / vMag) * maxVel;
        }

        // 位移应用
        n.x += n.vx; n.y += n.vy; n.z += n.vz;
    }

    // 重置状态检测
    if (!flipped && isFalling && (millis() - fallingTimer > FALLING_STRUGGLE_MS * 3)) {
        isFalling = false;
    }

    lastGx = gx; lastGy = gy; lastGz = gz;
}

void Skeleton::applyInternalForces() {
    for (int i = 1; i < MAX_NODES; i++) {
        float dx = nodes[i-1].x - nodes[i].x;
        float dy = nodes[i-1].y - nodes[i].y;
        float dz = nodes[i-1].z - nodes[i].z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        float baseRest = (i == 1) ? 9.0f : 6.0f; 
        // [核心改进] 应用 restLengthScale 动态缩放
        float restDist = baseRest * restLengthScale; 
        
        bool headAnchored = false;
        if (hasTarget[0]) {
            float hdx = targetX[0] - nodes[0].x, hdy = targetY[0] - nodes[0].y, hdz = targetZ[0] - nodes[0].z;
            // 放宽头部到达的判定范围（20像素以内就算抵达），防止头部在终点附近抖动导致无法触发收缩
            if (hdx*hdx + hdy*hdy + hdz*hdz < 400.0f) headAnchored = true; 
        }

        // 仅在强制脱落（受惊暴走）时拉长骨骼间距，产生凌厉的细长流体感
        if (forceDetach) restDist *= 2.2f; 
        else if (stiffnessMultiplier > 1.5f) {
            // 头到终点后，全身静息间距压缩至 15%，迫使尾巴“啪”地瞬间弹射回头部！
            if (headAnchored) {
                // 极致压缩 restDist (0.01x)，实现“重吸收融合”感，消除任何物理缝隙
                restDist *= 0.01f; 
                if (i > 0) {
                    float toHeadX = nodes[0].x - nodes[i].x;
                    float toHeadY = nodes[0].y - nodes[i].y;
                    float toHeadZ = nodes[0].z - nodes[i].z;
                    float toHeadDist = sqrtf(toHeadX*toHeadX + toHeadY*toHeadY + toHeadZ*toHeadZ);
                    if (toHeadDist > 3.0f) {
                        float massivePull = 120.0f; // 极致引力，确保尾巴瞬间合体
                        nodes[i].vx += (toHeadX / toHeadDist) * massivePull;
                        nodes[i].vy += (toHeadY / toHeadDist) * massivePull;
                        nodes[i].vz += (toHeadZ / toHeadDist) * massivePull;
                    }
                }
            } else {
                restDist *= 0.8f; 
            }
        }
        else if (stiffnessMultiplier < 0.5f) restDist *= 0.7f; // 闲置时更紧凑

        if (dist > 0.01f) {
            float stiffness = SPRING_STIFFNESS * powf(SPRING_STIFFNESS_DECAY, i-1) * stiffnessMultiplier;
            
            // 暴走和闲置时都不注入随机噪波，确保物理轨迹绝对平滑
                // 仅在常规移动或探索时加入微量随机感
                float noiseAmp = (stiffnessMultiplier > 0.8f && stiffnessMultiplier < 1.2f) ? 0.01f : 0.0f;
                if (noiseAmp > 0) {
                    nodes[i].vx += (random(-10, 11) * noiseAmp);
                    nodes[i].vy += (random(-10, 11) * noiseAmp);
                }

            // 【受力减振】下调受力倍率，彻底根除高 JIT 
            float force;
            // 降低倍率：移动态 (3.5)，闲置态 (1.0)
            float forceScale = (stiffnessMultiplier > 1.1f) ? 3.5f : 1.0f; 
            float delta = (dist - restDist);

            if (dist < restDist) {
                // 【核心改进：质量融合】当头部已锚定且处于强拉力状态时，禁止产生内部排斥力
                if (headAnchored && stiffnessMultiplier > 1.5f) force = 0;
                else force = delta * stiffness * 1.2f * forceScale; 
            } else {
                // 在暴走状态下引入渐进刚度
                if (forceDetach && delta > 0) {
                    force = powf(delta / restDist, 1.5f) * stiffness * 25.0f * forceScale;
                } else {
                    force = delta * stiffness * 0.8f * forceScale; 
                }
            }
            
            // 严格限制最大力输出，防止系统发散
            float maxForce = (stiffnessMultiplier > 1.1f) ? 80.0f : 20.0f; 
            force = fmaxf(-maxForce, fminf(maxForce, force));
            
            // 【质心迁移】计算张力：反映节点的拉伸状态
            nodes[i].tension = dist / restDist;

            // 【核心改进：动力学配平】
            // 1. 暴走模式：强化质量梯度，尾部设为头部的 4.5 倍，产生极强的滞后和甩动感
            // 2. 正常模式：取消增重防止迟钝，改用 95% 的反向拉力免疫来维持锚点稳固
            float m_i = 1.0f;
            float m_prev = 1.0f;
            float backPullScale = 1.0f;

            if (forceDetach) {
                m_i = 1.0f + ((float)i / MAX_NODES) * 3.5f;
                m_prev = 1.0f + ((float)(i-1) / MAX_NODES) * 3.5f;
            } else {
                if (i > 0 && hasTarget[i-1]) {
                    backPullScale = 0.05f; 
                }
            }

            nodes[i].vx += ((dx / dist) * force) / m_i;
            nodes[i].vy += ((dy / dist) * force) / m_i;
            nodes[i].vz += ((dz / dist) * force) / m_i;
            
            nodes[i-1].vx -= ((dx / dist) * force * backPullScale) / m_prev;
            nodes[i-1].vy -= ((dy / dist) * force * backPullScale) / m_prev;
            nodes[i-1].vz -= ((dz / dist) * force * backPullScale) / m_prev;

            // 【内部阻尼优化：动态粘滞力】
            float rvx = nodes[i-1].vx - nodes[i].vx;
            float rvy = nodes[i-1].vy - nodes[i].vy;
            float rvz = nodes[i-1].vz - nodes[i].vz;
            // 降低粘滞权重，防止速度超调 (Overshoot)
            float internalViscosity = (stiffnessMultiplier > 1.1f) ? 0.05f : 0.25f; 
            nodes[i].vx += rvx * internalViscosity;
            nodes[i].vy += rvy * internalViscosity;
            nodes[i].vz += rvz * internalViscosity;
            nodes[i-1].vx -= rvx * internalViscosity;
            nodes[i-1].vy -= rvy * internalViscosity;
            nodes[i-1].vz -= rvz * internalViscosity;
        } else {
            nodes[i].tension = 0.1f; // 极度压缩
        }
    }

    // --- 3. 弯曲约束弹簧 (Bending Springs) ---
    // 在 i 与 i-2 之间增加约束，防止高速运动时身体拉成直线，保持生物曲率感
    for (int i = 2; i < MAX_NODES; i++) {
        float dx = nodes[i-2].x - nodes[i].x;
        float dy = nodes[i-2].y - nodes[i].y;
        float dz = nodes[i-2].z - nodes[i].z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        if (dist > 0.01f) {
            float rest = (nodes[i-2].radius + nodes[i].radius) * 1.2f;
            float stiffness = (dampingMultiplier > 0.9f) ? 0.35f : 0.15f;
            float force = (dist - rest) * stiffness * stiffnessMultiplier;
            nodes[i].vx += (dx / dist) * force * 0.5f;
            nodes[i].vy += (dy / dist) * force * 0.5f;
            nodes[i-2].vx -= (dx / dist) * force * 0.5f;
            nodes[i-2].vy -= (dy / dist) * force * 0.5f;
        }
    }

    // 应用目标锁定拉力 (基于锚点的生物动力)
    for (int i = 0; i < MAX_NODES; i++) {
        if (hasTarget[i]) {
            float dx = targetX[i] - nodes[i].x;
            float dy = targetY[i] - nodes[i].y;
            float dz = targetZ[i] - nodes[i].z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist > 0.01f) {
                // 【力量平滑】限制单次牵引力的最大值，防止“瞬移”导致的物理逻辑崩溃
                const float maxPullForce = 25.0f;
                float k = targetStrength[i] * stiffnessMultiplier * 0.008f; 
                float fx = dx * k;
                float fy = dy * k;
                float fz = dz * k;
                
                // 彻底放开牵引力上限，配合内部弹簧传导，使得移动极速干脆
                float fMagSq = fx*fx + fy*fy + fz*fz;
                const float maxF = 150.0f; 
                float fMag = sqrtf(fMagSq);
                if (fMag > maxF) {
                    float s = maxF / fMag;
                    fx *= s; fy *= s; fz *= s;
                    fMag = maxF;
                }
                
                nodes[i].vx += fx;
                nodes[i].vy += fy;
                nodes[i].vz += fz;
                // 【关键同步】牵引力存入节点，供后续剥离逻辑使用
                nodes[i].externalForce = fMag; 
            }
        }
    }

    /* 
    // 【移除：全节点排斥力】这是产生“身体抖动”的主要根源
    for (int i = 0; i < MAX_NODES; i++) {
        for (int j = i + 1; j < MAX_NODES; j++) {
            float dx = nodes[i].x - nodes[j].x;
            float dy = nodes[j].y - nodes[i].y; 
            float dz = nodes[i].z - nodes[j].z;
            float distSq = dx*dx + dy*dy + dz*dz;
            float repulsionMult = (stiffnessMultiplier > 1.2f) ? 0.35f : 0.45f;
            float repulsionDist = (nodes[i].radius + nodes[j].radius) * repulsionMult;
            if (distSq < repulsionDist * repulsionDist && distSq > 0.001f) {
                float dist = sqrtf(distSq);
                float repulsionStrength = (stiffnessMultiplier > 1.2f) ? 0.6f : 0.4f;
                float force = (repulsionDist - dist) * repulsionStrength;
                float nx = dx / dist; float ny = dy / dist; float nz = dz / dist;
                nodes[i].vx += nx * force; nodes[i].vy += ny * force; nodes[i].vz += nz * force;
                nodes[j].vx -= nx * force; nodes[j].vy -= ny * force; nodes[j].vz -= nz * force;
            }
        }
    }
    */
}

void Skeleton::applyConstraints() {
    const float border = 0.5f; 
    for (int i = 0; i < MAX_NODES; i++) {
        Node& n = nodes[i];
        
        bool heavyImpact = false;
        const float bounce = 0.3f; 
        const float impactThreshold = 2.5f; // 必须有明显的速度撞击才算着陆
        if (n.x < -CUBE_W) { n.x = -CUBE_W; if (n.vx < -impactThreshold) heavyImpact = true; n.vx = abs(n.vx) * bounce; }
        if (n.x >  CUBE_W) { n.x =  CUBE_W; if (n.vx >  impactThreshold) heavyImpact = true; n.vx = -abs(n.vx) * bounce; }
        if (n.y < -CUBE_H) { n.y = -CUBE_H; if (n.vy < -impactThreshold) heavyImpact = true; n.vy = abs(n.vy) * bounce; }
        if (n.y >  CUBE_H) { n.y =  CUBE_H; if (n.vy >  impactThreshold) heavyImpact = true; n.vy = -abs(n.vy) * bounce; }
        if (n.z < -CUBE_D) { n.z = -CUBE_D; if (n.vz < -impactThreshold) heavyImpact = true; n.vz = abs(n.vz) * bounce; }
        if (n.z >  CUBE_D) { n.z =  CUBE_D; if (n.vz >  impactThreshold) heavyImpact = true; n.vz = -abs(n.vz) * bounce; }

        // 【啪嗒着陆保护】只有当节点真实撞穿墙面（即具有朝向墙外的较高速度并越界）时才触发着陆保护
        // 彻底消除了由于弹簧微小弹力将节点压入墙面所导致的假死死锁
        // 仅在闲置放松状态 (dampingMultiplier < 0.8f) 才允许着陆锁定，爬行和暴走时一律免除碰撞惩罚以保证丝滑极速
        if (heavyImpact && !n.is_stuck && !forceDetach && dampingMultiplier < 0.8f) {
            n.impactGrace = IMPACT_GRACE_FRAMES; 
        }

        // 限制在有效物理范围内
        n.x = fmaxf(-CUBE_W, fminf(CUBE_W, n.x));
        n.y = fmaxf(-CUBE_H, fminf(CUBE_H, n.y));
        n.z = fmaxf(-CUBE_D, fminf(CUBE_D, n.z));
    }
}

void Skeleton::applyWallAttraction() {
    // 极致墙面粘性：确保产生明显的“贴合”质感
    const float attractionDist = 15.0f;
    const float forceMag = 15.5f;

    for (int i = 0; i < MAX_NODES; i++) {
        Node& n = nodes[i];
        
        float dX = CUBE_W - abs(n.x);
        float dY = CUBE_H - abs(n.y);
        float dZ = CUBE_D - abs(n.z);

        float minDist = fminf(dX, fminf(dY, dZ));
        if (minDist < attractionDist && !forceDetach) {
            // 降低高速状态下的吸引力，防止被墙吸死
            float velMag = sqrtf(n.vx*n.vx + n.vy*n.vy + n.vz*n.vz);
            float vAttractionScale = fmaxf(0.1f, 1.0f - velMag / 10.0f);
            
            // 【核心修复：力学死区】如果距离墙面极近 (< 0.8)，吸引力线性衰减至 0
            // 彻底解决吸引力与 Collision Constraint 相互推搡导致的抖动
            float softZoneScale = (minDist < 0.8f) ? (minDist / 0.8f) : 1.0f;
            
            float strength = (1.0f - minDist/attractionDist) * forceMag * vAttractionScale * softZoneScale;
            if (dX == minDist) n.vx += (n.x > 0 ? 1 : -1) * strength;
            else if (dY == minDist) n.vy += (n.y > 0 ? 1 : -1) * strength;
            else if (dZ == minDist) n.vz += (n.z > 0 ? 1 : -1) * strength;
        }
    }
}
