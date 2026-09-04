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
static bool btn_b_hold_handled = false;

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
static float adaptive_noise_floor = 0.030f; // 自适应环境本底底噪估计器

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
            int zero_crossings = 0;
            float prev_sample = 0.0f;

            for (int i = 0; i < 128; ++i) {
                float ac_sample = ((float)mic_raw_buffer[i] - dc_mean) / 32768.0f;
                ac_sum_sq += ac_sample * ac_sample;

                if ((ac_sample > 0.002f && prev_sample < -0.002f) || (ac_sample < -0.002f && prev_sample > 0.002f)) {
                    zero_crossings++;
                }
                prev_sample = ac_sample;
            }

            // 2. 交流 RMS 均方根与气流冲击能量 (Blast Airflow Detection)
            float ac_rms = std::sqrt(ac_sum_sq / 128.0f);
            float p2p_ratio = peak_to_peak / 32768.0f;

            // 3. 极速/自适应底噪基线跟踪器 (追踪当前环境本底静音电平，消除办公室底噪偏置)
            if (ac_rms < adaptive_noise_floor) {
                adaptive_noise_floor = adaptive_noise_floor * 0.95f + ac_rms * 0.05f; // 快速下探
            } else {
                adaptive_noise_floor = adaptive_noise_floor * 0.998f + ac_rms * 0.002f; // 极慢上升
            }
            if (adaptive_noise_floor < 0.005f) adaptive_noise_floor = 0.005f;
            if (adaptive_noise_floor > 0.080f) adaptive_noise_floor = 0.080f;

            // 4. 计算纯净有效音频信号增量 (Effective Signal Above Floor)
            float net_rms = std::max(0.0f, ac_rms - adaptive_noise_floor * 0.92f);

            // 5. 精密真实物理分贝映射 (校准后: 安静办公室 31~36dB, 说话 48~60dB, 音乐 62~78dB, 吹气 85~95dB)
            float raw_db = 32.0f;
            if (p2p_ratio > 0.35f || net_rms > 0.12f) {
                // 吹气气流或猛烈拍手/大喊冲击 (80 ~ 95dB)
                float blast = std::min(1.0f, (p2p_ratio - 0.35f) / 0.55f + (net_rms / 0.25f));
                raw_db = 80.0f + blast * 15.0f;
            } else if (net_rms > 0.003f) {
                // 正常说话与音乐播放 (40 ~ 78dB 连续对数响应)
                float log_val = 20.0f * std::log10(net_rms / 0.003f); // 每倍增 6dB
                raw_db = 38.0f + std::min(40.0f, log_val * 1.35f);
            } else {
                // 环境底噪轻微自然浮动 (31 ~ 35dB)
                raw_db = 31.0f + (ac_rms / adaptive_noise_floor) * 4.0f;
            }
            raw_db = std::max(30.0f, std::min(96.0f, raw_db));

            // 6. 快攻慢释 (Attack-Release) 包络滤波
            if (raw_db > smoothed_mic_db) {
                smoothed_mic_db = smoothed_mic_db * 0.40f + raw_db * 0.60f; // 快速感知音乐节拍与吹气
            } else {
                smoothed_mic_db = smoothed_mic_db * 0.75f + raw_db * 0.25f; // 平稳迅速回落
            }

            // 7. 5 频段 Goertzel DFT 精密音频频谱分析 (Music Visualizer 5-Band Spectrum EQ)
            // 采样率 8000Hz, N=128 点, 分辨率 62.5Hz
            // Band 0 (Sub-Bass 125Hz, k=2): 鼓点与超低音 (对应 Node 0 头部)
            // Band 1 (Bass 375Hz, k=6): 贝斯与低频律动 (对应 Node 1 颈部)
            // Band 2 (Mid 1125Hz, k=18): 人声与主旋律 (对应 Node 2 中躯)
            // Band 3 (Presence 2250Hz, k=36): 高音旋律与吉他 (对应 Node 3 下躯)
            // Band 4 (Treble 3375Hz, k=54): 镲片与高频打击 (对应 Node 4 尾部)
            static const float GOERTZEL_COEFF[5] = {
                1.990369f, // 2*cos(2*PI*2/128)  k=2
                1.913886f, // 2*cos(2*PI*6/128)  k=6
                1.213360f, // 2*cos(2*PI*18/128) k=18
                -0.382683f, // 2*cos(2*PI*36/128) k=36
                -1.662939f  // 2*cos(2*PI*54/128) k=54
            };

            float bands[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

            if (net_rms > 0.003f) {
                float ac_buf[128];
                for (int i = 0; i < 128; ++i) {
                    ac_buf[i] = ((float)mic_raw_buffer[i] - dc_mean) / 32768.0f;
                }

                for (int b = 0; b < 5; ++b) {
                    float coeff = GOERTZEL_COEFF[b];
                    float q1 = 0.0f, q2 = 0.0f;
                    for (int i = 0; i < 128; ++i) {
                        float q0 = coeff * q1 - q2 + ac_buf[i];
                        q2 = q1;
                        q1 = q0;
                    }
                    float p_sq = q1 * q1 + q2 * q2 - q1 * q2 * coeff;
                    float mag = std::sqrt(std::max(0.0f, p_sq)) / 128.0f;

                    // 均衡频段动态增益
                    static const float BAND_GAIN[5] = {28.0f, 24.0f, 20.0f, 18.0f, 16.0f};
                    float val = std::min(1.0f, mag * BAND_GAIN[b]);
                    val *= (0.35f + (net_rms / (net_rms + 0.025f)) * 0.65f);
                    bands[b] = std::min(1.0f, val);
                }
            }

            if (p2p_ratio > 0.35f) {
                bands[4] = std::min(1.0f, bands[4] + (p2p_ratio - 0.35f) * 2.0f);
            }

            physiology.feedSpectrumBands(bands[0], bands[1], bands[2], bands[3], bands[4]);
            physiology.feedMicDecibels(smoothed_mic_db);

            // 8. 实时将低频重音与声能注入节拍周期追踪器 (Beat Tracker)
            rhythm.feedAudioBeat(bands[0] * 0.70f + bands[1] * 0.30f, bands[2], smoothed_mic_db, millis());
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

    // 绑定 Web 后台动作拟态测试回调 (自动调谐生理数值并自然过渡)
    portal.setOnDemoAction([](const String &action) {
        ai.requestDemoAction(action, skeleton, tentacles, physiology, expression);
    });

    prev_micros = micros();
    Serial.println("\n>>> [Venom Symbiote] Fluid Symbol Field Fusion Ready! <<<");
    Serial.println(">>> Commands: 'demo <peek|bounce|ball_play|swing|roll|catch_dust|crawl|creep|sleep>', 'screenshot', 'hud' <<<");
}

