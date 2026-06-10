#include "Skeleton.h"
#include <math.h>

Skeleton::Skeleton() {
    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].x = 0;
        nodes[i].y = 0;
        nodes[i].z = 0; 
        nodes[i].vx = nodes[i].vy = nodes[i].vz = 0;
        if (i == 0) {
            nodes[i].baseRadius = NODE_RADIUS_BASE; 
        } else {
            // 将衰减系数从 0.65 下调至 0.55，使“头大尾尖”的生物感更加极端明显
            nodes[i].baseRadius = NODE_RADIUS_BASE * powf(0.55f, i);
        }
        nodes[i].radius = nodes[i].baseRadius;
        nodes[i].is_stuck = false;
        nodes[i].tension = 0; 
        nodes[i].impactGrace = 0; 
        nodes[i].contactPressure = 0.0f; // [新增] 初始化
        hasTarget[i] = false;
        targetStrength[i] = 0;
    }
    stiffnessMultiplier = 1.0f;
    dampingMultiplier = 1.0f;
    restLengthScale = 1.0f; // [新增] 初始化
    shapeArchetype = 0;    // [新增] 初始化形态学原型
    lastGx = lastGy = lastGz = 0;
    fallingTimer = 0;
    isFalling = false;
    isSwinging = false; // [新增] 荡秋千物理覆盖默认初始化为 false
    maxCollisionSpeed = 0.0f;
}

float Skeleton::getMinDistToSurface(float x, float y, float z) const {
    float dX = CUBE_W - abs(x);
    float dY = CUBE_H - abs(y);
    // 忽略 Z 轴对接触压力的影响，使毒液只在屏幕四壁（左、右、上、下）贴附时发生形变与贴合
    return fminf(dX, dY);
}

