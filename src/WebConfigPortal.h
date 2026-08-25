#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <M5GFX.h>
#include "ConfigManager.h"

class WebConfigPortal {
public:
    WebConfigPortal();

    void start(M5Canvas *canvas = nullptr);
    void update();
    void stop();

    bool isRunning() const { return running; }
    bool isSaveAndRebootRequested() const { return save_reboot_requested; }

private:
    WebServer server;
    DNSServer dns_server;
    M5Canvas *portal_canvas = nullptr;

    bool running = false;
    bool save_reboot_requested = false;
    unsigned long reboot_time_ms = 0;
    unsigned long screen_refresh_timer = 0;

    String scanned_ssids_json;

    void scanNearbyNetworks();
    void setupRoutes();
    void handleRoot();
    void handleScan();
    void handleSave();
    void handleNotFound();

    void renderPortalScreen();
    String generateHTMLPage();
};
