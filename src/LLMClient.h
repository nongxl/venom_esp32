#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"

// V3 结构化意识输出数据包
struct ConsciousnessStateV3 {
    char emotional_shift[24] = "calm";
    V3Intent primary_intent = INTENT_WATCH_OBSERVER;
    V3Intent secondary_intent = INTENT_IDLE;
    char focus_target[24] = "observer";
    float impulse_strength = 0.5f;
    float expression_urge = 0.2f;
    float social_openness = 0.35f;
    float resentment_delta = 0.0f;
    float trust_delta = 0.0f;
    char notes[192] = "The glass is cool. I sense light and motion beyond.";
    bool has_new_update = false;
};

class LLMClient {
public:
    LLMClient();

    void init();
    void update(float dt);

    // 触发异步请求
    void requestConsciousnessUpdate(float energy, float stress, float curiosity,
                                    float comfort, float attachment,
                                    const char *current_behavior, const char *recent_events);

    // 获取最新意识状态 (线程安全)
    ConsciousnessStateV3 getLatestState();

    // 获取最新 notes (用于 5% 概率意识泄漏展示)
    const char* getLatestNotes() const { return latest_state.notes; }

    bool isConnected() const { return wifi_connected; }

private:
    ConsciousnessStateV3 latest_state;
    bool wifi_connected = false;
    bool request_in_progress = false;
    unsigned long last_request_time = 0;
    portMUX_TYPE state_mutex = portMUX_INITIALIZER_UNLOCKED;

    // 异步任务句柄
    TaskHandle_t llm_task_handle = nullptr;

    static void taskEntry(void *param);
    void runLLMTask();
    bool sendHTTPRequest(const String &json_payload);
    void parseV3Response(const String &response_text);
    void runLocalHeuristicFallback(float stress, float curiosity, float comfort, float attachment);
};
