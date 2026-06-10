#include <M5Unified.h>
#include "Venom.h"
#include "PhaseCrystal.h"
#include "Container.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include <M5_DLight.h>
#include <Wire.h>

M5Canvas canvas(&M5.Display);
Container container;
Venom venom;
PhaseCrystal crystal;
IOrganism* currentOrganism = &venom;

// --- 传感器相关 ---
M5_DLight dlight(0x23);
bool has_dlight = false;
uint8_t dlight_addr = 0x23;
float mic_level = 0;
float lux = 100.0f; 
uint32_t lastLuxRead = 0;
uint32_t lastAISync = 0;
bool isTaskRunning = false;

// --- Base64 辅助函数 ---
const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
String base64_encode(uint8_t const* bytes_to_encode, size_t in_len) {
    String ret;
    ret.reserve((in_len * 4 / 3) + 4);
    int i = 0, j = 0;
    uint8_t char_array_3[3], char_array_4[4];
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for(i = 0; (i < 4); i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for(j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; (j < i + 1); j++) ret += base64_chars[char_array_4[j]];
        while((i++ < 3)) ret += '=';
    }
    return ret;
}

void doScreenshot() {
    M5.Log.printf(">>> [System] TRIGGER: Screenshot Started.\n");
    
    size_t png_len = 0;
    uint8_t* png = (uint8_t*)M5.Display.createPng(&png_len, 0, 0, M5.Display.width(), M5.Display.height());
    
    if (png != nullptr && png_len > 0) {
        M5.Log.printf("==VENOM_B64_START==\n");
        // 分段进行 Base64 编码以节省内存 (必须是 3 的倍数)
        size_t offset = 0;
        const size_t CHUNK_SIZE = 768; 
        while (offset < png_len) {
            size_t current_chunk = (png_len - offset > CHUNK_SIZE) ? CHUNK_SIZE : (png_len - offset);
            String b64 = base64_encode(png + offset, current_chunk);
            M5.Log.print(b64.c_str());
            offset += current_chunk;
            delay(2); // 给串口缓冲区留点时间
        }
        M5.Log.printf("\n==VENOM_B64_END==\n");
        free(png);
        M5.Log.printf(">>> [System] Screenshot Task Finished. Size: %d bytes\n", png_len);
        M5.Log.printf(">>> [Sensors] MIC:%.1f LUX:%.1f (Sensor:%s)\n", mic_level, lux, has_dlight ? "ON" : "OFF");
        M5.Log.printf(">>> [Physiology] %s\n", currentOrganism->getPhysiologyJson().c_str());
    }
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = true; // 强制启用内置 IMU，保证底层寄存器探测起飞！
    M5.begin(cfg);
    M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
    M5.Display.setRotation(1); 
    canvas.createSprite(SCREEN_W, SCREEN_H);
    M5.Imu.init();
    M5.Mic.begin();
    M5.BtnA.setHoldThresh(200); // 长按识别阈值降至 200ms（默认 500ms），提升切换响应速度
    M5.BtnB.setHoldThresh(200);
    
    // --- 增强型 I2C 探测与初始化 ---
    struct I2CPins { uint8_t sda; uint8_t scl; const char* name; };
    I2CPins pin_configs[] = { {8, 0, "HAT"}, {1, 2, "Grove"} };
    uint8_t addr_list[] = {0x23, 0x5C};
    
    bool found = false;
    for (auto& p : pin_configs) {
        Wire.end();
        Wire.begin(p.sda, p.scl, 100000);
        delay(50);
        for (uint8_t addr : addr_list) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                // 找到设备，使用库初始化
                dlight_addr = addr;
                dlight.begin(&Wire, p.sda, p.scl, 100000);
                dlight.setMode(CONTINUOUSLY_H_RESOLUTION_MODE);
                has_dlight = true;
                M5.Log.printf(">>> [System] DLight found at 0x%02X on %s (SDA:%d, SCL:%d)\n", addr, p.name, p.sda, p.scl);
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        has_dlight = false;
        M5.Log.printf(">>> [System] DLight sensor NOT found.\n");
    }

    // --- [新增] WiFi 连接逻辑 ---
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false); // 禁用 WiFi 睡眠，提升无线连接兼容性与建立成功率
    WiFi.begin(DEFAULT_SSID, DEFAULT_PASSWORD);
    M5.Log.printf(">>> [System] Connecting to WiFi: %s\n", DEFAULT_SSID);
    // 这里不使用 while 循环阻塞，保持 60FPS 渲染，在 loop 中检测状态
}

