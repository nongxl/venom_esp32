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
    if (name == "idle") 
        return {0.3f, IDLE_TENSION_BASE, 0.1f, IDLE_DAMPING_BASE, 4000.0f, 0.6f};
    if (name == "crawl_edge" || name == "explore") 
        return {1.2f, 1.3f, 0.4f, 0.96f, 300.0f, 1.1f};
    if (name == "observe_user" || name == "track_observer") 
        return {0.4f, 1.6f, 0.95f, 0.4f, 2500.0f, 0.5f};
    if (name == "curious_probe") 
        return {0.7f, 1.2f, 0.8f, 0.75f, 1200.0f, 0.8f};
    if (name == "panic") 
        return {4.0f, 2.2f, 1.0f, 0.95f, 100.0f, 1.8f};
    if (name == "hiding" || name == "camouflage") 
        return {0.8f, 0.8f, 0.0f, 0.8f, 5000.0f, 0.7f};
    if (name == "sleep") 
        return {0.05f, 0.6f, 0.0f, 0.3f, 10000.0f, 0.3f};
    if (name == "recovery") 
        return {0.15f, 0.9f + recoveryProgress * 0.2f, 0.15f, 0.4f + recoveryProgress * 0.5f, 3000.0f, 0.4f};
    if (name == "alert") 
        return {0.0f, 2.4f, 1.0f, 0.4f, 2000.0f, 0.0f};
    if (name == "cling") 
        return {0.2f, 0.8f, 0.5f, 0.6f, 3500.0f, 0.4f};
    if (name == "grooming") 
        return {0.35f, 1.2f, 0.3f, 0.85f, 1800.0f, 0.6f};
    if (name == "startled") 
        return {STARTLED_SPEED_MULT, STARTLED_TENSION, 1.0f, 0.92f, 50.0f, 3.0f};
    if (name == "warning") 
        return {8.0f, 3.2f, 1.0f, 0.98f, 200.0f, 2.5f};
    if (name == "hesitation") 
        return {0.5f, 1.9f, 0.9f, 0.7f, 1500.0f, 0.9f};
    if (name == "mimicry") 
        return {0.65f, 1.5f, 0.7f, 0.8f, 800.0f, 0.8f};
    if (name == "silent_watch") 
        return {0.05f, 1.3f, 0.98f, 0.4f, 15000.0f, 0.2f};
    if (name == "trust_observe") 
        return {0.3f, 0.7f, 0.8f, 0.5f, 4500.0f, 0.4f};
    
    return {1.0f, 1.0f, 0.2f, 1.0f, 150.0f, 1.0f}; // Default
}

#endif
