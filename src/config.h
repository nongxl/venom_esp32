#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// 1. 屏幕与渲染分辨率配置
// ============================================================================
static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 135;

// Metaball 场分辨率 (Scale 2 表示场大小为 120x68，渲染时放大 2 倍)
// 调小 FIELD_SCALE 会增加计算量但让边缘更平滑
static constexpr int FIELD_W = 120;
static constexpr int FIELD_H = 68;
static constexpr int FIELD_SCALE = 2; 

// ============================================================================
// 2. 3D 逻辑容器尺寸与面定义
// ============================================================================
static constexpr float CUBE_W = 100.0f; // 逻辑宽度 (相对于中心)
static constexpr float CUBE_H = 60.0f;  // 逻辑高度
static constexpr float CUBE_D = 10.0f;  // 逻辑深度 (决定 3D 透视感)

enum Face {
    FRONT, BACK, LEFT, RIGHT, TOP, BOTTOM
};

// ============================================================================
// 3. 物理系统核心参数 (Skeleton / Physics)
// ============================================================================
static constexpr int   MAX_NODES         = 8;     // 身体节点总数: 8个是最佳平衡点
static constexpr float SPRING_STIFFNESS  = 0.72f; // [下调] 增加柔软度以吸收抖动
static constexpr float SPRING_DAMPING    = 0.95f; // [下调] 增加阻尼，让位移更稳定
static constexpr float NODE_RADIUS_BASE  = 26.0f; // [下调] 减小物理半径，使整体更细长
static constexpr float SPRING_STIFFNESS_DECAY = 0.96f; // 刚度随节点索引的衰减率

static constexpr float GRAVITY_MAGNITUDE = 2.5f;  // 重力强度: 决定自然下坠的速度
static constexpr float ANTIGRAVITY_STRENGTH = 0.90f; // 抗重力系数: 1.0 完全无视重力，0.0 完全下坠
static constexpr uint32_t FALLING_STRUGGLE_MS = 1200; // 挣扎时间: 抓取失效到完全坠落的时长
static constexpr float PULL_FORCE_MAGNITUDE  = 85.0f; // [大幅提升] 赋予头部更强的引擎拉力
static constexpr float LEAP_IMPULSE_BASE     = 180.0f; // 起跳基础冲量：值越大跳得越快
static constexpr float LEAP_SPREAD_BASE      = 0.8f;  // 跳跃时的溅射率：越大溅得越散
static constexpr float TAIL_DRAG_FACTOR      = 0.25f; // 尾部重量感：越高尾巴越沉，摆动感越强
static constexpr float STICK_DISTANCE        = 1.2f;  // 粘墙感应距离：节点离墙多近会触发吸附
static constexpr float ADHESION_THRESHOLD    = 0.85f; // [大幅提升] 抵消 85% 的重力漂移，消除“摆钟感”
static constexpr float BASE_SUCTION_FORCE    = 1.20f; // [翻倍] 远超重力的吸附力
static constexpr float UNSTICK_THRESHOLD     = 1.85f; // [关键] 提高剥离门槛，模拟胶带撕开的阻力
static constexpr float PEELING_FORCE_DECAY   = 0.15f; // [微调] 剥离过程中的阻尼
static constexpr float IMU_DETACH_FORCE      = 14.5f; // [新增] 撞击脱离阈值
static constexpr int   IMPACT_GRACE_FRAMES   = 12;    // 略微缩短保护期
static constexpr float PEELING_SOFTEN_RATIO  = 0.18f; // [下调] 增强剥离效应，使下坠更顺滑
static constexpr float INTERNAL_VISCOSITY_HI = 0.65f; // [新增] 极软态下的内部阻尼
static constexpr float INTERNAL_VISCOSITY_LO = 0.35f; // [新增] 常态下的内部阻尼
static constexpr float AIR_DRAG_COEFF        = 0.92f; // [新增] 自由落体时的空气粘滞系数

// ============================================================================
// 4. 视觉外观调优 (Rendering / Aesthetics)
// ============================================================================
// A. 体型比例 (Metaball 生成参数)
static constexpr float VISUAL_RADIUS_MULT        = 0.85f; // 半径缩放：整体胖瘦
static constexpr float VISUAL_RADIUS_OFFSET      = 1.4f;  // 半径偏移：基础厚度
static constexpr float VISUAL_BODY_SCALE_IDLE    = 1.2f;  // 闲置时的放大率：看起来更圆润
static constexpr float VISUAL_BODY_SCALE_STARTLED= 1.2f;  // 暴走时的放大率：显著增大体现威胁感

// B. 动态拉伸与拖影 (运动模糊效果)
static constexpr float VISUAL_STRETCH_NORMAL     = 4.5f;  // 普通拉伸强度
static constexpr float VISUAL_STRETCH_STARTLED   = 10.0f; // 暴走拉伸强度：数值越大残影越长

// C. 表面噪波与纹理 (特效)
static constexpr float NOISE_BASE_FREQ     = 0.20f; // 表面起伏频率：起伏的快慢
static constexpr float NOISE_AGITATION_MAX = 16.0f;  // 最大躁动度：表面“沸腾”的剧烈程度
static constexpr float NOISE_SPIKE_SCALE   = 100.0f; // 尖刺长度：暴走时“炸毛”的长度
static constexpr float BREATH_SPEED_BASE     = 0.02f; // 呼吸快慢
static constexpr float BREATH_RANGE_BASE     = 0.10f; // 基础呼吸幅度

