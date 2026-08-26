#pragma once
#include <Arduino.h>
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets_example.h"
#endif

// ─────────────────────────────────────────────────────────────
//  M5StickS3 / ESP32-S3 Venom Symbiote Configuration
// ─────────────────────────────────────────────────────────────

// 屏幕硬件分辨率（横屏标准 240x135）
static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 135;

// Metaball 低分辨率标量场配置
static constexpr int GRID_SCALE = 3;
static constexpr int GRID_W     = SCREEN_W / GRID_SCALE; // 80
static constexpr int GRID_H     = SCREEN_H / GRID_SCALE; // 45

// 骨架动力学参数
static constexpr int SKELETON_NODE_COUNT = 5;  // 0:Head, 1:UpperBody, 2:Center, 3:LowerBody, 4:Tail
static constexpr int MAX_DROPLETS        = 10; // 飞溅液滴微球最大数量
static constexpr int MAX_TENTACLES       = 4;  // 贝塞尔触手数量
static constexpr int VORONOI_SEEDS       = 12; // 动态 Voronoi 细胞核数量
static constexpr int MAX_INK_PARTICLES   = 48; // 活体液态墨水粒子数量（精简轻量）

// 物理与形变常数
static constexpr float DEFAULT_GRAVITY_Y = 0.35f;
static constexpr float SPRING_STIFFNESS  = 0.18f;
static constexpr float SPRING_DAMPING    = 0.72f;
static constexpr float WALL_STICK_FORCE  = 0.60f;
static constexpr float WALL_STICK_DIST   = 18.0f; // 像素吸附距离
static constexpr float WALL_FLATTEN_RATE = 0.55f; // 贴壁压扁形变率
static constexpr float SWING_ROPE_LENGTH = 65.0f; // 悬挂蛛丝单摆绳长 (牛顿摆物理高度)
static constexpr float SWING_PUMP_FREQ   = 2.4f;  // 牛顿单摆谐波摆动角速度 (周期约 2.6s)

// 情绪状态枚举
enum EmotionState {
    EMOTION_CALM = 0,       // 平静
    EMOTION_STRESS,         // 紧张
    EMOTION_FEAR,           // 恐惧
    EMOTION_ANGER,          // 愤怒
    EMOTION_CURIOSITY,      // 好奇
    EMOTION_EXHAUSTED       // 疲惫
};

// V3 行为意图枚举
enum V3Intent {
    INTENT_WATCH_OBSERVER = 0,
    INTENT_APPROACH_OBSERVER,
    INTENT_AVOID_OBSERVER,
    INTENT_TEST_BOUNDARY,
    INTENT_SEEK_SHADOW,
    INTENT_SEEK_SAFETY,
    INTENT_PATROL_TERRITORY,
    INTENT_HIDE_PRESENCE,
    INTENT_EXPRESS_DISTRESS,
    INTENT_IDLE
};

// 10 种非语言身体表达类型
enum ExpressionType {
    EXPR_NONE = 0,
    EXPR_OBSERVE,           // 1. 观察表达：贴近玻璃、呼吸放慢、触手静止
    EXPR_HESITATION,        // 2. 犹豫表达：前进试探 -> 停顿 -> 后退 -> 再试探
    EXPR_FEAR,              // 3. 害怕表达：紧缩硬化、静止抓边
    EXPR_TRUST,             // 4. 信任表达：摊开贴近、触手贴玻璃
    EXPR_DISCOMFORT,        // 5. 不满表达：远离躲避
    EXPR_CURIOSITY,         // 6. 好奇表达：探出眼睛、触手轻敲
    EXPR_MIMICRY,           // 7. 模仿表达：触手节拍敲击回应
    EXPR_WARNING,           // 8. 警告表达：扑向观察面覆盖后退
    EXPR_GLASS_CONTACT,     // 9. 隔玻璃接触：触手贴握持位置内侧
    EXPR_SILENT_OBSERVATION // 10. 沉默观察：长期静止注视
};

// 极简、有机、活体七肢桶符号类型（杜绝复杂单词）
enum FluidSymbolType {
    SYMBOL_NONE = 0,
    SYMBOL_RING,            // 七肢桶环形圆圈图腾 (Arrival Glyph)
    SYMBOL_QUESTION,        // 探究问号 "?" 与悬浮液滴
    SYMBOL_EXCLAMATION,     // 警觉叹号 "!"
    SYMBOL_CROSS,           // 拒绝/抗拒叉号 "X"
    SYMBOL_RIPPLE           // 同心波纹/涟漪
};

// 颜色定义 (RGB565)
enum BackgroundTheme {
    THEME_DESIGN_BLUE = 0, // design.png 经典宝蓝
    THEME_PURE_BLACK  = 1, // 纯黑
    THEME_SLATE_GRAY  = 2, // 暗灰
    THEME_NEON_PURPLE = 3, // 暗紫
    THEME_COUNT       = 4
};

static constexpr uint16_t COLOR_VENOM_BLACK   = 0x0841;
static constexpr uint16_t COLOR_VENOM_CORE    = 0x0000;
static constexpr uint16_t COLOR_DITHER_GRAY   = 0x2945;
static constexpr uint16_t COLOR_NEURO_PULSE   = 0x4228;
static constexpr uint16_t COLOR_INK_BLACK     = 0x0000; // 活体墨水深黑
static constexpr uint16_t COLOR_INK_GLOW      = 0x31E7; // 墨水湿润微反光
static constexpr uint16_t COLOR_GLOW_CYAN     = 0x9FFF;
static constexpr uint16_t COLOR_GLOW_WHITE    = 0xFFFF;
static constexpr uint16_t COLOR_EYE_WHITE     = 0xFFFF;
static constexpr uint16_t COLOR_EYE_PUPIL     = 0x0000;

// 硬件与外设设定
static constexpr float  IMU_LPF_ALPHA         = 0.88f;
static constexpr float  IMU_DEADZONE          = 0.08f;
static constexpr float  IMU_SHAKE_THRESHOLD   = 1.8f;

static constexpr uint8_t SYSTEM_VOLUME        = 100;
static constexpr uint8_t SYSTEM_BRIGHTNESS    = 180;

static constexpr int    VIBR_PIN              = 0;
static constexpr int    VIBR_PWM_CHANNEL      = 2;
static constexpr int    VIBR_PWM_FREQ         = 10000;
static constexpr int    VIBR_PWM_BITS         = 8;