void Skeleton::update(float gx, float gy, float gz) {
    // 【物理防火墙】过滤非法传感器数据
    if (isnan(gx) || isnan(gy) || isnan(gz)) { gx = 0; gy = 9.8f; gz = 0; }
    gx = fminf(20.0f, fmaxf(-20.0f, gx));
    gy = fminf(20.0f, fmaxf(-20.0f, gy));
    gz = fminf(20.0f, fmaxf(-20.0f, gz));

    maxCollisionSpeed = 0.0f;

    // -------------------------------------------------------------
    // 【新物理层：节点压力场与形态学原型解算】
    // -------------------------------------------------------------
    float totalPressure = 0.0f;
    int groundContactCount = 0;
    int ceilingContactCount = 0;
    int wallContactCount = 0;
    int cornerContactCount = 0;

    for (int i = 0; i < MAX_NODES; i++) {
        Node& n = nodes[i];
        float minDist = getMinDistToSurface(n.x, n.y, n.z);
        // 基于 ADHESION_RANGE (15.0) 归一化计算压力 [0.0 - 1.0]
        n.contactPressure = 1.0f - fminf(1.0f, fmaxf(0.0f, minDist / ADHESION_RANGE));
        totalPressure += n.contactPressure;

        if (minDist < 12.0f) {
            float dX = CUBE_W - abs(n.x);
            float dY = CUBE_H - abs(n.y);
            float dZ = CUBE_D - abs(n.z);
            
            int wallsNear = 0;
            if (dX < 12.0f) wallsNear++;
            if (dY < 12.0f) wallsNear++;
            if (dZ < 12.0f) wallsNear++;
            
            if (wallsNear >= 2) {
                cornerContactCount++;
            } else if (dY == minDist) {
                if (n.y > 0) groundContactCount++; // 靠近 BOTTOM 地面
                else ceilingContactCount++;       // 靠近 TOP 天花板
            } else {
                wallContactCount++;               // 靠近侧面墙壁/前后屏
            }
        }
    }

    // 联合判定整体形态
    float avgPressure = totalPressure / MAX_NODES;
    if (avgPressure < 0.08f) {
        shapeArchetype = 0; // FREE (空中自由形态)
    } else if (cornerContactCount >= 2) {
        shapeArchetype = 4; // CORNER (角落史莱姆形态)
    } else if (groundContactCount >= ceilingContactCount && groundContactCount >= wallContactCount) {
        shapeArchetype = 1; // GROUND (底面融化水滴形态)
    } else if (ceilingContactCount >= groundContactCount && ceilingContactCount >= wallContactCount) {
        shapeArchetype = 3; // CEILING (悬挂液滴形态)
    } else {
        shapeArchetype = 2; // WALL (压扁鼻涕虫形态)
    }

    // 1. 每帧清零外部牵引力并恢复基础半径 (为后文的体积再分配作底)
    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].externalForce = 0;
        nodes[i].radius = nodes[i].baseRadius;
    }

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

    // 1. 计算所有接触墙面节点的投影重心，用于切向扩散力 (Surface Spread Force)
    float sumX = 0, sumY = 0, sumZ = 0;
    int contactCount = 0;
    for (int k = 0; k < MAX_NODES; k++) {
        if (nodes[k].contactPressure > 0.15f) {
            sumX += nodes[k].x;
            sumY += nodes[k].y;
            sumZ += nodes[k].z;
            contactCount++;
        }
    }
    float centerX = (contactCount > 0) ? (sumX / contactCount) : 0.0f;
    float centerY = (contactCount > 0) ? (sumY / contactCount) : 0.0f;
    float centerZ = (contactCount > 0) ? (sumZ / contactCount) : 0.0f;

    for (int i = 0; i < MAX_NODES; i++) {
        Node& n = nodes[i];
        
        // --- 1. 环境感应与应力计算 ---
        bool onWall = (abs(n.x) >= CUBE_W - STICK_DISTANCE || abs(n.y) >= CUBE_H - STICK_DISTANCE);

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
            
            // 【强制剥离逻辑】如果与父节点距离拉长过大，直接产生巨大应力，彻底拔起尾巴（不再受前一节点是否 stuck 的限制）
            if (dist > rest * 1.25f) {
                stressMag += 65.0f; 
            }
        }

        // 【力学重构：拖尾解粘门槛与主动爬行解耦】
        // 1. 质量估算：半径越大，质量越大，受重力拉扯越猛 (使用 baseRadius 防重算污染)
        float massFactor = n.baseRadius / NODE_RADIUS_BASE; 
        
        // 2. 吸附门槛优化：越往尾部的节点，吸附力越小（防止尾部被粘在原地拽不动）
        float tailAttenuation = 1.0f - ((float)i / MAX_NODES) * 0.75f; 
        float baseThreshold = UNSTICK_THRESHOLD * (1.5f - massFactor * 0.8f) * tailAttenuation;
        float dynamicThreshold = baseThreshold;

        // 3. 滞后效应：如果已经粘住了，需要 3.5 倍的力量才能撕下来 (比原本的 5.0 倍稍微温和，减少死锁)
        if (n.is_stuck) {
            dynamicThreshold *= 3.5f;
            // 【高空天花板粘性强化】：若处于高空，将门槛再度拔高 2.5 倍
            if (n.y < -CUBE_H * 0.40f) {
                dynamicThreshold *= 2.5f;
            }
        }

        // 4. 【核心突破】主动爬行牵引期门槛骤降：若前部节点有自主牵引目标，解粘门槛降至 20%，全力支持前行！
        bool hasActivePull = false;
        for (int k = 0; k <= i; k++) {
            if (hasTarget[k]) { hasActivePull = true; break; }
        }
        if (hasActivePull) {
            dynamicThreshold *= 0.20f; 
        }

        // 5. 顺次剥离波：如果前一个节点已经撕开，本节点脱粘门槛大幅降低
        if (i > 0 && !nodes[i-1].is_stuck) dynamicThreshold *= 0.15f; 
        
        bool underHeavyStress = (stressMag > dynamicThreshold); 
        
        float gMag = sqrtf(gx*gx + gy*gy + gz*gz);
        bool externalShock = (gMag > IMU_DETACH_FORCE); 
        
        // -------------------------------------------------------------
        // 【物理重构：静态防滑锁定机制】
        // -------------------------------------------------------------
        if (stiffnessMultiplier < 1.5f && !externalShock && !forceDetach) {
            underHeavyStress = false;
        }

        // --- 2. 剥离与吸附逻辑 (Peeling Logic) ---
        float suctionX = 0, suctionY = 0, suctionZ = 0;
        float nodeWeightFactor = 1.0f - ((float)i / MAX_NODES) * 0.7f; 
        float currentSuctionK = BASE_SUCTION_FORCE * nodeWeightFactor;
        
        if (i > 0 && !nodes[i-1].is_stuck) currentSuctionK *= (PEELING_SOFTEN_RATIO); 

        // [整合] 强力吸附逻辑：使用 forceMag = 15.5 进行贴合
        // 独立解算每个轴向的贴壁强力吸引力，忽略 Z 轴深度混淆，支持多向吸引
        float dX = CUBE_W - abs(n.x);
        float dY = CUBE_H - abs(n.y);
        float minDist = fminf(dX, dY); // 定义 minDist 供后面的切向扩散和各向异性粘附使用
        
        if (!forceDetach) {
            float velMag = sqrtf(n.vx*n.vx + n.vy*n.vy + n.vz*n.vz);
            float vAttractionScale = fmaxf(0.1f, 1.0f - velMag / 10.0f);
            
            if (dX < 15.0f) {
                float softZoneScale = (dX < 0.8f) ? (dX / 0.8f) : 1.0f;
                float strengthX = (1.0f - dX/15.0f) * 15.5f * vAttractionScale * softZoneScale;
                suctionX = (n.x > 0 ? 1 : -1) * strengthX;
            }
            if (dY < 15.0f) {
                float softZoneScale = (dY < 0.8f) ? (dY / 0.8f) : 1.0f;
                float strengthY = (1.0f - dY/15.0f) * 15.5f * vAttractionScale * softZoneScale;
                // 天花板高粘性强化
                if (n.y < 0) {
                    strengthY *= 2.0f;
                }
                suctionY = (n.y > 0 ? 1 : -1) * strengthY;
            }
        } else if (forceDetach) {
            if (abs(n.x) > CUBE_W - 5.0f) suctionX = (n.x > 0 ? (CUBE_W - n.x) : (-CUBE_W - n.x)) * currentSuctionK;
            if (abs(n.y) > CUBE_H - 5.0f) suctionY = (n.y > 0 ? (CUBE_H - n.y) : (-CUBE_H - n.y)) * currentSuctionK;
        }

        if (underHeavyStress || forceDetach) {
            suctionX *= PEELING_FORCE_DECAY; suctionY *= PEELING_FORCE_DECAY; suctionZ *= PEELING_FORCE_DECAY;
        }
        n.vx += suctionX; n.vy += suctionY; n.vz += suctionZ;

        // --- 3. 状态更新与物理积分 ---
        bool canStickState = (dampingMultiplier < 0.7f || dampingMultiplier > 0.9f);
        
        bool extremelyHeavyStress = underHeavyStress && (stressMag > dynamicThreshold * 1.5f);
        bool slidingMode = underHeavyStress && !extremelyHeavyStress && !externalShock && !forceDetach && n.is_stuck;

        if (onWall && !externalShock && canStickState && (!underHeavyStress || slidingMode) && !isSwinging) {
            n.is_stuck = true; 
            n.lastStuckTime = millis();
            
            if (slidingMode) {
                n.vx = n.vx * 0.10f + gx * 0.02f;
                n.vy = n.vy * 0.10f + gy * 0.02f;
                n.vz = n.vz * 0.10f + gz * 0.02f;
            } else {
                n.vx = 0; n.vy = 0; n.vz = 0; 
            }
        } else {
            // 【剥离弹射力学】
            if (n.is_stuck) {
                if (i > 0) {
                    float dx = nodes[i-1].x - n.x;
                    float dy = nodes[i-1].y - n.y;
                    float dz = nodes[i-1].z - n.z;
                    float d = sqrtf(dx*dx + dy*dy + dz*dz + 0.001f);
                    
                    float kickForce = underHeavyStress ? 3.8f : 1.5f; 
                    n.vx += (dx / d) * kickForce;
                    n.vy += (dy / d) * kickForce;
                    n.vz += (dz / d) * kickForce;
                }
            }
            n.is_stuck = false;
            
            uint32_t timeSinceStuck = millis() - n.lastStuckTime;
            float viscousResist = (timeSinceStuck < 1500) ? 0.98f : ADHESION_THRESHOLD;
            float gravityResist = (dampingMultiplier < 0.6f) ? 0.98f : viscousResist;
            
            float resist = (isFalling) ? ((millis() - fallingTimer < FALLING_STRUGGLE_MS) ? 0.99f : 0.4f) : gravityResist;
            
            float gPull = (1.0f - resist) * (0.2f + massFactor * 1.6f);
            
            float effGx = gx;
            float effGy = gy;
            if (abs(gz) > 7.0f) {
                effGx = 0.0f;
                effGy = 9.8f;
            }
            
            float swingGPull = gPull;
            if (isSwinging) {
                swingGPull = 0.45f; 
            }
            n.vx += effGx * swingGPull; n.vy += effGy * swingGPull; n.vz += gz * swingGPull;
            
            if (isSwinging) {
                n.vx *= 0.82f; 
                n.vy *= 0.82f;
                n.vz *= 0.82f;
            } else {
                n.vx *= SPRING_DAMPING * dampingMultiplier;
                n.vy *= SPRING_DAMPING * dampingMultiplier;
                n.vz *= SPRING_DAMPING * dampingMultiplier;
            }
 
            if (dampingMultiplier < 0.6f && (n.vx*n.vx + n.vy*n.vy + n.vz*n.vz) < 0.02f) {
                n.vx = 0; n.vy = 0; n.vz = 0;
            }
        }

        // -------------------------------------------------------------
        // 【新物理层：切向扩散力 (Surface Spread Force) 实现】
        // -------------------------------------------------------------
        if (n.contactPressure > 0.15f && contactCount > 1) {
            float toNodeX = n.x - centerX;
            float toNodeY = n.y - centerY;
            float toNodeZ = n.z - centerZ;
            
            float normalX = 0, normalY = 0, normalZ = 0;
            if (minDist == dX) normalX = (n.x > 0) ? 1.0f : -1.0f;
            else if (minDist == dY) normalY = (n.y > 0) ? 1.0f : -1.0f;
            else normalZ = (n.z > 0) ? 1.0f : -1.0f;
            
            // 投影至切面上
            float dot = toNodeX * normalX + toNodeY * normalY + toNodeZ * normalZ;
            float tX = toNodeX - dot * normalX;
            float tY = toNodeY - dot * normalY;
            float tZ = toNodeZ - dot * normalZ;
            
            float len = sqrtf(tX*tX + tY*tY + tZ*tZ + 0.001f);
            tX /= len; tY /= len; tZ /= len;
            
            float spreadF = n.contactPressure * SPREAD_FORCE_MAG;
            n.vx += tX * spreadF;
            n.vy += tY * spreadF;
            n.vz += tZ * spreadF;
        }

        // -------------------------------------------------------------
        // 【新物理层：各向异性粘附投影 (Anisotropic Adhesion) 实现】
        // -------------------------------------------------------------
        if (minDist < 6.0f && !forceDetach) {
            float normalX = 0, normalY = 0, normalZ = 0;
            if (minDist == dX) normalX = (n.x > 0) ? 1.0f : -1.0f;
            else if (minDist == dY) normalY = (n.y > 0) ? 1.0f : -1.0f;
            else normalZ = (n.z > 0) ? 1.0f : -1.0f;
            
            float v_dot_n = n.vx * normalX + n.vy * normalY + n.vz * normalZ;
            float v_nX = v_dot_n * normalX;
            float v_nY = v_dot_n * normalY;
            float v_nZ = v_dot_n * normalZ;
            
            float v_tX = n.vx - v_nX;
            float v_tY = n.vy - v_nY;
            float v_tZ = n.vz - v_nZ;
            
            float kNormal = 0.05f;  // 垂直墙面方向极高阻抗，消除脱离和高频抖动
            float kTangent = 0.96f; // 沿墙滑动阻力极低
            
            if (n.is_stuck && !slidingMode) {
                kTangent = 0.15f; // stuck 时限制切向，使其极难滑移以固形
            }
            
            n.vx = v_nX * kNormal + v_tX * kTangent;
            n.vy = v_nY * kNormal + v_tY * kTangent;
            n.vz = v_nZ * kNormal + v_tZ * kTangent;
        }

        // 限制速度上限
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

    // -------------------------------------------------------------
    // 【新物理层：基于接触压力的体积再分配 (Volume Redistribution)】
    // -------------------------------------------------------------
    float sumPressure = 0.0f;
    int freeNodesCount = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        sumPressure += nodes[i].contactPressure;
        if (nodes[i].contactPressure < 0.20f) {
            freeNodesCount++;
        }
    }
    
    // 膨胀补偿强度
    float volumeCompensation = sumPressure * 0.16f; 
    
    for (int i = 0; i < MAX_NODES; i++) {
        Node& n = nodes[i];
        if (n.contactPressure < 0.20f && freeNodesCount > 0) {
            // 远离接触面的背部/悬空节点获得膨胀补偿
            n.radius = n.baseRadius * (1.0f + (volumeCompensation / freeNodesCount) * 4.5f);
            // 限制最大膨胀上限为 1.45 倍，防止过度畸变
            if (n.radius > n.baseRadius * 1.45f) n.radius = n.baseRadius * 1.45f;
        } else {
            // 接触面上的节点物理半径保持饱满，不进行过度收缩 (至多收缩 4%)
            n.radius = n.baseRadius * (1.0f - n.contactPressure * 0.04f);
        }
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
        else if (isSwinging) restDist *= 0.45f; // 荡秋千状态：极致压缩身体节段间距，抵消强吊挂拉力以保型

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
                    if (isSwinging && delta > 0) {
                        force = delta * stiffness * 4.5f * forceScale; // 荡秋千状态强力保型，将内部刚度提至4.5倍
                    } else {
                        force = delta * stiffness * 0.8f * forceScale; 
                    }
                }
            }
            
            // 严格限制最大力输出，防止系统发散
            float maxForce = (stiffnessMultiplier > 1.1f || isSwinging) ? 80.0f : 20.0f; 
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
                // 【新物理层：非对称生物拖尾质量分配】
                // 头部更轻(0.7x)，尾部更重(2.2x)，实现凌厉的头部探索与滞后性极强、长丝流体感的拖尾！
                float massScale_i = (i <= 2) ? 0.7f : (i >= 7 ? 2.2f : 1.0f);
                float massScale_prev = ((i-1) <= 2) ? 0.7f : ((i-1) >= 7 ? 2.2f : 1.0f);
                
                m_i = (1.0f + ((float)i / MAX_NODES) * 1.5f) * massScale_i;
                m_prev = (1.0f + ((float)(i-1) / MAX_NODES) * 1.5f) * massScale_prev;
            }

            // 【核心物理重构：粘弹性拉伸松弛】
            // 毒液作为高粘稠流体，在被拉伸挂墙时，其内部骨骼弹簧刚度应当产生“应力松弛 (Stress Relaxation)”。
            // 如果处于拉伸状态 (delta > 0) 且并非瞬间爆发态 (stiffnessMultiplier < 1.5f)，我们将刚度维持力减弱 45% (relaxation = 0.55f)，
            // 这给流体全身骨架提供了极好的柔韧延展与松弛形变空间，是消除僵硬钢丝感的第一步！
            float relaxation = 1.0f;
            if (delta > 0 && stiffnessMultiplier < 1.5f && !isSwinging) {
                relaxation = 0.55f;
            }
            
            // 【新物理层：自适应接触面刚度松弛 (Stiffness Relaxation)】
            // 当相邻节点贴在墙壁上受压时，弹簧恢复刚度会随着压力自适应衰减 (最低可降至 10%)
            float avgPressure = (nodes[i-1].contactPressure + nodes[i].contactPressure) * 0.5f;
            if (avgPressure > 0.15f) {
                relaxation *= (1.0f - avgPressure * 0.85f);
            }
            
            float finalForce = force * relaxation;

            nodes[i].vx += ((dx / dist) * finalForce) / m_i;
            nodes[i].vy += ((dy / dist) * finalForce) / m_i;
            nodes[i].vz += ((dz / dist) * finalForce) / m_i;
            
            nodes[i-1].vx -= ((dx / dist) * finalForce * backPullScale) / m_prev;
            nodes[i-1].vy -= ((dy / dist) * finalForce * backPullScale) / m_prev;
            nodes[i-1].vz -= ((dz / dist) * finalForce * backPullScale) / m_prev;

            // 【核心物理重构：重力悬链线下坠补偿 (Catenary Sag Impulse)】
            // 解决“被粘在侧壁上时，拉伸的身体是一条极反物理的绝对直线”！
            // 如果节点处于拉伸挂墙态且未被 stuck 焊死，我们沿着真实的重力方向注入一个与拉伸量成正比的沉坠冲量，
            // 这模拟了黏糊糊、沉甸甸的沥青胶体顺着重力方向在半空中向下坠落、弯曲成优美悬链线弧形的高保真视觉力学！
            if (delta > 0 && stiffnessMultiplier < 1.5f) {
                float sagWeight = (delta / restDist) * 0.16f; // 与拉伸强度正相关的下坠系数
                if (!nodes[i].is_stuck) {
                    nodes[i].vx += lastGx * sagWeight;
                    nodes[i].vy += lastGy * sagWeight;
                    nodes[i].vz += lastGz * sagWeight;
                }
                if (!nodes[i-1].is_stuck) {
                    nodes[i-1].vx += lastGx * sagWeight;
                    nodes[i-1].vy += lastGy * sagWeight;
                    nodes[i-1].vz += lastGz * sagWeight;
                }
            }

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
                float fx = 0.0f, fy = 0.0f, fz = 0.0f;
                float fMag = 0.0f;
                
                // -------------------------------------------------------------
                // 【物理重构：秋千悬吊手臂拉长与主体保型机制】
                // -------------------------------------------------------------
                // 当处于荡秋千 swing 状态时，头部与挂载点之间被重塑为一个具有 35.0f 像素 rest 臂长的弹簧！
                // 这将巨大的拉扯形变负荷彻底交由手臂（Bezier 曲线）承担，而让头节点 Node 0 和主体重新维持饱满短小圆润，杜绝过度拉长穿模！
                if (isSwinging && i == 0) {
                    const float armRest = 35.0f; // 手臂的物理悬吊静止长度
                    if (dist > armRest) {
                        float extension = dist - armRest;
                        // 精细调校刚度系数，使得悬挂状态既有张力又十分稳固
                        float k = targetStrength[i] * stiffnessMultiplier * 0.015f; 
                        fx = (dx / dist) * extension * k * 35.0f; 
                        fy = (dy / dist) * extension * k * 35.0f; 
                        fz = (dz / dist) * extension * k * 35.0f; 
                    }
                    fMag = sqrtf(fx*fx + fy*fy + fz*fz);
                } else {
                    // 正常抓墙/移动锁定引力（静态 rest 长度为 0）
                    float k = targetStrength[i] * stiffnessMultiplier * 0.008f; 
                    fx = dx * k;
                    fy = dy * k;
                    fz = dz * k;
                    
                    float fMagSq = fx*fx + fy*fy + fz*fz;
                    const float maxF = 150.0f; 
                    fMag = sqrtf(fMagSq);
                    if (fMag > maxF) {
                        float s = maxF / fMag;
                        fx *= s; fy *= s; fz *= s;
                        fMag = maxF;
                    }
                }
                
                nodes[i].vx += fx;
                nodes[i].vy += fy;
                nodes[i].vz += fz;
                // 【关键同步】牵引力存入节点，供后续剥离逻辑使用
                nodes[i].externalForce = fMag; 
            }
        }
    }
}