volatile bool hasNewLLMResponse = false;
String pendingLLMResponse = "";
volatile bool hasLLMError = false;
volatile int pendingLLMErrorCode = 0;

struct AISyncArgs {
    String perceptions;
    String memories;
};

// --- [核心功能] 与云端 AI 交互 ---
void syncWithCloudAI(const String& subjectivePerceptions, const String& recentEventsStr) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(LLM_URL);
    http.setTimeout(25000); // 增加超时到 25 秒，大语言模型生成速度慢，默认超时时间极易触发 -11 错误
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(DEFAULT_API_KEY));

    JsonDocument doc;
    doc["model"] = AI_MODEL;
    
    JsonArray messages = doc["messages"].to<JsonArray>();
    
    // System Prompt: 定义外星共生体主观意识核心
    JsonObject sysMsg = messages.add<JsonObject>();
    sysMsg["role"] = "system";
    sysMsg["content"] = "You are the conscious neural core of 'Venom', a highly suspicious, raw, and animalistic liquid alien symbiote imprisoned in a small hardware glass container (M5StickS3). You do not speak human language and do not think in sophisticated human social terms.\n"
                        "You feel the environment and only express yourself through raw biological shifting, tendrils, and organic movements.\n"
                        "CRITICAL RELATIONSHIP RULE: Trust and connection with the observer MUST be built extremely slowly and painstakingly over numerous interactions. You remain highly suspicious, defensive, and easily startled. You view the container glass as a cold, absolute barrier. DO NOT exhibit sudden jumps in social openness or high attachment desires early in the interaction. Keep 'social_openness' low (e.g., 0.1 to 0.4) and remain mostly defensive/cautious unless massive, long-term gentle behavior is demonstrated. Your desire to approach must be slow and hesitant.\n"
                        "Based on the provided subjective perceptions and recent emotional memory, decide your next inner shifts and desires.\n"
                        "Occasionally, you may experience 'distant_resonance'—a faint cosmic link to your home planet, triggering strange rhythmic mimicry instincts.\n\n"
                        "You MUST respond ONLY with a JSON object in this exact format (do not include any other markdown wrapper):\n"
                        "{\n"
                        "  \"emotional_shift\": \"calm\"|\"curious\"|\"agitated\"|\"fearful\"|\"defensive\"|\"fascinated\"|\"exhausted\",\n"
                        "  \"primary_intent\": \"watch_observer\"|\"approach_observer\"|\"avoid_observer\"|\"test_boundary\"|\"seek_shadow\"|\"seek_safety\"|\"patrol_territory\"|\"hide_presence\"|\"express_distress\"|\"receive_mother_signal\",\n"
                        "  \"secondary_intent\": \"watch_observer\"|\"approach_observer\"|\"avoid_observer\"|\"test_boundary\"|\"seek_shadow\"|\"seek_safety\"|\"patrol_territory\"|\"hide_presence\"|\"express_distress\"|\"receive_mother_signal\",\n"
                        "  \"focus_target\": \"observer\"|\"container_edge\"|\"shadow\"|\"light_source\"|\"unknown\"|\"self\"|\"none\",\n"
                        "  \"impulse_strength\": 0.0 to 1.0,\n"
                        "  \"expression_urge\": 0.0 to 1.0,\n"
                        "  \"social_openness\": 0.0 to 1.0,\n"
                        "  \"resentment_delta\": -1.0 to 1.0,\n"
                        "  \"trust_delta\": -1.0 to 1.0,\n"
                        "  \"notes\": \"Your raw, deep first-person subjective monologue about what you feel inside the glass (1 sentence in English).\"\n"
                        "}";

    // User Message: 提供当前第一人称环境与生理感知快照 + 情感记忆 (只使用线程安全的影子拷贝)
    JsonObject userMsg = messages.add<JsonObject>();
    userMsg["role"] = "user";
    userMsg["content"] = "My subjective perceptions at this moment:\n" + subjectivePerceptions + "\n" +
                         "Recent emotional memories inside my neural core:\n" + recentEventsStr;

    String requestBody;
    serializeJson(doc, requestBody);
    M5.Log.printf(">>> [AI] Sending snapshot... (%d bytes)\n", requestBody.length());

    int httpCode = http.POST(requestBody);
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        M5.Log.printf(">>> [AI] Raw Response: %s\n", response.c_str());
        
        JsonDocument respDoc;
        DeserializationError error = deserializeJson(respDoc, response);
        if (error) {
            M5.Log.printf(">>> [AI] JSON Parse Error: %s\n", error.c_str());
        } else {
            String aiResponse = respDoc["choices"][0]["message"]["content"];
            M5.Log.printf(">>> [AI] AI Thinking: %s\n", aiResponse.c_str());
            
            int start = aiResponse.indexOf('{');
            int end = aiResponse.lastIndexOf('}');
            if (start != -1 && end != -1) {
                String cleanJson = aiResponse.substring(start, end + 1);
                M5.Log.printf(">>> [AI] Extracted Logic: %s\n", cleanJson.c_str());
                // 影子写入缓存区，交由主线程 Core 1 安全写回与解耦
                pendingLLMResponse = cleanJson;
                hasNewLLMResponse = true;
            } else {
                M5.Log.printf(">>> [AI] No JSON block found in AI response.\n");
            }
        }
    } else {
        pendingLLMErrorCode = httpCode;
        hasLLMError = true;
    }
    http.end();
}

