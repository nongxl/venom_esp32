#include <M5Unified.h>
#include <Preferences.h>
#include "config.h"
#include "PhysiologySystem.h"
#include "RelationshipSystem.h"
#include "FluidSymbolSystem.h"
#include "RhythmDetector.h"
#include "LLMClient.h"
#include "ExpressionLayer.h"
#include "VoronoiSurface.h"
#include "SkeletonSystem.h"
#include "MetaballSystem.h"
#include "EyeSystem.h"
#include "TentacleRenderer.h"
#include "CreatureAI.h"
#include "Renderer.h"
#include "ConfigManager.h"
#include "WebConfigPortal.h"

// ─────────────────────────────────────────────────────────────
//  系统全局实例
// ─────────────────────────────────────────────────────────────
static M5Canvas           canvas(&M5.Display);
static PhysiologySystem   physiology;
static RelationshipSystem relationship;
static FluidSymbolSystem  fluid_symbols;
static RhythmDetector     rhythm;
static LLMClient          llm;
static ExpressionLayer    expression;
static VoronoiSurface     voronoi;
static SkeletonSystem     skeleton;
static MetaballSystem     metaballs;
static EyeSystem          eye;
static TentacleRenderer   tentacles;
static CreatureAI         ai;
static Renderer           renderer;
static WebConfigPortal    portal;

// IMU 低通滤波值
static float imu_lpf_x = 0.0f;
static float imu_lpf_y = DEFAULT_GRAVITY_Y;
static float imu_lpf_z = 0.0f;

// 时间与帧率诊断
static unsigned long prev_micros = 0;
static float current_fps = 30.0f;
static float fps_calc_accumulator = 0.0f;
static int   fps_calc_frames = 0;
static unsigned long last_fps_update_ms = 0;

// 震动电机脉冲
static unsigned long vibrate_end_ms = 0;

// 麦克风三频段音频分析缓存
static int16_t mic_raw_buffer[128];
static unsigned long last_mic_sample_ms = 0;

// LLM 意图请求调度计时
static unsigned long last_llm_request_ms = 0;

void triggerVibration(int duration_ms, uint8_t power = 180) {
    ledcWrite(VIBR_PWM_CHANNEL, power);
    vibrate_end_ms = millis() + duration_ms;
}

static void processAudioBands() {
    if (!M5.Mic.isEnabled()) return;

    if (millis() - last_mic_sample_ms >= 20) {
        last_mic_sample_ms = millis();
        if (M5.Mic.record(mic_raw_buffer, 128, 8000)) {
            float low_energy = 0.0f;
            float mid_energy = 0.0f;
            float high_energy = 0.0f;
            int zero_crossings = 0;
            float prev_sample = 0.0f;

            for (int i = 0; i < 128; ++i) {
                float sample = (float)mic_raw_buffer[i] / 32768.0f;
                float abs_val = std::abs(sample);

                if ((sample > 0 && prev_sample < 0) || (sample < 0 && prev_sample > 0)) {
                    zero_crossings++;
                }

                if (i % 4 == 0) low_energy += abs_val;
                if (i % 2 == 0) mid_energy += abs_val;
                high_energy += abs_val;

                prev_sample = sample;
            }

            low_energy  = std::min(1.0f, (low_energy / 32.0f) * 2.5f);
            mid_energy  = std::min(1.0f, (mid_energy / 64.0f) * 3.0f);
            high_energy = std::min(1.0f, (high_energy / 128.0f) * 3.8f);

            if (zero_crossings > 35) {
                high_energy = std::min(1.0f, high_energy * 1.5f + 0.2f);
            }

            physiology.feedAudioBands(low_energy, mid_energy, high_energy);
        }
    }
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.internal_imu = true;
    cfg.internal_mic = true;
    cfg.internal_spk = true;
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.setBrightness(SYSTEM_BRIGHTNESS);
    canvas.setPsram(false);
    canvas.createSprite(SCREEN_W, SCREEN_H);

    M5.Imu.begin();
    M5.Mic.begin();
    M5.Speaker.setVolume(SYSTEM_VOLUME);

    pinMode(VIBR_PIN, OUTPUT);
    ledcSetup(VIBR_PWM_CHANNEL, VIBR_PWM_FREQ, VIBR_PWM_BITS);
    ledcAttachPin(VIBR_PIN, VIBR_PWM_CHANNEL);
    ledcWrite(VIBR_PWM_CHANNEL, 0);

    ConfigManager::instance().init();

    physiology.init();
    relationship.init();
    fluid_symbols.init();
    rhythm.init();
    llm.init();
    expression.init();
    voronoi.init();
    skeleton.init();
    metaballs.init();
    eye.init();
    tentacles.init();
    ai.init();
    renderer.init(&canvas);

    prev_micros = micros();
    Serial.println("\n>>> [Venom Symbiote] Fluid Symbol Field Fusion Ready! <<<");
    Serial.println(">>> Commands: 'symbol <eye|?|!|x|o|heart|warning|splash>', 'screenshot', 'leak', 'hud' <<<");
}