void Skeleton::applyConstraints() {
    const float border = 0.5f; 
    for (int i = 0; i < MAX_NODES; i++) {
        Node& n = nodes[i];
        
        bool heavyImpact = false;
        const float bounce = 0.3f; 
        const float impactThreshold = 2.5f; // 必须有明显的速度撞击才算着陆
        if (n.x < -CUBE_W) { 
            n.x = -CUBE_W; 
            maxCollisionSpeed = fmaxf(maxCollisionSpeed, abs(n.vx));
            if (n.vx < -impactThreshold) heavyImpact = true; 
            n.vx = abs(n.vx) * bounce; 
        }
        if (n.x >  CUBE_W) { 
            n.x =  CUBE_W; 
            maxCollisionSpeed = fmaxf(maxCollisionSpeed, abs(n.vx));
            if (n.vx >  impactThreshold) heavyImpact = true; 
            n.vx = -abs(n.vx) * bounce; 
        }
        if (n.y < -CUBE_H) { 
            n.y = -CUBE_H; 
            maxCollisionSpeed = fmaxf(maxCollisionSpeed, abs(n.vy));
            if (n.vy < -impactThreshold) heavyImpact = true; 
            n.vy = -abs(n.vy) * bounce; 
        }
        if (n.y >  CUBE_H) { 
            n.y =  CUBE_H; 
            maxCollisionSpeed = fmaxf(maxCollisionSpeed, abs(n.vy));
            if (n.vy >  impactThreshold) heavyImpact = true; 
            n.vy = -abs(n.vy) * bounce; 
        }
        if (n.z < -CUBE_D) { 
            n.z = -CUBE_D; 
            maxCollisionSpeed = fmaxf(maxCollisionSpeed, abs(n.vz));
            if (n.vz < -impactThreshold) heavyImpact = true; 
            n.vz = abs(n.vz) * bounce; 
        }
        if (n.z >  CUBE_D) { 
            n.z =  CUBE_D; 
            maxCollisionSpeed = fmaxf(maxCollisionSpeed, abs(n.vz));
            if (n.vz >  impactThreshold) heavyImpact = true; 
            n.vz = -abs(n.vz) * bounce; 
        }

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

    // -------------------------------------------------------------
    // 【物理重构：双重迭代全局刚性距离投影解算器 (PBD Solver v6.7)】
    // -------------------------------------------------------------
    // 通过 2 次几何坐标投影迭代（Double-Iteration Solver），确保骨骼绝对不产生数值发散和拉伸。
    // 强制限制相邻节点的最大物理空间距离：
    // - 荡秋千悬挂 (isSwinging) 状态下：严格压死在 3.8f 像素内，使得身体在渲染层绝对保型为超萌的圆形黑糯米滋。
    // - 其它状态 (Move/Idle 等) 状态下：强制锁在 6.8f 像素内，拉伸量 100% 被触手承担，身体绝对拒绝任何面条状拉长！
    float maxLimitDist = isSwinging ? 3.8f : 6.8f; 
    for (int iter = 0; iter < 2; iter++) {
        for (int i = 1; i < MAX_NODES; i++) {
            float dx = nodes[i].x - nodes[i-1].x;
            float dy = nodes[i].y - nodes[i-1].y;
            float dz = nodes[i].z - nodes[i-1].z;
            float d = sqrtf(dx*dx + dy*dy + dz*dz);
            if (d > maxLimitDist && d > 0.01f) {
                // 几何投影：强行将子节点坐标拉回至 maxLimitDist 刚性边界
                nodes[i].x = nodes[i-1].x + (dx / d) * maxLimitDist;
                nodes[i].y = nodes[i-1].y + (dy / d) * maxLimitDist;
                nodes[i].z = nodes[i-1].z + (dz / d) * maxLimitDist;
                
                // 动能衰减与能量分摊，消除残余的物理惯性
                nodes[i].vx = nodes[i-1].vx * 0.70f;
                nodes[i].vy = nodes[i-1].vy * 0.70f;
                nodes[i].vz = nodes[i-1].vz * 0.70f;
            }
        }
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
