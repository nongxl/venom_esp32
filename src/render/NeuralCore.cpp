#include "NeuralCore.h"
#include "../Noise.h"
#include <math.h>

NeuralCore::NeuralCore() 
    : currentState(NeuralState::Idle),
      coreRadius(BASE_CORE_R),
      targetCoreRadius(BASE_CORE_R),
      coreVelocity(0.0f),
      rippleProgress(0.0f),
      lastTokenTime(0),
      tokenPulseCount(0),
      lastClusterRegenTime(0),
      clusterSeed(0.0f),
      stateTimer(0),
      lastUpdateTime(0) 
{
    // 均匀分布 32 根尖刺的角度，并初始化物理参数
    for (int i = 0; i < 32; i++) {
        spines[i].angle = i * (2.0f * M_PI / 32.0f);
        spines[i].length = BASE_SPINE_L;
        spines[i].targetLength = BASE_SPINE_L;
        spines[i].velocity = 0.0f;
    }
    lastUpdateTime = millis();
    lastClusterRegenTime = millis();
}

void NeuralCore::setState(NeuralState newState) {
    if (currentState == newState) return;
    
    currentState = newState;
    stateTimer = millis();
    
    // 状态切换时的特殊初始化
    if (newState == NeuralState::Streaming) {
        lastTokenTime = millis();
        rippleProgress = 0.0f;
        tokenPulseCount = 0;
    } else if (newState == NeuralState::Thinking) {
        lastClusterRegenTime = millis();
        clusterSeed = (float)random(100);
    }
}

void NeuralCore::notifyAISyncStarted() {
    // 只能在非失联情况下转入发送中
    if (currentState != NeuralState::Offline) {
        setState(NeuralState::Transmitting);
    }
}

void NeuralCore::notifyAIResponseReceived() {
    // 只能在非失联情况下转入流式
    if (currentState != NeuralState::Offline) {
        setState(NeuralState::Streaming);
    }
}

