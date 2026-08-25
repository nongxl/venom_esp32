#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"

// 状态结构体
struct ConsciousnessStateV3 {
    char emotional_shift[32] = "calm";
    V3Intent primary_intent = INTENT_WATCH_OBSERVER;
    V3Intent secondary_intent = INTENT_IDLE;
    char focus_target[32] = "observer";
    float impulse_strength = 0.3f;
    float expression_urge = 0.15f;
    float social_openness = 0.35f;
    float resentment_delta = 0.0f;
    float trust_delta = 0.02f;
    char notes[128] = "I perceive the glass container and the observer outside.";
    bool has_new_update = false;
};

struct LLMRequestBuffer {
    bool has_request = false;
    float energy = 0.8f;
    float stress = 0.1f;
    float curiosity = 0.5f;
    float comfort = 0.8f;
    float attachment = 0.2f;
    char current_behavior[32] = "IDLE";
    char recent_events[32] = "calm";
};

class LLMClient {
public:
    LLMClient();

    void init();
    void update(float dt);

    void requestConsciousnessUpdate(float energy, float stress, float curiosity,
                                   float comfort, float attachment,
                                   const char *current_behavior, const char *recent_events);

    ConsciousnessStateV3 getLatestState();
    const char* getLatestNotes() const { return latest_state.notes; }
    bool isWiFiConnected() const { return wifi_connected; }

private:
    ConsciousnessStateV3 latest_state;
    LLMRequestBuffer request_buf;
    portMUX_TYPE state_mutex = portMUX_INITIALIZER_UNLOCKED;

    TaskHandle_t llm_task_handle = nullptr;
    bool wifi_connected = false;
    bool request_in_progress = false;
    unsigned long last_request_time = 0;

    static void taskEntry(void *param);
    void runLLMTask();

    bool sendHTTPRequest(const String &json_payload);
    void parseV3Response(const String &response_text);
    void runLocalHeuristicFallback(float stress, float curiosity, float comfort, float attachment);
};
