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
#include "PreyBugSystem.h"
#include "PredatorSystem.h"

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
static PreyBugSystem      prey_bugs;
static PredatorSystem     predator;

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

// ==========================================
// 【多模式高质感仿生触觉震动引擎 (Soft Haptic Engine)】
// ==========================================
enum HapticPattern {
    HAPTIC_NONE = 0,
    HAPTIC_TICK,         // 1. 超轻按键微触点 (12ms, PWM 55) - 短按 HUD / 退出
    HAPTIC_SLING,        // 2. 挥甩离手轻风 (18ms, PWM 70) - 甩掷起飞
    HAPTIC_SPLAT,        // 3. 软泥啪嗒拍扁吸附 (28ms, PWM 100 渐弱) - 撞墙黏住
    HAPTIC_JOLT_DOUBLE,  // 4. 生物受惊双连微颤 (15ms+20ms停+15ms, PWM 80) - 戳弄受惊
    HAPTIC_SWALLOW,      // 5. 吞噬咽下满足微震 (32ms, PWM 75) - 捕食吞噬
    HAPTIC_LONG_PULSE    // 6. 长按配网温和长震 (85ms, PWM 115) - 开启 AP 热点
};

struct HapticController {
    HapticPattern pattern = HAPTIC_NONE;
    unsigned long start_ms = 0;

    void trigger(HapticPattern p) {
        pattern = p;
        start_ms = millis();
        update();
    }

    void update() {
        if (pattern == HAPTIC_NONE) return;
        unsigned long elapsed = millis() - start_ms;

        switch (pattern) {
            case HAPTIC_TICK:
                if (elapsed < 12) {
                    ledcWrite(VIBR_PWM_CHANNEL, 55); // 细腻温和微触
                } else {
                    ledcWrite(VIBR_PWM_CHANNEL, 0);
                    pattern = HAPTIC_NONE;
                }
                break;

            case HAPTIC_SLING:
                if (elapsed < 18) {
                    ledcWrite(VIBR_PWM_CHANNEL, 70); // 挥甩起飞
                } else {
                    ledcWrite(VIBR_PWM_CHANNEL, 0);
                    pattern = HAPTIC_NONE;
                }
                break;

            case HAPTIC_SPLAT:
                if (elapsed < 28) {
                    // 软泥拍扁渐弱触感 (100 -> 40)
                    uint8_t p = 100 - (elapsed * 60 / 28);
                    ledcWrite(VIBR_PWM_CHANNEL, p);
                } else {
                    ledcWrite(VIBR_PWM_CHANNEL, 0);
                    pattern = HAPTIC_NONE;
                }
                break;

            case HAPTIC_JOLT_DOUBLE:
                // 双脉冲生物心跳微颤 (0~14ms 震, 14~32ms 停, 32~48ms 震)
                if (elapsed < 14) {
                    ledcWrite(VIBR_PWM_CHANNEL, 80);
                } else if (elapsed < 32) {
                    ledcWrite(VIBR_PWM_CHANNEL, 0);
                } else if (elapsed < 48) {
                    ledcWrite(VIBR_PWM_CHANNEL, 65);
                } else {
                    ledcWrite(VIBR_PWM_CHANNEL, 0);
                    pattern = HAPTIC_NONE;
                }
                break;

            case HAPTIC_SWALLOW:
                if (elapsed < 32) {
                    ledcWrite(VIBR_PWM_CHANNEL, 75); // 吞咽柔和微震
                } else {
                    ledcWrite(VIBR_PWM_CHANNEL, 0);
                    pattern = HAPTIC_NONE;
                }
                break;

            case HAPTIC_LONG_PULSE:
                if (elapsed < 85) {
                    ledcWrite(VIBR_PWM_CHANNEL, 115); // 温和长震，绝不震手手麻
                } else {
                    ledcWrite(VIBR_PWM_CHANNEL, 0);
                    pattern = HAPTIC_NONE;
                }
                break;

            default:
                ledcWrite(VIBR_PWM_CHANNEL, 0);
                pattern = HAPTIC_NONE;
                break;
        }
    }
};

static HapticController haptics;

// 麦克风三频段音频分析缓存
static int16_t mic_raw_buffer[128];
static unsigned long last_mic_sample_ms = 0;

// LLM 意图请求调度计时
static unsigned long last_llm_request_ms = 0;

static float smoothed_mic_db = 32.0f;

