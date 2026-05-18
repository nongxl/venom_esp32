#include <M5Unified.h>
#include "Venom.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include <M5_DLight.h>
#include <Wire.h>

M5Canvas canvas(&M5.Display);
Venom venom;

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
        M5.Log.printf(">>> [Physiology] %s\n", venom.getPhysiologyJson().c_str());
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
    M5.Display.setRotation(1); 
    canvas.createSprite(SCREEN_W, SCREEN_H);
    M5.Imu.init();
    M5.Mic.begin();
    
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
    WiFi.begin(DEFAULT_SSID, DEFAULT_PASSWORD);
    M5.Log.printf(">>> [System] Connecting to WiFi: %s\n", DEFAULT_SSID);
    // 这里不使用 while 循环阻塞，保持 60FPS 渲染，在 loop 中检测状态
}

// --- [核心功能] 与云端 AI 交互 ---
void syncWithCloudAI() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(LLM_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(DEFAULT_API_KEY));

    JsonDocument doc;
    doc["model"] = AI_MODEL;
    
    JsonArray messages = doc["messages"].to<JsonArray>();
    
    // System Prompt: 定义毒液的生物本质
    JsonObject sysMsg = messages.add<JsonObject>();
    sysMsg["role"] = "system";
    sysMsg["content"] = "You are the neural core of 'Venom', a biomimetic alien symbiote on an M5StickS3. "
                        "Analyze its current physiology/environment and decide its next emotional evolution. "
                        "Respond ONLY in JSON with fields: curiosity, comfort, attachment, energy, suggested_behavior (idle/explore/hiding/sleep/observe).";

    // User Message: 提供当前传感器快照
    JsonObject userMsg = messages.add<JsonObject>();
    userMsg["role"] = "user";
    userMsg["content"] = "Current State: " + venom.getPhysiologyJson() + 
                         " Environment: LUX=" + String(lux) + " MIC=" + String(mic_level);

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
                venom.updateStateFromLLM(cleanJson);
            } else {
                M5.Log.printf(">>> [AI] No JSON block found in AI response.\n");
            }
        }
    } else {
        M5.Log.printf(">>> [AI] Cloud sync failed, HTTP code: %d\n", httpCode);
    }
    http.end();
}

static float fax = 0, fay = 0, faz = 0;
void loop() {
    M5.update();

    if (M5.BtnA.isHolding() || M5.BtnB.isHolding()) {
        doScreenshot();
        while (M5.BtnA.isPressed() || M5.BtnB.isPressed()) {
            delay(10);
            M5.update();
        }
    }

    if (M5.BtnA.wasClicked()) {
        venom.setStartled();
    }
    if (M5.BtnB.wasClicked()) {
        venom.toggleDebug();
    }

    auto imu_update = M5.Imu.update();
    if (imu_update) {
        float ax, ay, az;
        M5.Imu.getAccel(&ax, &ay, &az);
        fax = fax * 0.95f + ax * 0.05f;
        fay = fay * 0.95f + ay * 0.05f;
        faz = faz * 0.95f + az * 0.05f;
    }

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

    // --- [优化] 异步云端决策任务 ---
    if (millis() - lastAISync > 60000 && !isTaskRunning) {
        if (WiFi.status() == WL_CONNECTED) {
            lastAISync = millis();
            isTaskRunning = true;
            M5.Log.printf(">>> [AI] Triggering cloud sync task...\n");
            // 创建一次性后台任务
            xTaskCreatePinnedToCore([](void* arg){
                bool* running = (bool*)arg;
                syncWithCloudAI();
                *running = false;
                vTaskDelete(NULL);
            }, "AI_Task", 8192, &isTaskRunning, 1, NULL, 0); 
        } else {
            // 每 10 秒提示一次 WiFi 未连接
            static uint32_t lastWiFiWarn = 0;
            if (millis() - lastWiFiWarn > 10000) {
                lastWiFiWarn = millis();
                M5.Log.printf(">>> [AI] WiFi not connected (%d), waiting...\n", WiFi.status());
            }
        }
    }

    canvas.fillSprite(COLOR_BACKGROUND); 
    venom.draw(&canvas, fay, fax, faz);
    canvas.pushSprite(0, 0);
    delay(5);
}