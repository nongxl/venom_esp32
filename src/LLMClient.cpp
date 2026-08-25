#include "LLMClient.h"
#include <ArduinoJson.h>

static LLMClient *g_llm_instance = nullptr;

static V3Intent parseIntentString(const char *str) {
    if (!str) return INTENT_IDLE;
    if (strstr(str, "approach")) return INTENT_APPROACH_OBSERVER;
    if (strstr(str, "avoid"))    return INTENT_AVOID_OBSERVER;
    if (strstr(str, "watch"))    return INTENT_WATCH_OBSERVER;
    if (strstr(str, "test") || strstr(str, "boundary")) return INTENT_TEST_BOUNDARY;
    if (strstr(str, "shadow"))   return INTENT_SEEK_SHADOW;
    if (strstr(str, "safety"))   return INTENT_SEEK_SAFETY;
    if (strstr(str, "patrol"))   return INTENT_PATROL_TERRITORY;
    if (strstr(str, "hide"))     return INTENT_HIDE_PRESENCE;
    if (strstr(str, "distress")) return INTENT_EXPRESS_DISTRESS;
    return INTENT_IDLE;
}

LLMClient::LLMClient() {
    g_llm_instance = this;
}

void LLMClient::taskEntry(void *param) {
    LLMClient *client = (LLMClient *)param;
    client->runLLMTask();
}

void LLMClient::init() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(DEFAULT_SSID, DEFAULT_PASSWORD);

    // 创建 FreeRTOS 异步工作任务 (Core 0，优先级 1，栈 8192 字节)
    xTaskCreatePinnedToCore(taskEntry, "LLMTask", 8192, this, 1, &llm_task_handle, 0);
}

