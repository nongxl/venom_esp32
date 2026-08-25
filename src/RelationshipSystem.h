#pragma once
#include <Arduino.h>

class RelationshipSystem {
public:
    RelationshipSystem();

    void init();
    void update(float dt, float imu_shake, bool is_peaceful);

    void applyDeltas(float trust_delta, float resentment_delta, float openness_override = -1.0f);

    float getTrust()          const { return trust; }
    float getResentment()     const { return resentment; }
    float getSocialOpenness() const { return social_openness; }

private:
    float trust = 0.20f;
    float resentment = 0.05f;
    float social_openness = 0.35f;

    void updateSocialOpenness();
};
