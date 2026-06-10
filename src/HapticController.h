#ifndef HAPTIC_CONTROLLER_H
#define HAPTIC_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

class HapticController {
public:
    HapticController();
    void init(int pin);
    void triggerImpact(float collisionSpeed);
    void setContinuousCrawl(bool crawling, float speedMult);
    void update();

private:
    int motorPin;
    bool initialized;
    
    // 碰撞震动状态
    float impactIntensity;
    uint32_t impactStartTime;
    uint32_t impactDuration;
    
    // 蠕动震动状态
    bool isCrawling;
    float speedMultiplier;
};

#endif