void LLMClient::runLLMTask() {
    while (true) {
        // 1. 检查 WiFi 状态
        if (WiFi.status() == WL_CONNECTED) {
            if (!wifi_connected) {
                wifi_connected = true;
                Serial.println(">>> [LLM Background] WiFi Connected successfully!");
            }
        } else {
            wifi_connected = false;
        }

        // 2. 检查是否有主线程投递过来的 LLM 意图请求
        bool has_req = false;
        LLMRequestBuffer req;

        portENTER_CRITICAL(&state_mutex);
        if (request_buf.has_request) {
            has_req = true;
            req = request_buf;
            request_buf.has_request = false;
        }
        portEXIT_CRITICAL(&state_mutex);

        if (has_req) {
            if (!wifi_connected) {
                runLocalHeuristicFallback(req.stress, req.curiosity, req.comfort, req.attachment);
            } else {
                // 构建 JSON 请求
                StaticJsonDocument<1536> doc;
                doc["model"] = AI_MODEL;
                doc["temperature"] = 0.7;
                doc["max_tokens"] = 280;

                JsonArray messages = doc.createNestedArray("messages");

                JsonObject sys_msg = messages.createNestedObject();
                sys_msg["role"] = "system";
                sys_msg["content"] = "You are a conscious, living liquid symbiote (Venom) trapped inside an electronic glass device. "
                                     "You perceive the human observer outside. Output strictly a single valid JSON object without markdown fences:\n"
                                     "{\n"
                                     "  \"emotional_shift\": \"calm|curious|agitated|fearful|defensive|fascinated|exhausted\",\n"
                                     "  \"primary_intent\": \"watch_observer|approach_observer|avoid_observer|test_boundary|seek_shadow|seek_safety|patrol_territory|hide_presence|express_distress\",\n"
                                     "  \"secondary_intent\": \"watch_observer|approach_observer|avoid_observer|test_boundary|seek_shadow|seek_safety|idle\",\n"
                                     "  \"focus_target\": \"observer|container_edge|shadow|light_source|self\",\n"
                                     "  \"impulse_strength\": 0.0-1.0,\n"
                                     "  \"expression_urge\": 0.0-1.0,\n"
                                     "  \"social_openness\": 0.0-1.0,\n"
                                     "  \"resentment_delta\": -1.0-1.0,\n"
                                     "  \"trust_delta\": -1.0-1.0,\n"
                                     "  \"notes\": \"first-person visceral subjective experience of the symbiote trapped in glass\"\n"
                                     "}";

                JsonObject user_msg = messages.createNestedObject();
                user_msg["role"] = "user";
                char state_str[256];
                snprintf(state_str, sizeof(state_str),
                         "Current state: energy=%.2f, stress=%.2f, curiosity=%.2f, comfort=%.2f, attachment=%.2f. Behavior: %s. Events: %s.",
                         req.energy, req.stress, req.curiosity, req.comfort, req.attachment, req.current_behavior, req.recent_events);
                user_msg["content"] = state_str;

                String payload;
                serializeJson(doc, payload);

                // 在后台核心 (Core 0) 执行 HTTP POST，主线程 (Core 1) 绝对 0 阻塞！
                if (!sendHTTPRequest(payload)) {
                    runLocalHeuristicFallback(req.stress, req.curiosity, req.comfort, req.attachment);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void LLMClient::runLocalHeuristicFallback(float stress, float curiosity, float comfort, float attachment) {
    portENTER_CRITICAL(&state_mutex);
    if (stress > 0.65f) {
        strncpy(latest_state.emotional_shift, "agitated", sizeof(latest_state.emotional_shift) - 1);
        latest_state.primary_intent = INTENT_AVOID_OBSERVER;
        latest_state.secondary_intent = INTENT_SEEK_SAFETY;
        latest_state.impulse_strength = 0.85f;
        latest_state.expression_urge = 0.75f;
        latest_state.resentment_delta = 0.08f;
        latest_state.trust_delta = -0.05f;
        strncpy(latest_state.notes, "Tremors rattle my fluid core. The cage is unstable.", sizeof(latest_state.notes) - 1);
    } else if (curiosity > 0.55f && comfort > 0.40f) {
        strncpy(latest_state.emotional_shift, "curious", sizeof(latest_state.emotional_shift) - 1);
        latest_state.primary_intent = INTENT_APPROACH_OBSERVER;
        latest_state.secondary_intent = (attachment < 0.35f) ? INTENT_AVOID_OBSERVER : INTENT_WATCH_OBSERVER;
        latest_state.impulse_strength = 0.60f;
        latest_state.expression_urge = 0.45f;
        latest_state.resentment_delta = -0.02f;
        latest_state.trust_delta = 0.04f;
        strncpy(latest_state.notes, "Light filters through the transparent barrier. I want to inspect the presence beyond.", sizeof(latest_state.notes) - 1);
    } else {
        strncpy(latest_state.emotional_shift, "calm", sizeof(latest_state.emotional_shift) - 1);
        latest_state.primary_intent = INTENT_WATCH_OBSERVER;
        latest_state.secondary_intent = INTENT_IDLE;
        latest_state.impulse_strength = 0.30f;
        latest_state.expression_urge = 0.15f;
        latest_state.resentment_delta = -0.01f;
        latest_state.trust_delta = 0.02f;
        strncpy(latest_state.notes, "A quiet hum surrounds the chamber. My mass rests against the glass.", sizeof(latest_state.notes) - 1);
    }
    latest_state.has_new_update = true;
    portEXIT_CRITICAL(&state_mutex);
}

void LLMClient::parseV3Response(const String &response_text) {
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, response_text);

    if (error) {
        Serial.print(">>> [LLM] JSON parse failed: ");
        Serial.println(error.c_str());
        return;
    }

    const char *content = doc["choices"][0]["message"]["content"];
    if (!content) return;

    StaticJsonDocument<1024> content_doc;
    DeserializationError c_error = deserializeJson(content_doc, content);
    if (c_error) {
        String s = content;
        int start = s.indexOf('{');
        int end = s.lastIndexOf('}');
        if (start >= 0 && end > start) {
            String clean_json = s.substring(start, end + 1);
            c_error = deserializeJson(content_doc, clean_json);
        }
    }

    if (!c_error) {
        portENTER_CRITICAL(&state_mutex);
        const char *emo = content_doc["emotional_shift"] | "calm";
        const char *p_intent = content_doc["primary_intent"] | "watch_observer";
        const char *s_intent = content_doc["secondary_intent"] | "idle";
        const char *f_target = content_doc["focus_target"] | "observer";
        const char *notes_str = content_doc["notes"] | "I feel the glass.";

        strncpy(latest_state.emotional_shift, emo, sizeof(latest_state.emotional_shift) - 1);
        latest_state.primary_intent = parseIntentString(p_intent);
        latest_state.secondary_intent = parseIntentString(s_intent);
        strncpy(latest_state.focus_target, f_target, sizeof(latest_state.focus_target) - 1);
        latest_state.impulse_strength = content_doc["impulse_strength"] | 0.5f;
        latest_state.expression_urge = content_doc["expression_urge"] | 0.2f;
        latest_state.social_openness = content_doc["social_openness"] | 0.35f;
        latest_state.resentment_delta = content_doc["resentment_delta"] | 0.0f;
        latest_state.trust_delta = content_doc["trust_delta"] | 0.0f;
        strncpy(latest_state.notes, notes_str, sizeof(latest_state.notes) - 1);
        latest_state.has_new_update = true;
        portEXIT_CRITICAL(&state_mutex);

        Serial.printf(">>> [LLM Async V3] Intent: %s | Notes: %s\n", p_intent, notes_str);
    }
}

bool LLMClient::sendHTTPRequest(const String &json_payload) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    if (!http.begin(client, LLM_URL)) {
        return false;
    }

    http.setTimeout(6000);
    http.addHeader("Content-Type", "application/json");
    String auth = String("Bearer ") + DEFAULT_API_KEY;
    http.addHeader("Authorization", auth.c_str());

    int http_code = http.POST(json_payload);
    bool success = false;

    if (http_code == HTTP_CODE_OK) {
        String response = http.getString();
        parseV3Response(response);
        success = true;
    } else {
        Serial.printf(">>> [LLM] HTTP error: %d\n", http_code);
    }

    http.end();
    return success;
}

// 主线程调用接口：纯无锁压入缓冲区，耗时 0 微秒！
void LLMClient::requestConsciousnessUpdate(float energy, float stress, float curiosity,
                                           float comfort, float attachment,
                                           const char *current_behavior, const char *recent_events) {
    unsigned long now = millis();
    if (now - last_request_time < 12000) {
        return;
    }
    last_request_time = now;

    portENTER_CRITICAL(&state_mutex);
    request_buf.has_request = true;
    request_buf.energy = energy;
    request_buf.stress = stress;
    request_buf.curiosity = curiosity;
    request_buf.comfort = comfort;
    request_buf.attachment = attachment;
    if (current_behavior) strncpy(request_buf.current_behavior, current_behavior, 31);
    if (recent_events) strncpy(request_buf.recent_events, recent_events, 31);
    portEXIT_CRITICAL(&state_mutex);
}

ConsciousnessStateV3 LLMClient::getLatestState() {
    ConsciousnessStateV3 res;
    portENTER_CRITICAL(&state_mutex);
    res = latest_state;
    latest_state.has_new_update = false;
    portEXIT_CRITICAL(&state_mutex);
    return res;
}

void LLMClient::update(float dt) {}
