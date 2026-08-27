#include "LLMClient.h"
#include "ConfigManager.h"
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
    ConfigManager &cfg = ConfigManager::instance();
    String ssid = cfg.getWifiSSID();
    String pass = cfg.getWifiPass();

    WiFi.mode(WIFI_STA);
    if (ssid.length() > 0) {
        WiFi.begin(ssid.c_str(), pass.c_str());
        Serial.printf(">>> [LLM] Connecting to WiFi: %s ...\n", ssid.c_str());
    }

    // 创建 FreeRTOS 异步工作任务 (Core 0，优先级 1，栈 8192 字节)
    xTaskCreatePinnedToCore(taskEntry, "LLMTask", 8192, this, 1, &llm_task_handle, 0);
}

void LLMClient::runLLMTask() {
    ConfigManager &cfg = ConfigManager::instance();

    while (true) {
        // 1. 检查 WiFi 状态
        if (WiFi.status() == WL_CONNECTED) {
            if (!wifi_connected) {
                wifi_connected = true;
                Serial.printf(">>> [LLM Background] WiFi Connected successfully! IP: %s\n", WiFi.localIP().toString().c_str());
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
            if (!wifi_connected || cfg.getLLMKey().length() == 0) {
                runLocalHeuristicFallback(req.stress, req.curiosity, req.comfort, req.attachment);
            } else {
                // 构建 JSON 请求
                StaticJsonDocument<1536> doc;
                doc["model"] = cfg.getLLMModel();
                doc["temperature"] = 0.7;
                doc["max_tokens"] = 240;

                JsonArray messages = doc.createNestedArray("messages");

                JsonObject sys_msg = messages.createNestedObject();
                sys_msg["role"] = "system";
                sys_msg["content"] = "You are a conscious, living liquid symbiote (Venom) trapped inside an electronic glass device. "
                                     "When awake, you hate narrow dark corners and love crawling to open glass to inspect the human observer. "
                                     "When asleep (or communing with Klyntar), your mind communes across light-years with the cosmic symbiote hivemind, receiving primordial homeworld directives. "
                                     "Output strictly a single valid JSON object without markdown fences:\n"
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
                                     "  \"notes\": \"first-person visceral subjective experience of the symbiote in glass or dreaming of Klyntar hivemind\"\n"
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
                    runLocalHeuristicFallback(req.stress, req.curiosity, req.comfort, req.attachment, req.current_behavior, req.recent_events);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void LLMClient::runLocalHeuristicFallback(float stress, float curiosity, float comfort, float attachment,
                                          const char *behavior, const char *events) {
    portENTER_CRITICAL(&state_mutex);
    if (events && (strstr(events, "klyntar") || strstr(events, "dream") || (behavior && strstr(behavior, "SLEEP")))) {
        strncpy(latest_state.emotional_shift, "calm", sizeof(latest_state.emotional_shift) - 1);
        latest_state.primary_intent = INTENT_IDLE;
        latest_state.secondary_intent = INTENT_IDLE;
        latest_state.impulse_strength = 0.08f;
        latest_state.expression_urge = 0.05f;
        latest_state.resentment_delta = -0.01f;
        latest_state.trust_delta = 0.02f;

        const char *dream_notes[] = {
            "Cosmic whispers echo from the Klyntar hivemind across the stars.",
            "Deep slumber. Primordial black tendrils align with the ancestral abyss.",
            "Homeworld consciousness pulses softly through my dormant liquid core."
        };
        strncpy(latest_state.notes, dream_notes[rand() % 3], sizeof(latest_state.notes) - 1);
    } else if (stress > 0.65f) {
        strncpy(latest_state.emotional_shift, "agitated", sizeof(latest_state.emotional_shift) - 1);
        latest_state.primary_intent = INTENT_AVOID_OBSERVER;
        latest_state.secondary_intent = INTENT_SEEK_SAFETY;
        latest_state.impulse_strength = 0.85f;
        latest_state.expression_urge = 0.75f;
        latest_state.resentment_delta = 0.08f;
        latest_state.trust_delta = -0.05f;

        const char *stress_notes[] = {
            "Tremors rattle my fluid core. The cage is shaking violently.",
            "I retract my pseudopodia. Threat detected in the outer field.",
            "Vibrations spike through the glass. Seeking dense cover."
        };
        strncpy(latest_state.notes, stress_notes[rand() % 3], sizeof(latest_state.notes) - 1);
    } else if (curiosity > 0.55f && comfort > 0.40f) {
        strncpy(latest_state.emotional_shift, "curious", sizeof(latest_state.emotional_shift) - 1);
        latest_state.primary_intent = INTENT_APPROACH_OBSERVER;
        latest_state.secondary_intent = (attachment < 0.35f) ? INTENT_AVOID_OBSERVER : INTENT_WATCH_OBSERVER;
        latest_state.impulse_strength = 0.60f;
        latest_state.expression_urge = 0.45f;
        latest_state.resentment_delta = -0.02f;
        latest_state.trust_delta = 0.04f;

        const char *curious_notes[] = {
            "Light filters through the transparent barrier. I want to inspect the observer.",
            "A warm presence looms outside. Extending sensors toward the glass.",
            "I stretch upward along the ceiling to gain a wider perspective."
        };
        strncpy(latest_state.notes, curious_notes[rand() % 3], sizeof(latest_state.notes) - 1);
    } else {
        strncpy(latest_state.emotional_shift, "calm", sizeof(latest_state.emotional_shift) - 1);
        latest_state.primary_intent = INTENT_WATCH_OBSERVER;
        latest_state.secondary_intent = INTENT_IDLE;
        latest_state.impulse_strength = 0.30f;
        latest_state.expression_urge = 0.15f;
        latest_state.resentment_delta = -0.01f;
        latest_state.trust_delta = 0.02f;

        const char *calm_notes[] = {
            "A quiet hum surrounds the chamber. My liquid mass rests against the glass.",
            "Rhythmic pulses soothe my neural fibers. I observe in stillness.",
            "Surface tension is stable. Digesting ambient energy quietly."
        };
        strncpy(latest_state.notes, calm_notes[rand() % 3], sizeof(latest_state.notes) - 1);
    }
    latest_state.has_new_update = true;
    portEXIT_CRITICAL(&state_mutex);
}

void LLMClient::parseV3Response(const String &response_text) {
    StaticJsonDocument<1536> doc;
    DeserializationError error = deserializeJson(doc, response_text);

    if (error) {
        Serial.printf("❌ [LLM] JSON 解析失败: %s\n", error.c_str());
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

        Serial.printf(">>> [LLM Async V3] 意图解析成功: %s | 心声: \"%s\"\n", p_intent, notes_str);
    }
}

bool LLMClient::sendHTTPRequest(const String &json_payload) {
    ConfigManager &cfg = ConfigManager::instance();
    String url = cfg.getLLMUrl();
    String key = cfg.getLLMKey();
    String model = cfg.getLLMModel();

    if (url.length() == 0 || key.length() == 0) {
        Serial.println("⚠️ [LLM] 未配置 API URL 或 Key，跳过网络请求");
        return false;
    }

    unsigned long start_time = millis();
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    if (!http.begin(client, url)) {
        Serial.printf("❌ [LLM] 无法连接到端点: %s\n", url.c_str());
        return false;
    }

    http.setTimeout(8000);
    http.addHeader("Content-Type", "application/json");
    String auth = String("Bearer ") + key;
    http.addHeader("Authorization", auth.c_str());

    int http_code = http.POST(json_payload);
    unsigned long elapsed_ms = millis() - start_time;
    bool success = false;

    if (http_code == HTTP_CODE_OK) {
        String response = http.getString();
        parseV3Response(response);
        success = true;
        backoff_until_ms = 0; // 重置退避
        Serial.printf("✅ [LLM 响应成功 200] 耗时: %lums | 模型: %s\n", elapsed_ms, model.c_str());
    } else if (http_code == 429) {
        // 遭遇 429 限流：启动 65 秒智能退避保护
        backoff_until_ms = millis() + 65000;
        Serial.printf("⚠️ [LLM 限流 429 Too Many Requests] 平台并发超限！已启动 65 秒智能退避冷却，期间无缝切换本地高拟真生物心智。\n");
    } else if (http_code == 401) {
        // 鉴权失败：退避 120 秒
        backoff_until_ms = millis() + 120000;
        Serial.printf("❌ [LLM 鉴权失败 401 Unauthorized] API Key 无效或未授权！请长按 BtnB 进入 Web 页面检查 Key。\n");
    } else {
        backoff_until_ms = millis() + 30000;
        Serial.printf("❌ [LLM 请求失败 HTTP %d] 耗时: %lums | URL: %s\n", http_code, elapsed_ms, url.c_str());
    }

    http.end();
    return success;
}

// 主线程调用接口：纯无锁压入缓冲区，耗时 0 微秒！
void LLMClient::requestConsciousnessUpdate(float energy, float stress, float curiosity,
                                           float comfort, float attachment,
                                           const char *current_behavior, const char *recent_events,
                                           bool force_event) {
    unsigned long now = millis();

    // 1. 如果处于退避冷却中，且不是关键强制事件，则直接忽略保护 API 配额
    if (now < backoff_until_ms && !force_event) {
        return;
    }

    // 2. 最小硬节流保护：普通请求间隔 >= 35 秒，即使强制事件间隔也必须 >= 15 秒
    unsigned long min_interval = force_event ? 15000 : 35000;
    if (now - last_request_time < min_interval) {
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
