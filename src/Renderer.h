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

class Renderer {
public:
    Renderer();

    void init(M5Canvas *target_canvas);
    void render(const SkeletonSystem &skeleton, const MetaballSystem &metaballs,
                const EyeSystem &eye, const TentacleRenderer &tentacles,
                const CreatureAI &ai, const PhysiologySystem &physiology,
                const VoronoiSurface &voronoi, const FluidSymbolSystem &fluid_symbols,
                const RelationshipSystem &relationship, const ExpressionLayer &expression,
                const ConsciousnessStateV3 &v3_state, float fps);

    void nextTheme();
    void setTheme(BackgroundTheme theme);
    BackgroundTheme getTheme() const { return current_theme; }

    void sendScreenshotSerial();
    void toggleHUD();
    bool isHUDActive() const { return show_hud; }
    M5Canvas* getCanvas() { return canvas; }

private:
    M5Canvas *canvas = nullptr;
    BackgroundTheme current_theme = THEME_DESIGN_BLUE;
    bool show_hud = false;

    // 意识泄漏事件状态
    bool consciousness_leak_active = false;

    uint16_t getBackgroundColor() const;
    void renderFieldAndVoronoi(const MetaballSystem &metaballs, const VoronoiSurface &voronoi, const PhysiologySystem &physiology);
    void renderMeniscusGlow(const MetaballSystem &metaballs);
    void renderHUD(const CreatureAI &ai, const PhysiologySystem &physiology,
                   const RelationshipSystem &relationship, const ExpressionLayer &expression,
                   const ConsciousnessStateV3 &v3_state, float fps);
};
