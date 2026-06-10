#ifndef SKILLS_H
#define SKILLS_H

#include <Arduino.h>
#include "config.h"

struct SkillParams {
    float speed;
    float tension;
    float eyeFocus;
    float damping;
    float pause;
    float pullScale;
};

inline SkillParams getSkillParams(String name, float recoveryProgress = 1.0f) {
    // V3 重构：8 大 Base Skills 底层基础物理刚度与阻尼系数
    if (name == "rest") 
        return {0.2f, IDLE_TENSION_BASE, 0.1f, IDLE_DAMPING_BASE, 4500.0f, 0.5f};
    if (name == "move") 
        return {1.0f, 1.2f, 0.4f, 0.95f, 400.0f, 1.0f};
    if (name == "observe") 
        return {0.3f, 1.5f, 0.9f, 0.4f, 2500.0f, 0.4f};
    if (name == "hide") 
        return {0.7f, 0.8f, 0.0f, 0.8f, 5000.0f, 0.6f};
    if (name == "explore") 
        return {0.8f, 1.1f, 0.5f, 0.85f, 1200.0f, 0.8f};
    if (name == "express") 
        return {0.5f, 1.6f, 0.7f, 0.75f, 1500.0f, 0.7f};
    if (name == "interact") 
        return {0.7f, 1.3f, 0.8f, 0.8f, 1200.0f, 0.8f};
    if (name == "recover") 
        return {0.15f, 0.9f + recoveryProgress * 0.2f, 0.2f, 0.4f + recoveryProgress * 0.5f, 3000.0f, 0.4f};
    
    // 应急/遗留指令默认参数
    return {0.3f, 1.0f, 0.2f, 0.8f, 2000.0f, 0.5f};
}

#endif
