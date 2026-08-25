#include "RelationshipSystem.h"
#include <algorithm>

RelationshipSystem::RelationshipSystem() {}

void RelationshipSystem::init() {
    trust = 0.20f;
    resentment = 0.05f;
    social_openness = 0.35f;
}

void RelationshipSystem::updateSocialOpenness() {
    social_openness = std::max(0.05f, std::min(0.95f, 0.30f + trust * 0.60f - resentment * 0.50f));
}

void RelationshipSystem::applyDeltas(float trust_delta, float resentment_delta, float openness_override) {
    trust = std::max(0.0f, std::min(1.0f, trust + trust_delta));
    resentment = std::max(0.0f, std::min(1.0f, resentment + resentment_delta));

    if (openness_override >= 0.0f) {
        social_openness = openness_override;
    } else {
        updateSocialOpenness();
    }
}

void RelationshipSystem::update(float dt, float imu_shake, bool is_peaceful) {
    if (imu_shake > 0.3f) {
        resentment = std::min(1.0f, resentment + imu_shake * dt * 0.04f);
        trust = std::max(0.0f, trust - imu_shake * dt * 0.02f);
    } else if (is_peaceful) {
        trust = std::min(1.0f, trust + dt * 0.008f);
        resentment = std::max(0.0f, resentment - dt * 0.012f);
    }

    updateSocialOpenness();
}