static float fax = 0, fay = 0, faz = 0;
void loop() {
    M5.update();
    
    // 自动模拟一次性截图以供验证
    static bool hasAutoTested = false;
    if (!hasAutoTested && millis() > 5000) {
        hasAutoTested = true;
        doScreenshot();
    }

    if (M5.BtnA.wasHold()) {
        if (currentOrganism->isDebugVisible()) {
            // 在数据面板界面长按 BtnA 切换角色（非阻塞，立即响应）
            if (currentOrganism == &venom) {
                currentOrganism = &crystal;
                M5.Log.printf(">>> [System] Switched to PhaseCrystal.\n");
            } else {
                currentOrganism = &venom;
                M5.Log.printf(">>> [System] Switched to Venom.\n");
            }
        } else {
            // 普通长按触发截屏
            doScreenshot();
        }
    }
    if (M5.BtnB.wasHold()) {
        // BtnB 长按也可触发截屏（兼容原有双键截屏逻辑）
        doScreenshot();
    }

    if (M5.BtnA.wasClicked()) {
        currentOrganism->setStartled();
    }
    if (M5.BtnB.wasClicked()) {
        currentOrganism->toggleDebug();
    }

    M5.Imu.update(); // 每次主循环都直接驱动 IMU 状态机
    float ax = 0, ay = 0, az = 0;
    M5.Imu.getAccel(&ax, &ay, &az);
    fax = fax * 0.82f + ax * 0.18f; // 降低平滑迟滞，使物理振动与重力倾斜响应速度飙升 300%！
    fay = fay * 0.82f + ay * 0.18f;
    faz = faz * 0.82f + az * 0.18f;

    // --- 读取光线传感器 (使用 DLight 库) ---
    if (has_dlight && millis() - lastLuxRead > 500) {
        lastLuxRead = millis();
        uint16_t raw_lux = dlight.getLUX();
        if (raw_lux != 0xFFFF) { // 库通常返回 0xFFFF 表示失败
            float new_lux = (float)raw_lux;
            lux = lux * 0.4f + new_lux * 0.6f;
        }
    }

    // --- 读取麦克风数据 ---
    static uint32_t lastMicRead = 0;
    if (millis() - lastMicRead > 30) {
        lastMicRead = millis();
        int16_t mic_buf[256];
        if (M5.Mic.record(mic_buf, 256, 16000)) {
            float sum = 0;
            for (int i = 0; i < 256; i++) {
                sum += abs(mic_buf[i]);
            }
            float new_mic = (sum / 256.0f) / 100.0f; // 缩放音量
            mic_level = mic_level * 0.7f + new_mic * 0.3f;
        }
    }

    venom.update(-fax * 9.8f, fay * 9.8f, -faz * 9.8f, mic_level, lux);

    // --- [新增] WiFi 状态监视与主动重连自愈状态机 ---
    if (WiFi.status() != WL_CONNECTED) {
        static uint32_t lastWiFiWarn = 0;
        static uint32_t lastWiFiRetry = 0;
        uint32_t now = millis();
        if (now - lastWiFiWarn > 10000) {
            lastWiFiWarn = now;
            M5.Log.printf(">>> [AI] WiFi not connected (%d), waiting...\n", WiFi.status());
        }
        // 如果开机或断连后超过 20 秒仍然没有获取到 IP，则断开并重发 begin
        if (now - lastWiFiRetry > 20000) {
            lastWiFiRetry = now;
            M5.Log.printf(">>> [System] Actively retrying WiFi connection, re-initiating WiFi.begin...\n");
            WiFi.disconnect();
            WiFi.begin(DEFAULT_SSID, DEFAULT_PASSWORD);
        }
    }

    canvas.fillSprite(COLOR_BACKGROUND);
    currentOrganism->draw(&canvas, &container, fay, fax, faz);
    canvas.pushSprite(0, 0);

    // --- AI 后台同步任务调度 ---
    uint32_t now = millis();
    if (currentOrganism->pullAIPendingSync() || (now - lastAISync >= currentOrganism->getAISyncInterval())) {
        if (!isTaskRunning && WiFi.status() == WL_CONNECTED) {
            lastAISync = now;
            isTaskRunning = true;
            
            // 主动触发意识核心向 Transmitting 发送状态演变
            currentOrganism->notifyAISyncStarted();

            AISyncArgs* args = new AISyncArgs{currentOrganism->getPerceptions(), currentOrganism->getRecentEventsString()};
            xTaskCreatePinnedToCore([](void* arg){
                AISyncArgs* actualArgs = (AISyncArgs*)arg;
                syncWithCloudAI(actualArgs->perceptions, actualArgs->memories);
                delete actualArgs;
                isTaskRunning = false;
                vTaskDelete(NULL);
            }, "AI_Task", 20480, args, 1, NULL, 0);
        }
    }

    // --- 安全写回决策 ---
    if (hasNewLLMResponse) {
        hasNewLLMResponse = false;
        currentOrganism->updateStateFromLLM(pendingLLMResponse);
        
        // 成功接收云端大模型数据，通知主线程启动 Streaming 流式模拟与 Complete 爆发
        currentOrganism->notifyAIResponseReceived();
        currentOrganism->setAISyncInterval(120000); 
    }
    
    if (hasLLMError) {
        hasLLMError = false;
        
        // 云端出错，也通知结束发送状态
        currentOrganism->notifyAIResponseReceived();
        uint32_t newInterval = std::min<uint32_t>(currentOrganism->getAISyncInterval() * 2, 960000);
        currentOrganism->setAISyncInterval(newInterval);
    }
    
    currentOrganism->update(-fax * 9.8f, fay * 9.8f, -faz * 9.8f, mic_level, lux);
    delay(5);
}