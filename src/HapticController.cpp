#include "HapticController.h"

// 自适应检测 ESP32 Arduino Core 版本，提供最佳的 API 桥接
#ifdef ESP_ARDUINO_VERSION
#define HAS_LEDC_ATTACH (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
#else
#define HAS_LEDC_ATTACH 0
#endif

HapticController::HapticController() 
    : motorPin(PIN_MOTOR), initialized(false), 
      impactIntensity(0.0f), impactStartTime(0), impactDuration(0),
      isCrawling(false), speedMultiplier(1.0f) {}

void HapticController::init(int pin) {
    motorPin = pin;
    
#if HAS_LEDC_ATTACH
    // ESP32 Arduino Core 3.x 推荐的高频 supersonic (20kHz) PWM 绑定
    // 8-bit 分辨率支持 0-255 占空比，高频不仅消除了电机线圈哨叫声，也利用感抗使得电流变化更平滑，消除了屏幕闪烁
    ledcAttach(motorPin, 20000, 8);
    ledcWrite(motorPin, 0);
#else
    // 兼容 2.x 版本的高频 PWM 初始化
    ledcSetup(0, 20000, 8); // 通道0, 20kHz, 8位分辨率
    ledcAttachPin(motorPin, 0);
    ledcWrite(0, 0);
#endif

    initialized = true;
}

void HapticController::triggerImpact(float speed) {
    if (speed < COLLISION_SPEED_MIN) return;
    
    // 线性映射速度到强度百分比
    float t = (speed - COLLISION_SPEED_MIN) / (COLLISION_SPEED_MAX - COLLISION_SPEED_MIN);
    t = fmaxf(0.0f, fminf(1.0f, t));
    
    float peakIntensity = MOTOR_DUTY_MIN + t * (MOTOR_DUTY_MAX - MOTOR_DUTY_MIN);
    uint32_t duration = IMPACT_DECAY_MS + (uint32_t)(t * 180.0f); // 120ms 到 300ms
    
    // 只有当新的撞击强度大于当前残余撞击强度时才覆盖，防止重叠衰减导致的杂音
    if (peakIntensity > impactIntensity) {
        impactIntensity = peakIntensity;
        impactStartTime = millis();
        impactDuration = duration;
    }
}

void HapticController::setContinuousCrawl(bool crawling, float speedMult) {
    isCrawling = crawling;
    speedMultiplier = speedMult;
}

extern bool has_dlight;

void HapticController::update() {
    // 【硬件引脚冲突自动解决】Hat DLight 和 Hat Vibrator 都占用 G0 引脚。
    // 若系统已成功探测并初始化了 DLight（I2C），则彻底禁用震动模块的 PWM 信号输出，防止干扰 I2C 总线导致系统极度卡顿。
    if (has_dlight) return; 

    if (!initialized) {
        init(PIN_MOTOR);
    }
    
    uint32_t now = millis();
    float currentImpact = 0.0f;
    
    // 1. 计算碰撞衰减震动 (二次方曲线)
    if (impactIntensity > 0.0f) {
        uint32_t elapsed = now - impactStartTime;
        if (elapsed < impactDuration) {
            float decay = 1.0f - ((float)elapsed / (float)impactDuration);
            currentImpact = impactIntensity * (decay * decay); // 二次方包络，前陡后缓，极其清脆
        } else {
            impactIntensity = 0.0f;
        }
    }
    
    // 2. 计算有机蠕动轻微震动 (多频叠加模拟生物流体活动)
    float currentCrawl = 0.0f;
    if (isCrawling) {
        // 时间轴加入爬行速度倍率的微调，运动越快，“呼吸率”越高
        float timeFactor = (float)now * 0.006f * (0.5f + speedMultiplier * 0.5f);
        float organicWave = sinf(timeFactor) * 0.7f + sinf(timeFactor * 2.3f) * 0.3f; // 复合谐波
        
        // 蠕动震动基础大小与摆动范围
        float baseDuty = 38.0f + speedMultiplier * 10.0f; // 40-50 占空比，极其轻微
        float amplitude = 6.0f;
        
        currentCrawl = baseDuty + organicWave * amplitude;
    }
    
    // 3. 非线性融合 (取最大值) 并输出到硬件
    float finalIntensity = fmaxf(currentImpact, currentCrawl);
    int finalDuty = (int)fmaxf(0.0f, fminf((float)MOTOR_DUTY_MAX, finalIntensity));
    
#if HAS_LEDC_ATTACH
    ledcWrite(motorPin, finalDuty);
#else
    ledcWrite(0, finalDuty);
#endif
}