void loop() {
    M5.update();

    // 0. 配网热点模式接管
    if (portal.isRunning()) {
        portal.update();
        if (M5.BtnA.wasPressed()) {
            triggerVibration(40, 200);
            portal.stop();
            delay(100);
            ESP.restart();
        }
        return;
    }

    unsigned long now_micros = micros();
    float dt = (now_micros - prev_micros) * 0.000001f;
    prev_micros = now_micros;
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.001f) dt = 0.001f;

    fps_calc_accumulator += (1.0f / dt);
    fps_calc_frames++;
    if (millis() - last_fps_update_ms >= 500) {
        current_fps = fps_calc_accumulator / (float)fps_calc_frames;
        fps_calc_accumulator = 0.0f;
        fps_calc_frames = 0;
        last_fps_update_ms = millis();
    }

    if (vibrate_end_ms > 0 && millis() >= vibrate_end_ms) {
        ledcWrite(VIBR_PWM_CHANNEL, 0);
        vibrate_end_ms = 0;
    }

    // 1. 真实物理 IMU 坐标对齐 (M5StickS3 横屏 Rotation 1)
    float raw_ax = 0.0f, raw_ay = 0.0f, raw_az = 0.0f;
    if (M5.Imu.isEnabled()) {
        M5.Imu.getAccel(&raw_ax, &raw_ay, &raw_az);
    }
    // 横屏模式: 屏幕水平 X 对应 -raw_ax * 9.8，垂直 Y 对应 raw_ay * 9.8
    float cur_gx = -raw_ax * 9.8f;
    float cur_gy = raw_ay * 9.8f;

    imu_lpf_x = imu_lpf_x * 0.82f + cur_gx * 0.18f;
    imu_lpf_y = imu_lpf_y * 0.82f + cur_gy * 0.18f;
    imu_lpf_z = imu_lpf_z * 0.82f + raw_az * 0.18f;

    // 传感器静止死区过滤 (平放桌面时自然归零，彻底消灭右下角假引力)
    float gx = (std::abs(imu_lpf_x) > 0.8f) ? (imu_lpf_x * 0.45f) : 0.0f;
    float gy = (std::abs(imu_lpf_y) > 0.8f) ? (imu_lpf_y * 0.45f) : 0.0f;
    bool is_upside_down = (imu_lpf_y < -3.5f);

    // 1.1 甩动抛体干脆甩飞系统 (Dynamic Accel Shake Sling Physics)
    float acc_mag = std::sqrt(raw_ax * raw_ax + raw_ay * raw_ay + raw_az * raw_az);
    float dynamic_g = std::max(0.0f, acc_mag - 1.0f);
    static unsigned long last_sling_time_ms = 0;

    // 当用户用力甩动设备时 (动态合加速度 > 0.50g 即可 100% 灵敏触发)
    if (dynamic_g > 0.50f && (millis() - last_sling_time_ms > 250)) {
        last_sling_time_ms = millis();

        // 确定甩掷方向: 横屏模式下向右甩动 raw_ax < 0 -> dir_x > 0; 向上甩动 raw_ay < 0 -> dir_y < 0
        float dir_x = -raw_ax;
        float dir_y = raw_ay;
        float dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y);

        if (dir_len > 0.12f) {
            dir_x /= dir_len;
            dir_y /= dir_len;
        } else {
            dir_x = (rand() % 2 == 0) ? 1.0f : -1.0f;
            dir_y = 0.0f;
        }

        // 初速度 38.0 ~ 58.0 px/s (极速横跨屏幕直冲边界！)
        float throw_speed = 38.0f + dynamic_g * 14.0f;
        if (throw_speed > 58.0f) throw_speed = 58.0f;

        skeleton.triggerSlingThrow(dir_x, dir_y, throw_speed);
        tentacles.reset(); // 打断爪盘与触手
        ai.triggerStartle(1.5f);
        triggerVibration(25, 200);
    }

    // 2. 音频分析与节拍检测
    processAudioBands();
    float total_g_shake = std::max(0.0f, std::abs(raw_ax) + std::abs(raw_ay) + std::abs(raw_az) - 1.0f);
    rhythm.update(dt, total_g_shake, physiology.getAudioHigh());

    // 3. 按键与交互
    bool btn_a_pressed = M5.BtnA.wasPressed();

    // 长按 BtnB (1200ms) 触发开启 HTTP Web 配网热点
    if (M5.BtnB.pressedFor(1200)) {
        triggerVibration(120, 255);
        portal.start(renderer.getCanvas());
        return;
    }

    if (btn_a_pressed) {
        ai.triggerJolt(skeleton, metaballs, 1.2f);
        triggerVibration(50, 220);
    }

    if (M5.BtnB.wasClicked()) {
        renderer.toggleHUD();
        triggerVibration(25, 150);
    }

    // 4. 串口交互指令
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.equalsIgnoreCase("screenshot") || cmd.equalsIgnoreCase("s")) {
            renderer.sendScreenshotSerial();
        } else if (cmd.equalsIgnoreCase("hud")) {
            renderer.toggleHUD();
        } else if (cmd.equalsIgnoreCase("leak")) {
            renderer.toggleHUD();
            Serial.printf(">>> [LEAK] Mind Echo: \"%s\"\n", llm.getLatestNotes());
        } else if (cmd.startsWith("symbol")) {
            String sym = cmd.substring(6);
            sym.trim();
            fluid_symbols.trigger(sym);
        } else if (cmd.equalsIgnoreCase("theme")) {
            renderer.nextTheme();
        }
    }

    // 5. LLM 意识系统异步请求与意图更新
    if (millis() - last_llm_request_ms >= 18000) {
        last_llm_request_ms = millis();
        llm.requestConsciousnessUpdate(physiology.getEnergy(), physiology.getStress(),
                                       physiology.getCuriosity(), physiology.getComfort(),
                                       physiology.getAttachment(), ai.getStateName(),
                                       (total_g_shake > 0.4f) ? "shake" : "calm");
    }

    ConsciousnessStateV3 v3_state = llm.getLatestState();
    if (v3_state.has_new_update) {
        relationship.applyDeltas(v3_state.trust_delta, v3_state.resentment_delta, v3_state.social_openness);
    }

    // 6. 生理与关系系统更新
    physiology.update(dt, total_g_shake, is_upside_down, btn_a_pressed);
    relationship.update(dt, total_g_shake, (physiology.getStress() < 0.2f));

    // 7. 非语言表达层与液态符号系统更新
    expression.update(dt, v3_state, physiology, relationship, rhythm, fluid_symbols, skeleton, is_upside_down);
    fluid_symbols.update(dt);

    // 8. 毒液爬行头部物理擦除/重吸收墨迹
    float hx, hy;
    skeleton.getHeadPos(hx, hy);
    fluid_symbols.wipePoints(hx, hy, 22.0f);

    // 9. AI 行为状态机更新
    ai.updateSensors(raw_ax, raw_ay, raw_az, physiology, btn_a_pressed);
    ai.update(dt, skeleton, metaballs, tentacles, physiology, relationship, expression, v3_state);

    // 10. 骨架动力学更新
    float crawl_bx, crawl_by;
    ai.getCrawlBias(crawl_bx, crawl_by);
    skeleton.update(dt, gx, gy, crawl_bx, crawl_by,
                    physiology.getNeuroTension(), physiology.getSpikeIntensity(),
                    ai.getRespiration(), is_upside_down);

    // 10.1 撞击“啪嗒”事件检测与触觉/飞溅联动 (Sticky Splat Feedback)
    float imp_spd, hit_x, hit_y;
    if (skeleton.checkAndConsumeImpactEvent(imp_spd, hit_x, hit_y)) {
        // “啪嗒”拍在玻璃上的有力短促震动反馈
        triggerVibration(45, 255);
        // 瞬间向外爆射 6 根应力尖刺
        metaballs.triggerSpikeBurst(6, 1.35f);
        // 飞溅 3 颗微小黏液滴
        for (int k = 0; k < 3; ++k) {
            float sp_vx = ((rand() % 80) - 40) * 0.08f;
            float sp_vy = ((rand() % 80) - 40) * 0.08f;
            metaballs.spawnDroplet(hit_x + sp_vx * 2.0f, hit_y + sp_vy * 2.0f, sp_vx, sp_vy, 2.5f, true);
        }
    }

    // 11. Voronoi 细胞与标量场（含符号粒子融合）更新
    float look_x, look_y;
    ai.getLookTarget(look_x, look_y);
    voronoi.update(dt, skeleton, physiology, look_x, look_y);
    metaballs.update(dt, skeleton, gx, gy, physiology);
    metaballs.computeField(skeleton, physiology, fluid_symbols, gx, gy);

    // 12. 触手与眼睛系统更新
    tentacles.update(dt, skeleton, physiology, is_upside_down);
    eye.update(dt, skeleton, physiology, look_x, look_y, ai.isSleeping());

    // 13. 渲染输出
    renderer.render(skeleton, metaballs, eye, tentacles, ai, physiology,
                    voronoi, fluid_symbols, relationship, expression, v3_state, current_fps);
    canvas.pushSprite(0, 0);

    delay(2);
}
