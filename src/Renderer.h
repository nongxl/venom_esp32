#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"
#include "SkeletonSystem.h"
#include "MetaballSystem.h"
#include "EyeSystem.h"
#include "TentacleRenderer.h"
#include "CreatureAI.h"
#include "PhysiologySystem.h"
#include "VoronoiSurface.h"
#include "FluidSymbolSystem.h"
#include "RelationshipSystem.h"
#include "ExpressionLayer.h"
#include "LLMClient.h"
#include "PreyBugSystem.h"
#include "PredatorSystem.h"

enum MindEchoState {
    ECHO_IDLE = 0,
    ECHO_TYPING,
    ECHO_SUSTAIN,
    ECHO_FADING
};

class Renderer {
public:
    Renderer();

    void init(M5Canvas *target_canvas);
    void render(const SkeletonSystem &skeleton, const MetaballSystem &metaballs,
                const EyeSystem &eye, const TentacleRenderer &tentacles,
                const CreatureAI &ai, const PhysiologySystem &physiology,
                const VoronoiSurface &voronoi, const FluidSymbolSystem &fluid_symbols,
                const RelationshipSystem &relationship, const ExpressionLayer &expression,
                const ConsciousnessStateV3 &v3_state, float fps,
                const PreyBugSystem *bugs = nullptr,
                const PredatorSystem *predator = nullptr);

    void nextTheme();
    void setTheme(BackgroundTheme theme);
    BackgroundTheme getTheme() const { return current_theme; }

    void sendScreenshotSerial();
    void toggleHUD();
    bool isHUDActive() const { return show_hud; }
    M5Canvas* getCanvas() { return canvas; }

    // 触发心声打字机涌现
    void triggerMindEcho(const char *custom_text = nullptr);

private:
    M5Canvas *canvas = nullptr;
    BackgroundTheme current_theme = THEME_DESIGN_BLUE;
    bool show_hud = false;

    // 动态打字机心流状态
    MindEchoState echo_state = ECHO_IDLE;
    char current_echo_text[128] = "";
    int typed_char_count = 0;
    float char_timer = 0.0f;
    float state_timer = 0.0f;
    float auto_trigger_cooldown = 6.0f;

    uint16_t getBackgroundColor() const;
    void renderFieldAndVoronoi(const MetaballSystem &metaballs, const VoronoiSurface &voronoi, const PhysiologySystem &physiology, bool is_sleeping = false, bool is_stealth = false);
    void renderHUD(const CreatureAI &ai, const PhysiologySystem &physiology,
                   const RelationshipSystem &relationship, const ExpressionLayer &expression,
                   const ConsciousnessStateV3 &v3_state, float fps);

    void updateMindEchoLifecycle(float dt, const ConsciousnessStateV3 &v3_state);
    void renderMindEchoPanel();

    // HUD 数据面板视觉刷新节流与显示缓冲 (每 350ms 刷新一次，彻底消灭高频跳字)
    unsigned long last_hud_refresh_ms = 0;
    char hud_line1_ai[16] = "AI: --";
    char hud_line1_emo[16] = "EMO: --";
    char hud_line1_fps[16] = "FPS: --";
    char hud_line2_nrg[16] = "NRG: --";
    char hud_line2_tru[16] = "TRU: --";
    char hud_line2_exp[16] = "EXP: --";
    char hud_line3_mic[16] = "MIC: --";
    char hud_line3_res[16] = "RES: --";
    char hud_line3_soc[16] = "SOC: --";
    char hud_line4_int[28] = "INT: --";
};