// ============================================================================
// 5. 色彩方案 (RGB565 格式)
// ============================================================================
static constexpr uint16_t COLOR_BACKGROUND   = 0x3186; 
static constexpr uint16_t COLOR_VENOM_BODY   = 0x0000; 
static constexpr uint16_t COLOR_VENOM_SHADOW = 0x18C3; 
static constexpr uint16_t COLOR_VENOM_GLOSS  = 0x5AEB; 
static constexpr uint16_t COLOR_EYE_WHITE    = 0xFFFF; 
static constexpr uint16_t COLOR_PUPIL        = 0x0000; 
static constexpr uint16_t COLOR_FRAME        = 0x18C3; 

// ============================================================================
// 6. 生理、互动与 AI 逻辑 (Behavioral AI)
// ============================================================================
// 基础性格变化率
static constexpr float DRIFT_RATE_SLOW     = 0.001f; // 生理数值极其缓慢的漂移率
static constexpr float DRIFT_RATE_MED      = 0.005f; 
static constexpr float DRIFT_RATE_FAST     = 0.02f;  
static constexpr float RECOVERY_RATE       = 0.0008f; // 压力自动恢复速度：越大恢复越快

// 眼睛神态
static constexpr float EYE_W_NORMAL       = 6.0f;  
static constexpr float EYE_H_NORMAL       = 4.0f;  
static constexpr float EYE_W_FOCUS        = 8.0f; 
static constexpr float EYE_H_FOCUS        = 5.0f;  
static constexpr float EYE_W_ALERT        = 10.0f; 
static constexpr float EYE_H_ALERT        = 7.0f;  
static constexpr float PUPIL_RADIUS_BASE  = 4.2f;  

// 行为状态转换参数
static constexpr float IDLE_TENSION_BASE         = 0.25f; // 闲置张力：越小越瘫软
static constexpr float IDLE_DAMPING_BASE         = 0.45f; // 闲置阻尼：越高越不动
static constexpr float IDLE_NOISE_EDGE_BASE      = 1.4f;  // 闲置边缘活性：边缘蠕动的剧烈程度
static constexpr float STARTLED_TENSION          = 1.4f;  // 暴走张力：越大身体越紧实
static constexpr float STARTLED_SPIKE_INTENSITY  = 6.5f;  // 暴走初始尖刺度
static constexpr float STARTLED_JUMP_SPIKE       = 5.0f;  // 碰撞维持尖刺度
static constexpr float STARTLED_SPEED_MULT       = 35.0f; // 暴走时的综合速度倍率
static constexpr float STARTLED_IMPULSE_MULT     = 5.5f;  // 暴走初速度倍率
static constexpr int   STARTLED_JUMP_MIN         = 12;    // 暴走最少撞击次数
static constexpr int   STARTLED_JUMP_MAX         = 20;    // 暴走最多撞击次数
static constexpr uint32_t SKILL_REACTION_DELAY_MS = 600;   // 受惊物理剥离到暴走反应的延迟时间
static constexpr float STRESS_GAIN_SCALE         = 0.0003f; // 晃动导致的压力增长系数
static constexpr float STRESS_IMPACT_BOOST       = 0.35f;  // 撞击一次增加多少压力
static constexpr uint32_t STARTLED_COOLDOWN_MS   = 3000;   // 暴走后的冷却时间

static constexpr uint32_t SKILL_MIN_DURATION_MS = 5000;  // 技能最短持续时间
static constexpr uint32_t SKILL_MAX_DURATION_MS = 20000; // 技能最长持续时间
static constexpr uint32_t RECOVERY_MIN_MS    = 4000;   // 暴走后的强制恢复时间

// 传感器触发阈值
static constexpr float IMU_STARE_THRESHOLD    = 0.15f; // 加速度计波动小于此值判定为“盯着它看”
static constexpr float IMU_STARTLED_THRESHOLD = 7.5f;   // 吓到它的晃动阈值
static constexpr float IMU_MAX_G_THRESHOLD    = 5.0f;   // 判定“剧烈震动”的阈值
static constexpr float IMU_STRESS_THRESHOLD   = 2.2f;   // 增加压力的晃动阈值
static constexpr float SOUND_THRESHOLD_LOW    = 62.0f;   // 低分贝噪音阈值 (dB)
static constexpr float SOUND_THRESHOLD_HIGH   = 82.0f;   // 高分贝噪音阈值 (dB)
static constexpr uint32_t OBSERVE_TRIGGER_MS  = 3000;  // 盯着看多久会触发它的观察动作
static constexpr uint32_t HIDE_TRIGGER_MS     = 10000; // 静止多久会触发它的躲藏动作
static constexpr uint32_t ALERT_TRIGGER_MS    = 1500;   // 噪音持续多久会引起它的警觉

// 初始生理参数 (性格画像)
static constexpr float INIT_ENERGY     = 0.4f;  // 能量：高了爱动，低了爱睡
static constexpr float INIT_STRESS     = 0.1f;  // 压力
static constexpr float INIT_CURIOSITY  = 0.8f;  // 好奇心：高了爱观察用户
static constexpr float INIT_COMFORT    = 0.4f;  // 舒适度
static constexpr float INIT_ATTACHMENT = 0.6f;  // 亲密度
static constexpr float INIT_FATIGUE    = 0.0f;  // 疲劳度
static constexpr float INIT_VIGILANCE  = 0.4f;  // 警觉度
static constexpr float INIT_IRRITATION = 0.2f;  // 烦躁度

// 音频采样配置
static constexpr float AUDIO_VOL_MIN_DB    = 40.0f; 
static constexpr float AUDIO_VOL_MAX_DB    = 80.0f; 
static constexpr float AUDIO_HARSH_THRESHOLD = 0.6f; // 刺耳声频率判定阈值

#endif