void loop() {
    M5.update();
    haptics.update();

    // 0. 配网热点与动作测试后台处理
    if (portal.isRunning()) {
        portal.update();
        if (portal.isBlockingScreen()) {
            // 在全屏配网引导界面下，单击 BtnB 可直接切入毒液屏幕实时观察演示 (防长按释放误触)
            if (M5.BtnB.wasClicked() && !btn_b_hold_handled) {
                portal.setBlockingScreen(false);
            }
            if (M5.BtnA.wasPressed()) {
                haptics.trigger(HAPTIC_TICK);
                portal.stop();
            }
            return;
        }
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

    // 1.1 甩动抛体与晕眩累积系统 (Dynamic Accel Shake Sling Physics & Dizzy Accumulation)
    static float dizzy_shake_meter = 0.0f;
    static unsigned long last_dizzy_trigger_ms = 0;
    static unsigned long last_sling_time_ms = 0;

    float acc_mag = std::sqrt(raw_ax * raw_ax + raw_ay * raw_ay + raw_az * raw_az);
    float dynamic_g = std::max(0.0f, acc_mag - 1.0f);

    // 晃动能量自然衰减
    dizzy_shake_meter = std::max(0.0f, dizzy_shake_meter - dt * 0.85f);
    if (dynamic_g > 0.30f) {
        dizzy_shake_meter += dt * (2.0f + dynamic_g * 3.8f);
    }

    // 甩飞物理 (支持用力甩动手腕将毒液从屏幕一边啪嗒甩到另一边，彻底挣脱抓力)
    if (ai.getState() != STATE_SWING && ai.getState() != STATE_BAT_HANG) {
        if (dynamic_g > 0.35f && (millis() - last_sling_time_ms > 280)) {
            last_sling_time_ms = millis();
            dizzy_shake_meter += 1.35f; // 每次猛烈甩飞累加晕眩值

            float hx, hy;
            skeleton.getHeadPos(hx, hy);

            // 真实物理惯性抛射方向：
            // 甩动手腕急停刹车瞬间，加速度计产生反向制动加速度：
            // 1. 向下甩动急停时 raw_ay < 0 -> 惯性向前冲到底边 (dir_y = +1.0)
            // 2. 向上甩动急停时 raw_ay > 0 -> 惯性向前冲到顶边 (dir_y = -1.0)
            // 3. 向右甩动急停时 raw_ax > 0 -> 惯性向前冲到右壁 (dir_x = +1.0)
            // 4. 向左甩动急停时 raw_ax < 0 -> 惯性向前冲到左壁 (dir_x = -1.0)
            float dir_x = 0.0f;
            float dir_y = 0.0f;

            if (std::abs(raw_ax) > std::abs(raw_ay)) {
                // 水平甩动为主
                if (std::abs(raw_ax) > 0.18f) {
                    dir_x = (raw_ax > 0) ? 1.0f : -1.0f;
                } else {
                    dir_x = (hx < (float)SCREEN_W * 0.5f) ? 1.0f : -1.0f;
                }
                dir_y = 0.0f;
            } else {
                // 垂直甩动为主
                if (std::abs(raw_ay) > 0.18f) {
                    dir_y = (raw_ay < 0) ? 1.0f : -1.0f;
                } else {
                    dir_y = (hy < (float)SCREEN_H * 0.5f) ? 1.0f : -1.0f;
                }
                dir_x = 0.0f;
            }

            // 归一化
            float dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
            if (dir_len > 0.01f) {
                dir_x /= dir_len;
                dir_y /= dir_len;
            }

            // 初速度 180.0 ~ 250.0 px/s (极速破空横跨屏幕直冲对向边界！)
            float throw_speed = 180.0f + dynamic_g * 50.0f;
            if (throw_speed > 250.0f) throw_speed = 250.0f;

            skeleton.triggerSlingThrow(dir_x, dir_y, throw_speed);
            tentacles.setCreepMode(false); // 彻底收回爪子
            tentacles.reset();             // 打断爪盘与触手
            predator.cancelHunt(&skeleton, &prey_bugs); // 打断捕食并释放活虫，防残留死锁
            ai.triggerStartle(1.5f);       // 受到惊吓，紧缩硬化飞行
            haptics.trigger(HAPTIC_SLING); // 甩飞轻盈离手感
        }
    }

    // 触发头晕吐蚊香符号判定 (摇晃累积达到 3.5 且冷却间隔 > 4.5s)
    if (dizzy_shake_meter >= 3.5f && (millis() - last_dizzy_trigger_ms > 4500)) {
        last_dizzy_trigger_ms = millis();
        dizzy_shake_meter = 0.0f;

        float hx, hy;
        skeleton.getHeadPos(hx, hy);
        float sym_x = std::max(25.0f, std::min((float)SCREEN_W - 35.0f, hx));
        float sym_y = std::max(28.0f, std::min((float)SCREEN_H - 25.0f, hy - 18.0f));

        fluid_symbols.trigger("dizzy", sym_x, sym_y, hx, hy);
        skeleton.triggerLocalBleb(0, 1.6f); // 吐出蚊香圈时头部流体喷吐动效
        ai.triggerStartle(1.2f);
        haptics.trigger(HAPTIC_TICK);
        Serial.println(">>> [SHAKE] Device shaken excessively! Spat out DIZZY (mosquito-coil) symbol! <<<");
    }

    // 1.2 高频瞬态敲击检测器 (High-Pass Jerk & Multi-Tap Detection)
    static float prev_raw_ax = 0.0f, prev_raw_ay = 0.0f, prev_raw_az = 0.0f;
    static unsigned long last_tap_pulse_ms = 0;
    static int tap_counter = 0;

    float d_ax = raw_ax - prev_raw_ax;
    float d_ay = raw_ay - prev_raw_ay;
    float d_az = raw_az - prev_raw_az;
    prev_raw_ax = raw_ax;
    prev_raw_ay = raw_ay;
    prev_raw_az = raw_az;

    float jerk_mag = std::sqrt(d_ax * d_ax + d_ay * d_ay + d_az * d_az);

    // 调试辅助：当检测到任何微小加加速度突变时打印
    if (jerk_mag > 0.10f) {
        Serial.printf("[IMU-PULSE] jerk=%.3f (dx=%.2f dy=%.2f dz=%.2f)\n", jerk_mag, d_ax, d_ay, d_az);
    }

    // 敲击脉冲判定 (超灵敏门限 jerk > 0.13g 且距离上次脉冲 > 60ms 去抖)
    if (jerk_mag > 0.13f && (millis() - last_tap_pulse_ms > 60)) {
        unsigned long now_ms = millis();
        if (now_ms - last_tap_pulse_ms > 1000) {
            tap_counter = 1;
        } else {
            tap_counter++;
        }
        last_tap_pulse_ms = now_ms;

        Serial.printf("[TAP-PULSE] Triggered! jerk=%.3f -> count=%d\n", jerk_mag, tap_counter);

        // 触觉轻微敲击反响
        haptics.trigger(HAPTIC_TICK);

        if (tap_counter >= 3) {
            // 【3. 连续敲击激惹 (Multi-Tap Irritate)】
            Serial.println("[TAP-ACTION] Multi-Tap Irritate Triggered (Anger & Cyan Glow)!");
            ai.handleMultiTapIrritate(fluid_symbols, expression, physiology, skeleton);
            tap_counter = 0;
        }
    }

    // 敲击窗口结算 (处理单击与双击，520ms 舒适手感窗口)
    if (tap_counter > 0 && (millis() - last_tap_pulse_ms > 520)) {
        if (tap_counter == 2) {
            // 【1. 双击 (Double Tap)】：主动荡秋千(延长至22s) / 喷出爱心 / 问号
            Serial.println("[TAP-ACTION] Double Tap Triggered (Swing 22s / Heart / ?)");
            ai.handleDoubleTap(fluid_symbols, expression, physiology, skeleton, tentacles);
        } else if (tap_counter == 1) {
            // 【2. 单击 (Single Tap)】：睡眠时半睁眼微眯观察 / 醒着时好奇注视
            Serial.printf("[TAP-ACTION] Single Tap Triggered (sleeping=%d)\n", ai.isSleeping() ? 1 : 0);
            ai.handleSingleTap(fluid_symbols, expression, physiology);
        }
        tap_counter = 0;
    }

    // 2. 音频分析与节拍检测
    processAudioBands();
    float total_g_shake = std::max(0.0f, std::abs(raw_ax) + std::abs(raw_ay) + std::abs(raw_az) - 1.0f);
    rhythm.update(dt, total_g_shake, physiology.getAudioHigh());

    // 3. 按键与交互
    bool btn_a_pressed = M5.BtnA.wasPressed();

    if (M5.BtnB.isPressed()) {
        // 长按 BtnB (1000ms) 触发开启 HTTP Web 配网热点 (单次闭锁触发，避免连续重入)
        if (!portal.isRunning() && !btn_b_hold_handled && M5.BtnB.pressedFor(1000)) {
            btn_b_hold_handled = true;
            haptics.trigger(HAPTIC_LONG_PULSE); // 温和长震提示
            portal.start(renderer.getCanvas());
            return;
        }
    } else {
        // 释放后检查短按单击
        if (M5.BtnB.wasClicked() && !btn_b_hold_handled) {
            if (portal.isRunning()) {
                portal.setBlockingScreen(!portal.isBlockingScreen());
            } else {
                renderer.toggleHUD();
            }
            haptics.trigger(HAPTIC_TICK); // 清脆微触感
        }
        btn_b_hold_handled = false;
    }

    if (btn_a_pressed) {
        if (portal.isRunning()) {
            haptics.trigger(HAPTIC_TICK);
            portal.stop();
        } else {
            ai.triggerJolt(skeleton, metaballs, 1.2f);
            haptics.trigger(HAPTIC_JOLT_DOUBLE); // 受惊双连微颤
        }
    }

    // 4. 串口交互指令
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.startsWith("demo ") || cmd.startsWith("d ")) {
            String act = cmd.substring(cmd.indexOf(' ') + 1);
            act.trim();
            ai.requestDemoAction(act, skeleton, tentacles, physiology, expression);
        } else if (cmd.equalsIgnoreCase("peek")) {
            ai.requestDemoAction("peek", skeleton, tentacles, physiology, expression);
        } else if (cmd.equalsIgnoreCase("screenshot") || cmd.equalsIgnoreCase("s")) {
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
        } else if (cmd.equalsIgnoreCase("bug") || cmd.equalsIgnoreCase("spawn")) {
            prey_bugs.spawnBugImmediate();
            Serial.printf(">>> [BUG] Spawned new bug! Active bugs count: %d\n", prey_bugs.getActiveBugCount());
        } else if (cmd.equalsIgnoreCase("bounce")) {
            float hx, hy; skeleton.getHeadPos(hx, hy);
            ai.triggerActionState(STATE_BOUNCE, &tentacles, &skeleton, hx, hy);
            Serial.println(">>> [ACTION] Trampoline Bounce triggered!");
        } else if (cmd.equalsIgnoreCase("roll")) {
            float hx, hy; skeleton.getHeadPos(hx, hy);
            ai.triggerActionState(STATE_ROLL, &tentacles, &skeleton, hx, hy);
            Serial.println(">>> [ACTION] Sonic Roll triggered!");
        } else if (cmd.equalsIgnoreCase("dust") || cmd.equalsIgnoreCase("catch")) {
            float hx, hy; skeleton.getHeadPos(hx, hy);
            ai.triggerActionState(STATE_CATCH_DUST, &tentacles, &skeleton, hx, hy);
            Serial.println(">>> [ACTION] Glowing Dust Pounce triggered!");
        } else if (cmd.equalsIgnoreCase("ball")) {
            float hx, hy; skeleton.getHeadPos(hx, hy);
            ai.triggerActionState(STATE_BALL_PLAY, &tentacles, &skeleton, hx, hy);
            Serial.println(">>> [ACTION] Ball Juggling triggered!");
        } else if (cmd.equalsIgnoreCase("swing")) {
            float hx, hy; skeleton.getHeadPos(hx, hy);
            ai.triggerActionState(STATE_SWING, &tentacles, &skeleton, hx, hy);
            Serial.println(">>> [ACTION] Ceiling Swing triggered!");
        } else if (cmd.equalsIgnoreCase("theme")) {
            renderer.nextTheme();
        }
    }

    // 5. LLM 意识系统超低频异步请求与意图更新 (清醒 7 分钟 / 睡眠 12 分钟超长间隔，彻底避免触发 API 控频)
    bool is_sleeping = ai.isSleeping();
    unsigned long llm_interval = is_sleeping ? 720000 : 420000; // 睡时 12 分钟，清醒时 7 分钟
    if (millis() - last_llm_request_ms >= llm_interval) {
        last_llm_request_ms = millis();
        const char *stimulus = is_sleeping ? "klyntar_hivemind_communion" : ((total_g_shake > 0.4f) ? "shake" : "calm");
        llm.requestConsciousnessUpdate(physiology.getEnergy(), physiology.getStress(),
                                       physiology.getCuriosity(), physiology.getComfort(),
                                       physiology.getAttachment(), ai.getStateName(),
                                       stimulus, is_sleeping);
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

    // 8. 毒液头部物理坐标与流体墨迹擦除
    float hx, hy;
    skeleton.getHeadPos(hx, hy);
    fluid_symbols.wipePoints(hx, hy, 22.0f);

    // 7.1 音乐规律节拍感知与流体水墨音符喷射 (♪ / ♫)
    if (rhythm.checkAndConsumeMusicNoteEvent() && !fluid_symbols.hasActiveSymbol() && !is_sleeping) {
        float sym_x = (hx < SCREEN_W * 0.5f) ? (hx + 30.0f + (rand() % 20)) : (hx - 30.0f - (rand() % 20));
        sym_x = std::max(25.0f, std::min((float)SCREEN_W - 25.0f, sym_x));
        float sym_y = (hy > 60.0f) ? (hy - 32.0f) : (hy + 28.0f);
        sym_y = std::max(22.0f, std::min((float)SCREEN_H - 22.0f, sym_y));

        const char *note_type = (rand() % 2 == 0) ? "music" : "music_double";
        fluid_symbols.trigger(note_type, sym_x, sym_y, hx, hy);
        skeleton.triggerLocalBleb(0, 1.4f); // 吐出音符时头部微喷动效
        haptics.trigger(HAPTIC_TICK);
    }

    // 8.1 活体小虫子生态与捕食进食系统更新 (睡觉时绝对停止捕食)
    prey_bugs.update(dt, hx, hy);
    predator.update(dt, prey_bugs, skeleton, physiology, metaballs, is_sleeping);

    // 9. AI 行为状态机更新 (注入猎物注视感知与流体墨迹符号系统)
    ai.updateSensors(raw_ax, raw_ay, raw_az, physiology, btn_a_pressed, &skeleton, &tentacles);
    ai.update(dt, skeleton, metaballs, tentacles, physiology, relationship, expression, v3_state, &prey_bugs, &fluid_symbols);

    // 10. 骨架动力学更新 (注入低频重音脉动、节拍鼓包与音乐听歌点头律动)
    float crawl_bx, crawl_by;
    ai.getCrawlBias(crawl_bx, crawl_by);
    float spec_bands[5];
    for (int b = 0; b < 5; ++b) spec_bands[b] = physiology.getSmoothedSpectrumBand(b);

    float dynamic_resp = ai.getRespiration();
    if (rhythm.isMusicPlaying() && !is_sleeping) {
        // 随音乐节拍相位产生自然弹性的听歌点头律动 (Head-bobbing Groove)
        dynamic_resp += std::sin(rhythm.getBeatPhase() * 6.28318f) * 0.12f;
    }

    skeleton.update(dt, gx, gy, crawl_bx, crawl_by,
                    physiology.getNeuroTension(), physiology.getSpikeIntensity(),
                    dynamic_resp, is_upside_down,
                    spec_bands);

    // 10.1 撞击“啪嗒”事件检测与触觉/飞溅联动 (Sticky Splat Feedback)
    float imp_spd, hit_x, hit_y;
    if (skeleton.checkAndConsumeImpactEvent(imp_spd, hit_x, hit_y)) {
        if (ai.getState() != STATE_SWING && ai.getState() != STATE_BAT_HANG) {
            // “啪嗒”拍在玻璃上的黏性软泥微震反馈
            haptics.trigger(HAPTIC_SPLAT);
            // 瞬间向外爆射 6 根应力尖刺
            metaballs.triggerSpikeBurst(6, 1.35f);
            // 飞溅 3 颗微小黏液滴
            for (int k = 0; k < 3; ++k) {
                float sp_vx = ((rand() % 80) - 40) * 0.08f;
                float sp_vy = ((rand() % 80) - 40) * 0.08f;
                metaballs.spawnDroplet(hit_x + sp_vx * 2.0f, hit_y + sp_vy * 2.0f, sp_vx, sp_vy, 2.5f, true);
            }
        }
    }

    // 11. Voronoi 细胞与标量场（含符号粒子融合）更新
    float look_x, look_y;
    ai.getLookTarget(look_x, look_y);
    bool is_stealth = (ai.getState() == STATE_PEEK || ai.getState() == STATE_CREEP);
    voronoi.update(dt, skeleton, physiology, look_x, look_y, is_sleeping, is_stealth);
    metaballs.update(dt, skeleton, gx, gy, physiology);
    float ball_x = -1.0f, ball_y = -1.0f, ball_r = 0.0f;
    if (ai.hasActiveBall()) {
        ai.getBallPos(ball_x, ball_y, ball_r);
    }
    metaballs.computeField(skeleton, physiology, fluid_symbols, gx, gy, ball_x, ball_y, ball_r);

    // 12. 触手与眼睛系统更新
    tentacles.update(dt, skeleton, physiology, is_upside_down, is_sleeping);
    eye.update(dt, skeleton, physiology, look_x, look_y, ai.isSleeping(), ai.isSleepPeeking());

    // 13. 渲染输出 (包含小虫子与捕食进食视觉)
    renderer.render(skeleton, metaballs, eye, tentacles, ai, physiology,
                    voronoi, fluid_symbols, relationship, expression, v3_state, current_fps,
                    &prey_bugs, &predator);
    canvas.pushSprite(0, 0);

    delay(2);
}
