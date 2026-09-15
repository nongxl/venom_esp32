#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <M5GFX.h>
#include <functional>
#include "ConfigManager.h"

class CreatureAI;
class PhysiologySystem;
class RelationshipSystem;
class ExpressionLayer;
class LLMClient;

class WebConfigPortal {
public:
    WebConfigPortal();

    void setNeuralDataSource(const CreatureAI *ai, const PhysiologySystem *phys,
                             const RelationshipSystem *rel, const ExpressionLayer *expr,
                             const LLMClient *llm) {
        src_ai = ai;
        src_phys = phys;
        src_rel = rel;
        src_expr = expr;
        src_llm = llm;
    }

    void start(M5Canvas *canvas = nullptr);
    void update();
    void stop();

    bool isRunning() const { return running; }
    bool isSaveAndRebootRequested() const { return save_reboot_requested; }

    bool isBlockingScreen() const { return blocking_screen; }
    void setBlockingScreen(bool block) { blocking_screen = block; }

    void setOnDemoAction(std::function<void(const String&)> cb) { on_demo_action = cb; }

private:
    WebServer server;
    DNSServer dns_server;
    M5Canvas *portal_canvas = nullptr;

    const CreatureAI *src_ai = nullptr;
    const PhysiologySystem *src_phys = nullptr;
    const RelationshipSystem *src_rel = nullptr;
    const ExpressionLayer *src_expr = nullptr;
    const LLMClient *src_llm = nullptr;

    bool running = false;
    bool blocking_screen = true;
    bool routes_configured = false;
    bool save_reboot_requested = false;
    unsigned long reboot_time_ms = 0;
    unsigned long screen_refresh_timer = 0;

    String scanned_ssids_json;
    std::function<void(const String&)> on_demo_action = nullptr;

    void scanNearbyNetworks();
    void setupRoutes();
    void handleRoot();
    void handleScan();
    void handleSave();
    void handleDemo();
    void handleNeural();
    void handleNotFound();

    void renderPortalScreen();
    String generateHTMLPage();
};