static void processAudioBands() {
    if (!M5.Mic.isEnabled()) return;

    if (millis() - last_mic_sample_ms >= 15) {
        last_mic_sample_ms = millis();
        if (M5.Mic.record(mic_raw_buffer, 128, 8000)) {
            int16_t min_v = 32767;
            int16_t max_v = -32768;
            int32_t sum_raw = 0;

            for (int i = 0; i < 128; ++i) {
                int16_t val = mic_raw_buffer[i];
                if (val < min_v) min_v = val;
                if (val > max_v) max_v = val;
                sum_raw += val;
            }

            // 1. 直流偏置消除 (DC Bias Removal)
            float dc_mean = (float)sum_raw / 128.0f;
            float peak_to_peak = (float)(max_v - min_v);

            float ac_sum_sq = 0.0f;
            float low_accum = 0.0f;
            float mid_accum = 0.0f;
            float high_accum = 0.0f;
            int zero_crossings = 0;
            float prev_sample = 0.0f;

            for (int i = 0; i < 128; ++i) {
                float ac_sample = ((float)mic_raw_buffer[i] - dc_mean) / 32768.0f;
                float abs_ac = std::abs(ac_sample);
                ac_sum_sq += ac_sample * ac_sample;

                if ((ac_sample > 0.001f && prev_sample < -0.001f) || (ac_sample < -0.001f && prev_sample > 0.001f)) {
                    zero_crossings++;
                }

                // 低频（低过零率/大振幅重低音）、中频（旋律）、高频（高过零率/镲片）
                low_accum += abs_ac;
                if (i % 2 == 0) mid_accum += abs_ac;
                high_accum += abs_ac;

                prev_sample = ac_sample;
            }

            // 2. 交流 RMS 均方根与气流冲击能量 (Blast Airflow Detection)
            float ac_rms = std::sqrt(ac_sum_sq / 128.0f);
            float p2p_ratio = peak_to_peak / 32768.0f;

            // 3. 连续平滑真实声学分贝映射 (无阶梯断层: 安静 30~36dB, 说话 48~60dB, 音乐 62~78dB, 吹气 80~95dB)
            float base_energy = ac_rms * 1.8f + p2p_ratio * 0.35f;
            float raw_db = 20.0f * std::log10(std::max(0.0006f, base_energy)) + 93.0f;
            raw_db = std::max(30.0f, std::min(96.0f, raw_db));

            // 4. 快攻慢释 (Attack-Release) 包络滤波
            if (raw_db > smoothed_mic_db) {
                smoothed_mic_db = smoothed_mic_db * 0.45f + raw_db * 0.55f; // 快速感知音乐节拍与吹气
            } else {
                smoothed_mic_db = smoothed_mic_db * 0.70f + raw_db * 0.30f; // 随节拍平稳起伏
            }

            // 5. 真实三频段动态归一化 (保留连续音乐律动，杜绝硬切断)
            float low_energy = std::max(0.0f, std::min(1.0f, (ac_rms - 0.003f) * 16.0f + (zero_crossings < 18 ? 0.35f * ac_rms * 20.0f : 0.0f)));
            float mid_energy = std::max(0.0f, std::min(1.0f, (ac_rms - 0.004f) * 14.0f));
            float high_energy = std::max(0.0f, std::min(1.0f, (p2p_ratio > 0.35f ? (p2p_ratio - 0.35f) * 2.2f : 0.0f) + (zero_crossings > 30 ? 0.3f : 0.0f)));

            physiology.feedAudioBands(low_energy, mid_energy, high_energy);
            physiology.feedMicDecibels(smoothed_mic_db);
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
    prey_bugs.init();
    predator.init();
    renderer.init(&canvas);

    prev_micros = micros();
    Serial.println("\n>>> [Venom Symbiote] Fluid Symbol Field Fusion Ready! <<<");
    Serial.println(">>> Commands: 'symbol <eye|?|!|x|o|heart|warning|splash>', 'screenshot', 'leak', 'hud' <<<");
}

void loop() {
    M5.update();
    haptics.update();

    // 0. 配网热点模式接管
    if (portal.isRunning()) {
        portal.update();
        if (M5.BtnA.wasPressed()) {
            haptics.trigger(HAPTIC_TICK);
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
        haptics.trigger(HAPTIC_SLING); // 甩飞轻盈离手感
    }

    // 2. 音频分析与节拍检测
    processAudioBands();
    float total_g_shake = std::max(0.0f, std::abs(raw_ax) + std::abs(raw_ay) + std::abs(raw_az) - 1.0f);
    rhythm.update(dt, total_g_shake, physiology.getAudioHigh());

    // 3. 按键与交互
    bool btn_a_pressed = M5.BtnA.wasPressed();

    // 长按 BtnB (1200ms) 触发开启 HTTP Web 配网热点
    if (M5.BtnB.pressedFor(1200)) {
        haptics.trigger(HAPTIC_LONG_PULSE); // 温和长震提示
        portal.start(renderer.getCanvas());
        return;
    }

    if (btn_a_pressed) {
        ai.triggerJolt(skeleton, metaballs, 1.2f);
        haptics.trigger(HAPTIC_JOLT_DOUBLE); // 受惊双连微颤
        llm.requestConsciousnessUpdate(physiology.getEnergy(), 0.9f, 0.8f, 0.1f,
                                       physiology.getAttachment(), "JOLT", "poked_by_human", true);
    }

    if (M5.BtnB.wasClicked()) {
        renderer.toggleHUD();
        haptics.trigger(HAPTIC_TICK); // 清脆微触感
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
            renderer.triggerMindEcho(llm.getLatestNotes());
            Serial.printf(">>> [LEAK] Mind Echo Triggered: \"%s\"\n", llm.getLatestNotes());
        } else if (cmd.startsWith("symbol")) {
            String sym = cmd.substring(6);
            sym.trim();
            fluid_symbols.trigger(sym);
        } else if (cmd.equalsIgnoreCase("theme")) {
            renderer.nextTheme();
        }
    }

    // 5. LLM 意识系统异步请求与意图更新 (平稳 50 秒基础周期，保护 API 配额)
    if (millis() - last_llm_request_ms >= 50000) {
        last_llm_request_ms = millis();
        llm.requestConsciousnessUpdate(physiology.getEnergy(), physiology.getStress(),
                                       physiology.getCuriosity(), physiology.getComfort(),
                                       physiology.getAttachment(), ai.getStateName(),
                                       (total_g_shake > 0.4f) ? "shake" : "calm", false);
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

    // 8.1 活体小虫子生态与捕食进食系统更新
    prey_bugs.update(dt, hx, hy);
    predator.update(dt, prey_bugs, skeleton, physiology, metaballs);

    // 9. AI 行为状态机更新 (注入猎物注视感知)
    ai.updateSensors(raw_ax, raw_ay, raw_az, physiology, btn_a_pressed);
    ai.update(dt, skeleton, metaballs, tentacles, physiology, relationship, expression, v3_state, &prey_bugs);

    // 10. 骨架动力学更新 (注入低频重音脉动与节拍鼓包)
    float crawl_bx, crawl_by;
    ai.getCrawlBias(crawl_bx, crawl_by);
    skeleton.update(dt, gx, gy, crawl_bx, crawl_by,
                    physiology.getNeuroTension(), physiology.getSpikeIntensity(),
                    ai.getRespiration(), is_upside_down,
                    physiology.getRawAudioLow());

    // 10.1 撞击“啪嗒”事件检测与触觉/飞溅联动 (Sticky Splat Feedback)
    float imp_spd, hit_x, hit_y;
    if (skeleton.checkAndConsumeImpactEvent(imp_spd, hit_x, hit_y)) {
        // “啪嗒”拍在玻璃上的渐弱软泥微震反馈 (绝不震手手麻)
        haptics.trigger(HAPTIC_SPLAT);
        // 瞬间向外爆射 6 根应力尖刺
        metaballs.triggerSpikeBurst(6, 1.35f);
        // 飞溅 3 颗微小黏液滴
        for (int k = 0; k < 3; ++k) {
            float sp_vx = ((rand() % 80) - 40) * 0.08f;
            float sp_vy = ((rand() % 80) - 40) * 0.08f;
            metaballs.spawnDroplet(hit_x + sp_vx * 2.0f, hit_y + sp_vy * 2.0f, sp_vx, sp_vy, 2.5f, true);
        }

        // 撞击贴壁后瞬间激发狂暴反弹爬行，立刻射出触手向开阔地带挣脱爬行！
        ai.triggerReactiveCrawl(skeleton, tentacles);
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

    // 13. 渲染输出 (包含小虫子与捕食进食视觉)
    renderer.render(skeleton, metaballs, eye, tentacles, ai, physiology,
                    voronoi, fluid_symbols, relationship, expression, v3_state, current_fps,
                    &prey_bugs, &predator);
    canvas.pushSprite(0, 0);

    delay(2);
}