void NeuralCore::update(float soundLevel, float lux, bool isWifiConnected) {
    uint32_t now = millis();
    lastUpdateTime = now;

    // --- 1. 网络离线自愈判定 (失联状态最高优先级) ---
    if (!isWifiConnected) {
        if (currentState != NeuralState::Offline) {
            setState(NeuralState::Offline);
        }
    } else {
        if (currentState == NeuralState::Offline) {
            setState(NeuralState::Idle); // 恢复网络后切回闲置
        }
    }

    float time = now * 0.001f;
    float flowAngle = time * 1.6f; // Transmitting 状态下的漂移水流基准角

    // --- 2. 状态机 Procedural 波波参数解算 ---
    switch (currentState) {
        case NeuralState::Offline: {
            // 所有尖刺极速收缩塌缩，核心缩水
            for (int i = 0; i < 32; i++) {
                spines[i].targetLength = 0.0f;
            }
            targetCoreRadius = BASE_CORE_R * 0.55f;
            break;
        }

        case NeuralState::Idle: {
            // 缓慢心跳呼吸与 Perlin 细微抖动
            float breathing = sinf(time * 1.4f) * 0.6f;
            targetCoreRadius = BASE_CORE_R + breathing * 0.25f;

            for (int i = 0; i < 32; i++) {
                // 用 Perlin 噪声生成极具生命机理的起伏
                float noise = FastNoise::perlin(time * 0.5f, spines[i].angle * 1.5f, 0.0f) * 0.9f;
                spines[i].targetLength = BASE_SPINE_L + breathing * 0.4f + noise;
            }
            break;
        }

        case NeuralState::Listening: {
            // 尖刺随声音剧烈蠕动伸展，核心轻微膨胀
            targetCoreRadius = BASE_CORE_R + soundLevel * 1.5f;
            for (int i = 0; i < 32; i++) {
                float voiceFluct = sinf(time * 24.0f + i * 0.8f) * (soundLevel * 1.2f);
                spines[i].targetLength = BASE_SPINE_L + soundLevel * 5.0f + voiceFluct;
            }

            // 静音 800ms 后平滑回退
            if (soundLevel <= 0.05f && (now - stateTimer > 800)) {
                setState(NeuralState::Idle);
            }
            break;
        }

        case NeuralState::Transmitting: {
            // 海葵受水流偏折效果：尖刺向统一漂移角摆动
            targetCoreRadius = BASE_CORE_R * 0.9f; // 稍紧绷收缩
            for (int i = 0; i < 32; i++) {
                float diff = flowAngle - spines[i].angle;
                float flowAlign = cosf(diff); // 顺水流较长，逆水流缩短
                spines[i].targetLength = BASE_SPINE_L * (1.1f + flowAlign * 0.35f);
            }

            // 兜底超时 2.5 秒，自动进入思考
            if (now - stateTimer > 2500) {
                setState(NeuralState::Thinking);
            }
            break;
        }

        case NeuralState::Thinking: {
            // 尖刺重组为动态神经簇 (意识核心亮点)
            if (now - lastClusterRegenTime > 3000) {
                lastClusterRegenTime = now;
                clusterSeed = (float)random(100);
            }
            // 核心呼吸
            targetCoreRadius = BASE_CORE_R + sinf(time * 2.8f) * 0.4f;

            for (int i = 0; i < 32; i++) {
                // 用 Perlin 控制神经簇聚合分布 (周期 2.5)
                float clusterVal = FastNoise::perlin(time * 1.3f + spines[i].angle * 2.5f, clusterSeed, 0.0f);
                spines[i].targetLength = BASE_SPINE_L * (0.8f + (clusterVal + 1.0f) * 0.9f);
            }
            break;
        }

        case NeuralState::Streaming: {
            // 模拟收到流式 Token：每 180ms 产生一次中心爆裂扩散波纹
            if (now - lastTokenTime > 180) {
                lastTokenTime = now;
                rippleProgress = 0.0f;
                tokenPulseCount++;
                if (tokenPulseCount > 12) {
                    setState(NeuralState::Complete);
                    break;
                }
            }

            // 波纹前推
            rippleProgress += 0.065f;

            // 核心吹胀回弹：在波纹初期膨胀，后期坍缩
            if (rippleProgress < 0.25f) {
                targetCoreRadius = BASE_CORE_R * 1.35f;
            } else {
                targetCoreRadius = BASE_CORE_R * 0.95f;
            }

            for (int i = 0; i < 32; i++) {
                float wave = 0.0f;
                // 波纹在末期传导给尖刺顶端，使尖刺瞬间向前飞刺
                if (rippleProgress >= 0.65f && rippleProgress <= 0.95f) {
                    wave = 1.0f - abs(rippleProgress - 0.8f) / 0.15f;
                }
                spines[i].targetLength = BASE_SPINE_L * (1.0f + wave * 1.6f);
            }
            break;
        }

        case NeuralState::Complete: {
            // 爆发现成：全部尖刺瞬间暴涨
            targetCoreRadius = BASE_CORE_R * 1.2f;
            for (int i = 0; i < 32; i++) {
                spines[i].targetLength = BASE_SPINE_L * 2.1f;
            }

            // 持续 250ms 后退回 Idle
            if (now - stateTimer > 250) {
                setState(NeuralState::Idle);
            }
            break;
        }
    }

    // --- 3. [Listening 软触发] 若在 Idle 且有环境噪音直接唤醒 Listening ---
    if (currentState == NeuralState::Idle && soundLevel > 0.05f) {
        setState(NeuralState::Listening);
    }

    // --- 4. 尖刺与核心胡克弹簧动力学物理迭代 ---
    for (int i = 0; i < 32; i++) {
        spines[i].velocity += (spines[i].targetLength - spines[i].length) * SPRING_SPINE;
        spines[i].length += spines[i].velocity;
        spines[i].velocity *= DAMPING_SPINE;
        
        if (spines[i].length < 0.0f) spines[i].length = 0.0f;
    }

    coreVelocity += (targetCoreRadius - coreRadius) * SPRING_CORE;
    coreRadius += coreVelocity;
    coreVelocity *= DAMPING_CORE;
    if (coreRadius < 1.0f) coreRadius = 1.0f;
}

