#include "ExpressionLayer.h"
#include <cmath>

ExpressionLayer::ExpressionLayer() {}

void ExpressionLayer::init() {
    current_expression = EXPR_NONE;
    expr_timer = 0.0f;
    expr_duration = 3.5f;
    hesitation_phase = 0.0f;
    mimic_current_tap = 0;
}

const char* ExpressionLayer::getExpressionName() const {
    switch (current_expression) {
        case EXPR_OBSERVE:            return "EXPR_OBSERVE";
        case EXPR_HESITATION:         return "EXPR_HESITATION";
        case EXPR_FEAR:               return "EXPR_FEAR";
        case EXPR_TRUST:              return "EXPR_TRUST";
        case EXPR_DISCOMFORT:         return "EXPR_DISCOMFORT";
        case EXPR_CURIOSITY:          return "EXPR_CURIOSITY";
        case EXPR_MIMICRY:            return "EXPR_MIMICRY";
        case EXPR_WARNING:            return "EXPR_WARNING";
        case EXPR_GLASS_CONTACT:      return "EXPR_GLASS_CONTACT";
        case EXPR_SILENT_OBSERVATION: return "EXPR_SILENT_OBSERVE";
        default:                      return "EXPR_NONE";
    }
}

void ExpressionLayer::triggerExpression(ExpressionType type, float duration) {
    current_expression = type;
    expr_timer = 0.0f;
    expr_duration = duration;

    if (type == EXPR_HESITATION) {
        hesitation_phase = 0.0f;
    }
}

void ExpressionLayer::analyzeNotesSemantics(const char *notes) {
    if (!notes) return;
}

void ExpressionLayer::evaluateAutonomousExpressions(float dt, const ConsciousnessStateV3 &v3_state,
                                                   const PhysiologySystem &physiology, const RelationshipSystem &relationship,
                                                   FluidSymbolSystem &fluid_symbols, const SkeletonSystem &skeleton,
                                                   bool is_upside_down) {
    float stress = physiology.getStress();
    float curiosity = physiology.getCuriosity();
    float comfort = physiology.getComfort();
    float trust = relationship.getTrust();
    float resentment = relationship.getResentment();
    float urge = v3_state.expression_urge;

    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    // 1. 极简七肢桶活体符号喷射（低频、神秘、有机）
    if (!fluid_symbols.hasActiveSymbol()) {
        // 倒置且严重不安 -> 叹号 "help" / "!"
        if (is_upside_down && stress > 0.45f) {
            fluid_symbols.trigger("help");
            triggerExpression(EXPR_FEAR, 4.0f);
            return;
        }

        // 高怨恨或持续强刺激 -> 拒止叉号 "no" / "x"
        if (stress > 0.70f || resentment > 0.60f) {
            if ((rand() % 100) < 55) {
                fluid_symbols.trigger("no");
                triggerExpression(EXPR_DISCOMFORT, 3.8f);
                return;
            }
        }

        // 高好奇或 LLM 意图驱动 -> 问号 "question" 或 七肢桶环形圆圈 "eye"
        if (urge > 0.65f) {
            if (curiosity > 0.60f) {
                fluid_symbols.trigger("question");
                triggerExpression(EXPR_CURIOSITY, 3.5f);
                return;
            } else if (trust > 0.45f) {
                fluid_symbols.trigger("eye");
                triggerExpression(EXPR_OBSERVE, 4.5f);
                return;
            }
        }
    }

    // 2. 意图冲突（想靠近又害怕）
    if (v3_state.primary_intent == INTENT_APPROACH_OBSERVER && v3_state.secondary_intent == INTENT_AVOID_OBSERVER) {
        if (current_expression != EXPR_HESITATION) {
            triggerExpression(EXPR_HESITATION, 5.0f);
            return;
        }
    }

    // 3. 隔玻璃贴手接触
    if (trust > 0.60f && stress < 0.20f && comfort > 0.65f) {
        if (current_expression == EXPR_NONE && (rand() % 100) < 30) {
            contact_target_x = (hx < SCREEN_W * 0.5f) ? 10.0f : (SCREEN_W - 10.0f);
            contact_target_y = SCREEN_H * 0.5f + ((rand() % 40) - 20);
            triggerExpression(EXPR_GLASS_CONTACT, 4.5f);
            return;
        }
    }

    // 4. 信任表达
    if (trust > 0.50f && stress < 0.15f && current_expression == EXPR_NONE) {
        if ((rand() % 100) < 25) {
            triggerExpression(EXPR_TRUST, 4.0f);
            return;
        }
    }

    // 5. 沉默观察
    if (current_expression == EXPR_NONE && comfort > 0.6f && (rand() % 100) < 20) {
        triggerExpression(EXPR_SILENT_OBSERVATION, 6.0f);
        return;
    }
}

void ExpressionLayer::update(float dt, const ConsciousnessStateV3 &v3_state,
                             const PhysiologySystem &physiology, const RelationshipSystem &relationship,
                             const RhythmDetector &rhythm, FluidSymbolSystem &fluid_symbols,
                             SkeletonSystem &skeleton, bool is_upside_down) {
    if (rhythm.hasRhythmToMimic() && current_expression != EXPR_MIMICRY) {
        triggerExpression(EXPR_MIMICRY, (rhythm.getMimicTapCount() * rhythm.getMimicIntervalMs() + 1200) * 0.001f);
        mimic_start_ms = millis() + 600;
        mimic_interval = rhythm.getMimicIntervalMs();
        mimic_total_taps = rhythm.getMimicTapCount();
        mimic_current_tap = 0;
        last_tap_exec_ms = 0;
    }

    if (current_expression != EXPR_NONE) {
        expr_timer += dt;

        if (current_expression == EXPR_HESITATION) {
            hesitation_phase += dt * 0.8f;
        }

        if (current_expression == EXPR_MIMICRY) {
            unsigned long now = millis();
            if (now >= mimic_start_ms && mimic_current_tap < mimic_total_taps) {
                if (now - last_tap_exec_ms >= mimic_interval) {
                    last_tap_exec_ms = now;
                    mimic_current_tap++;
                    skeleton.applyImpulse(((rand() % 2 == 0) ? -0.8f : 0.8f), -0.4f);
                }
            }
        }

        if (expr_timer >= expr_duration) {
            current_expression = EXPR_NONE;
        }
    } else {
        evaluateAutonomousExpressions(dt, v3_state, physiology, relationship, fluid_symbols, skeleton, is_upside_down);
    }
}
