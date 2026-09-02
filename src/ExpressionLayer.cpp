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
        case EXPR_OBSERVE:            return "OBSRV";
        case EXPR_HESITATION:         return "HESIT";
        case EXPR_FEAR:               return "FEAR";
        case EXPR_TRUST:              return "LOVE";
        case EXPR_DISCOMFORT:         return "DISCOM";
        case EXPR_CURIOSITY:          return "CURIO";
        case EXPR_MIMICRY:            return "MIMIC";
        case EXPR_WARNING:            return "WARN";
        case EXPR_GLASS_CONTACT:      return "TOUCH";
        case EXPR_SILENT_OBSERVATION: return "STARE";
        default:                      return "NONE";
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
    float energy = physiology.getEnergy();
    float trust = relationship.getTrust();
    float resentment = relationship.getResentment();
    float urge = v3_state.expression_urge;

    float hx, hy;
    skeleton.getHeadPos(hx, hy);

    // 符号自然冷却倒计时 (避免频繁刷屏，维持外星语言神秘质感)
    if (symbol_cooldown > 0.0f) {
        symbol_cooldown -= dt;
    }

    // 动态选取远离毒液身体的开阔屏幕区域生成符号 (绝不再被头部覆盖擦除!)
    float sym_cx = (hx < SCREEN_W * 0.5f) ? (SCREEN_W * 0.65f) : (SCREEN_W * 0.35f);
    float sym_cy = (hy > SCREEN_H * 0.5f) ? 45.0f : 85.0f;

    // 1. 七肢桶活体符号喷射（8大经典符号全面自主激活，从身体表皮由小到大喷薄而出）
    if (!fluid_symbols.hasActiveSymbol() && symbol_cooldown <= 0.0f) {
        // [符号 1: 叹号 "help" / "!"] 倒置或突发剧烈惊恐
        if (is_upside_down && stress > 0.40f) {
            fluid_symbols.trigger("help", sym_cx, sym_cy, hx, hy);
            triggerExpression(EXPR_FEAR, 4.0f);
            symbol_cooldown = 14.0f;
            return;
        }

        // [符号 2: 拒止叉号 "no" / "x"] 高怨恨或持续强刺激
        if (resentment > 0.50f || stress > 0.65f) {
            if ((rand() % 100) < 45) {
                fluid_symbols.trigger("no", sym_cx, sym_cy, hx, hy);
                triggerExpression(EXPR_DISCOMFORT, 3.8f);
                symbol_cooldown = 15.0f;
                return;
            }
        }

        // [符号 3: 液态爱心 "heart"] 极高信任与深沉依恋
        if (trust > 0.60f && comfort > 0.55f && stress < 0.20f) {
            if ((rand() % 100) < 35) {
                fluid_symbols.trigger("heart", sym_cx, sym_cy, hx, hy);
                triggerExpression(EXPR_TRUST, 4.5f);
                symbol_cooldown = 20.0f;
                return;
            }
        }

        // [符号 4: 友善水墨圆环 "o" / "agree"] 信任认可与舒适
        if (trust > 0.40f && comfort > 0.60f && stress < 0.25f) {
            if ((rand() % 100) < 30) {
                fluid_symbols.trigger("o", sym_cx, sym_cy, hx, hy);
                triggerExpression(EXPR_TRUST, 4.0f);
                symbol_cooldown = 16.0f;
                return;
            }
        }

        // [符号 5: 好奇问号 "question" / "?"] 好奇探究新事物或声音
        if (curiosity > 0.55f && stress < 0.30f) {
            if ((rand() % 100) < 40) {
                fluid_symbols.trigger("question", sym_cx, sym_cy, hx, hy);
                triggerExpression(EXPR_CURIOSITY, 4.0f);
                symbol_cooldown = 16.0f;
                return;
            }
        }

        // [符号 6: 七肢桶环形观察之眼 "eye" / "watch"] 深度注视观察者
        if (comfort > 0.50f && stress < 0.20f && current_expression == EXPR_NONE) {
            if ((rand() % 100) < 25) {
                fluid_symbols.trigger("eye", sym_cx, sym_cy, hx, hy);
                triggerExpression(EXPR_OBSERVE, 4.5f);
                symbol_cooldown = 18.0f;
                return;
            }
        }

        // [符号 7: 泼墨图腾 "splash" / "doodle"] 精力充沛活泼玩闹
        if (energy > 0.70f && comfort > 0.55f && stress < 0.20f) {
            if ((rand() % 100) < 20) {
                fluid_symbols.trigger("splash", sym_cx, sym_cy, hx, hy);
                symbol_cooldown = 22.0f;
                return;
            }
        }

        // [符号 8: 放射警示 "warning"] 强刺激神经紧绷
        if (stress > 0.75f) {
            fluid_symbols.trigger("warning", sym_cx, sym_cy, hx, hy);
            triggerExpression(EXPR_WARNING, 3.5f);
            symbol_cooldown = 14.0f;
            return;
        }

        // [符号 9: 剧毒咬痕 "bite"] 怨念威慑或饥饿捕食兽性
        if (resentment > 0.35f || (stress > 0.45f && energy < 0.40f)) {
            if ((rand() % 100) < 35) {
                fluid_symbols.trigger("bite", sym_cx, sym_cy, hx, hy);
                triggerExpression(EXPR_WARNING, 4.0f);
                symbol_cooldown = 16.0f;
                return;
            }
        }

        // LLM 远程意图直通支持
        if (urge > 0.60f) {
            if (curiosity > 0.50f) {
                fluid_symbols.trigger("question", sym_cx, sym_cy, hx, hy);
                triggerExpression(EXPR_CURIOSITY, 3.5f);
                symbol_cooldown = 15.0f;
                return;
            } else if (trust > 0.35f) {
                fluid_symbols.trigger("eye", sym_cx, sym_cy, hx, hy);
                triggerExpression(EXPR_OBSERVE, 4.5f);
                symbol_cooldown = 15.0f;
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