void NeuralCore::draw(M5Canvas* canvas, int cx, int cy) {
    uint32_t now = millis();
    float time = now * 0.001f;
    float flowAngle = time * 1.6f;

    // 颜色系统定义
    uint16_t colBody   = 0x0000; // 纯黑共生体
    uint16_t colShadow = 0x18C3; // 深邃海葵阴影 (暗蓝色)
    uint16_t colGloss  = 0x5AEB; // 荧光微光泽 (亮冰蓝)

    // --- 1. [第一阶段] 绘制深色轮廓与阴影描边 (轮廓层) ---
    // 核心球体软阴影
    canvas->fillCircle(cx, cy, (int)(coreRadius + 1.2f), colShadow);

    // 尖刺阴影描边 (偏斜 1px 先画一遍)
    for (int i = 0; i < 32; i++) {
        if (spines[i].length < 0.1f) continue;
        
        float renderAngle = spines[i].angle;
        if (currentState == NeuralState::Transmitting) {
            float diff = flowAngle - spines[i].angle;
            renderAngle += sinf(diff) * 0.28f; // 偏转
        }

        // 终点定位
        float len = coreRadius + spines[i].length;
        float ex = cx + cosf(renderAngle) * len;
        float ey = cy + sinf(renderAngle) * len;

        // 左右偏置 1px 绘制深蓝阴影包络，达成完美的手绘深色厚重轮廓！
        canvas->drawLine((int)cx - 1, (int)cy, (int)ex - 1, (int)ey, colShadow);
        canvas->drawLine((int)cx + 1, (int)cy, (int)ex + 1, (int)ey, colShadow);
        canvas->drawLine((int)cx, (int)cy - 1, (int)ex, (int)ey - 1, colShadow);
        canvas->drawLine((int)cx, (int)cy + 1, (int)ex, (int)ey + 1, colShadow);
    }

    // --- 2. [第二阶段] 流式中间态荧光波纹层 (置于轮廓上、黑色核心下) ---
    if (currentState == NeuralState::Streaming && rippleProgress >= 0.25f && rippleProgress <= 0.75f) {
        float r_ripple = coreRadius + (rippleProgress - 0.25f) / 0.5f * BASE_SPINE_L * 1.5f;
        // 荡开一圈泛光亮蓝环
        canvas->drawCircle(cx, cy, (int)r_ripple, colGloss);
    }

    // --- 3. [第三阶段] 绘制黑色共生体尖刺主体 ---
    for (int i = 0; i < 32; i++) {
        if (spines[i].length < 0.1f) continue;
        
        float renderAngle = spines[i].angle;
        if (currentState == NeuralState::Transmitting) {
            float diff = flowAngle - spines[i].angle;
            renderAngle += sinf(diff) * 0.28f;
        }

        float len = coreRadius + spines[i].length;
        float ex = cx + cosf(renderAngle) * len;
        float ey = cy + sinf(renderAngle) * len;

        // 纯黑线段
        canvas->drawLine(cx, cy, (int)ex, (int)ey, colBody);

        // 如果在流式冲击爆燃期，在尖刺端顶闪烁晶莹的荧光像素！
        if (currentState == NeuralState::Streaming && rippleProgress >= 0.70f && rippleProgress <= 0.90f) {
            canvas->drawPixel((int)ex, (int)ey, colGloss);
        }
    }

    // --- 4. [第四阶段] 绘制黑色共生体核心球体与极致高光 ---
    canvas->fillCircle(cx, cy, (int)coreRadius, colBody);

    // 失联状态下不产生反光高光
    if (currentState != NeuralState::Offline) {
        // 左上方绘制饱满的亮蓝色光泽
        canvas->fillCircle(cx - 2, cy - 2, 1, colGloss);
        // 高反光白色亮斑点缀
        canvas->drawPixel(cx - 3, cy - 3, 0xFFFF);
    }
}
