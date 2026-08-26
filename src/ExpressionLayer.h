#pragma once
#include <Arduino.h>
#include "config.h"
#include "PhysiologySystem.h"
#include "RelationshipSystem.h"
#include "FluidSymbolSystem.h"
#include "SkeletonSystem.h"
#include "LLMClient.h"
#include "RhythmDetector.h"

class ExpressionLayer {
public:
    ExpressionLayer();

    void init();
    void update(float dt, const ConsciousnessStateV3 &v3_state,
                const PhysiologySystem &physiology, const RelationshipSystem &relationship,
                const RhythmDetector &rhythm, FluidSymbolSystem &fluid_symbols,
                SkeletonSystem &skeleton, bool is_upside_down);

    ExpressionType getCurrentExpression() const { return current_expression; }
    const char* getExpressionName() const;

    // 犹豫行为参数（用于骨架运动调制）
    bool isHesitating() const { return current_expression == EXPR_HESITATION; }
    float getHesitationStep() const { return hesitation_phase; }

    // 节拍模仿参数
    bool isMimicking() const { return current_expression == EXPR_MIMICRY; }
    int getMimicTapIndex() const { return mimic_current_tap; }

    // 隔玻璃贴手目标位置
    bool isGlassContacting() const { return current_expression == EXPR_GLASS_CONTACT; }
    void getGlassContactTarget(float &tx, float &ty) const { tx = contact_target_x; ty = contact_target_y; }

    // 触发指定表达
    void triggerExpression(ExpressionType type, float duration = 3.5f);

private:
    ExpressionType current_expression = EXPR_NONE;
    float expr_timer = 0.0f;
    float expr_duration = 3.5f;

    // 犹豫动力学
    float hesitation_phase = 0.0f; // 0~1:前探, 1~2:停顿, 2~3:后撤, 3~4:再试探

    // 节拍模仿动力学
    unsigned long mimic_start_ms = 0;
    unsigned long mimic_interval = 400;
    int mimic_total_taps = 2;
    int mimic_current_tap = 0;
    unsigned long last_tap_exec_ms = 0;

    // 隔玻璃贴手
    float contact_target_x = SCREEN_W - 10.0f;
    float contact_target_y = SCREEN_H * 0.5f;
    float symbol_cooldown = 8.0f; // 符号喷射自然冷却期

    // 语义倾向加权
    void analyzeNotesSemantics(const char *notes);
    void evaluateAutonomousExpressions(float dt, const ConsciousnessStateV3 &v3_state,
                                       const PhysiologySystem &physiology, const RelationshipSystem &relationship,
                                       FluidSymbolSystem &fluid_symbols, const SkeletonSystem &skeleton,
                                       bool is_upside_down);
};
