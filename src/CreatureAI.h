#pragma once
#include <Arduino.h>
#include "config.h"
#include "PhysiologySystem.h"
#include "RelationshipSystem.h"
#include "SkeletonSystem.h"
#include "MetaballSystem.h"
#include "TentacleRenderer.h"
#include "ExpressionLayer.h"
#include "LLMClient.h"

enum CreatureState {
    STATE_IDLE = 0,
    STATE_CRAWL,
    STATE_OBSERVE,
    STATE_SLEEP,
    STATE_STARTLED,
    STATE_HESITATING,
    STATE_JOLTING,
    STATE_EXPRESSING
};

class CreatureAI {
public:
    CreatureAI();

    void init();
    void update(float dt, SkeletonSystem &skeleton, MetaballSystem &metaballs,
                TentacleRenderer &tentacles, PhysiologySystem &physiology,
                RelationshipSystem &relationship, ExpressionLayer &expression,
                const ConsciousnessStateV3 &v3_state);

    CreatureState getState() const { return current_state; }
    const char* getStateName() const;

    void getCrawlBias(float &bx, float &by) const { bx = crawl_force_x; by = crawl_force_y; }
    void getLookTarget(float &lx, float &ly) const { lx = target_look_x; ly = target_look_y; }
    float getRespiration() const { return respiration_factor; }

    bool isStartled() const { return current_state == STATE_STARTLED || current_state == STATE_JOLTING; }
    bool isSleeping() const { return current_state == STATE_SLEEP; }

    void triggerStartle(float intensity = 1.0f);
    void triggerJolt(SkeletonSystem &skeleton, MetaballSystem &metaballs, float intensity = 1.0f);
    void triggerInteraction();
    void updateSensors(float imu_gx, float imu_gy, float imu_gz, const PhysiologySystem &physiology, bool btn_a_pressed);

private:
    CreatureState current_state = STATE_IDLE;
    CreatureState pending_state = STATE_IDLE;
    float state_timer = 0.0f;
    float state_duration = 4.0f;
    float hesitation_timer = 0.0f;

    float respiration_phase = 0.0f;
    float respiration_factor = 0.0f;
    float twitch_timer = 0.0f;
    float twitch_offset = 0.0f;

    float micro_behavior_timer = 0.0f;

    float crawl_force_x = 0.0f;
    float crawl_force_y = 0.0f;
    float crawl_target_x = 120.0f;
    float crawl_target_y = 100.0f;
    int crawl_perimeter_edge = 0;
    float crawl_shoot_timer = 0.0f;

    float target_look_x = 120.0f;
    float target_look_y = 67.0f;

    float startle_energy = 0.0f;

    void enterState(CreatureState new_state, TentacleRenderer *tentacles = nullptr, float hx = 120.0f, float hy = 100.0f);
    void enterHesitation(CreatureState target_state, float delay_sec);
    void updateOrganicBreathing(float dt, const PhysiologySystem &physiology, const ExpressionLayer &expression);
    void updateMicroBehaviors(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology);

    void updateIdle(float dt, float hx, float hy, const PhysiologySystem &physiology, const RelationshipSystem &relationship, TentacleRenderer &tentacles);
    void updateCrawl(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles);
    void updateObserve(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles);
    void updateSleep(float dt, float hx, float hy, const PhysiologySystem &physiology);
    void updateStartled(float dt, float hx, float hy, const PhysiologySystem &physiology);
    void updateHesitating(float dt, float hx, float hy, const ExpressionLayer &expression);
    void updateJolting(float dt, float hx, float hy, const PhysiologySystem &physiology);
    void updateExpressing(float dt, float hx, float hy, const ExpressionLayer &expression);
};
