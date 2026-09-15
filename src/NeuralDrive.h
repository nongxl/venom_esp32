#pragma once
#include <Arduino.h>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────
//  感知特征层 (Perception Signals)
//  归一化 [0.0, 1.0] 的环境与机体感知特征
// ─────────────────────────────────────────────────────────────
struct PerceptionSignals {
    float sudden_motion      = 0.0f; // 剧烈震动/突发加速度变化 (Jerk / Violent Shake)
    float loud_sound         = 0.0f; // 突发高分贝巨响 / 爆鸣音
    float touch_contact      = 0.0f; // 敲击脉冲 / 按键轻触 / 触摸互动
    float user_nearby        = 0.0f; // 持续温和人声 / 轻度晃动交互
    float prey_detected      = 0.0f; // 视野内活跃猎物小虫刺激
    float stable_surface     = 1.0f; // 贴附/抓地稳定性 (失去附着时为 0)
    float movement_activity  = 0.0f; // 机体当前运动剧烈程度
};

// ─────────────────────────────────────────────────────────────
//  八维连续神经驱动 (Drive State)
// ─────────────────────────────────────────────────────────────
struct DriveState {
    // 快速响应 (秒级/亚秒级动态)
    float threat      = 0.05f; // 环境威胁/受惊驱动 [0.0, 1.0]
    float arousal     = 0.20f; // 整体神经激活/兴奋水平 [0.0, 1.0]

    // 中速演化 (数秒到数十秒)
    float curiosity   = 0.50f; // 好奇心与探知欲 [0.0, 1.0]
    float comfort     = 0.75f; // 舒适度与身体稳定感 [0.0, 1.0]
    float energy      = 0.85f; // 体力/活动能量储量 [0.0, 1.0]
    float fatigue     = 0.10f; // 运动疲劳累积值 [0.0, 1.0]

    // 慢速演化 (分钟级长期互动)
    float attachment  = 0.25f; // 对用户的亲近与信任依恋 [0.0, 1.0]
    float habituation = 0.00f; // 对重复刺激的习惯化钝感 [0.0, 1.0]

    void reset() {
        threat      = 0.05f;
        arousal     = 0.20f;
        curiosity   = 0.50f;
        comfort     = 0.75f;
        energy      = 0.85f;
        fatigue     = 0.10f;
        attachment  = 0.25f;
        habituation = 0.00f;
    }
};

// ─────────────────────────────────────────────────────────────
//  八类生物动机行为 (Behavior Types)
// ─────────────────────────────────────────────────────────────
enum NeuralBehaviorType {
    BEH_ESCAPE = 0,  // 应激逃窜 / 闪避
    BEH_FREEZE,      // 警惕定身 / 屏息僵直
    BEH_OBSERVE,     // 好奇注视 / 探头张望
    BEH_EXPLORE,     // 巡游漫步 / 大步攀爬
    BEH_APPROACH,    // 亲近依恋 / 靠近互动
    BEH_PROBE,       // 试探捕抓 / 拨弄尘埃
    BEH_REST,        // 静养憩息 / 深度睡眠 / 倒挂小憩
    BEH_ATTACH,      // 共生玩耍 / 蛛丝悬挂 / 弹跳颠球
    BEH_COUNT
};

const char* getNeuralBehaviorName(NeuralBehaviorType beh);

// ─────────────────────────────────────────────────────────────
//  行为打分与竞争结果
// ─────────────────────────────────────────────────────────────
struct BehaviorScores {
    float raw_score[BEH_COUNT]   = {0};
    float final_score[BEH_COUNT] = {0};

    NeuralBehaviorType winner     = BEH_OBSERVE;
    float winner_score            = 0.0f;

    NeuralBehaviorType runner_up  = BEH_EXPLORE;
    float runner_up_score         = 0.0f;
};

// ─────────────────────────────────────────────────────────────
//  短期事件类型与记录结构 (Short-Term Event Memory)
// ─────────────────────────────────────────────────────────────
enum NeuralEventType {
    EVT_NONE = 0,
    EVT_TOUCH,           // 轻敲 / 触摸脉冲
    EVT_SHAKE,           // 剧烈晃动
    EVT_LOUD_SOUND,      // 突发巨响 / 爆鸣音
    EVT_USER_APPROACH,   // 温和人声 / 用户轻晃互动
    EVT_ESCAPE,          // 发生应激避险逃逸
    EVT_PROBE_SUCCESS,   // 捕抓成功 (如捕获浮尘 / 猎物)
    EVT_PROBE_FAIL,      // 扑空试探
    EVT_REST_COMPLETE    // 憩息回能完成
};

const char* getNeuralEventName(NeuralEventType evt);

struct RecentEvent {
    NeuralEventType type = EVT_NONE;
    float timestamp      = 0.0f; // 相对时间 (秒)
    float intensity      = 0.0f; // 强度 [0.0, 3.0]
};

// ─────────────────────────────────────────────────────────────
//  物理运动意图向量 (Motor Intent Vector)
//  将神经行为转化为连续的物理参数驱动 12-Node 身体
// ─────────────────────────────────────────────────────────────
struct MotorIntent {
    // 物理冲量与方向
    float impulse_x            = 0.0f; // 瞬时冲量 X
    float impulse_y            = 0.0f; // 瞬时冲量 Y
    float impulse_strength     = 0.0f; // 冲量大小 [0.0, 3.0]

    // 形态张力与形变
    float elongation           = 1.0f; // 沿运动朝向的拉伸比 (1.0=基准, 1.35=拉长探出, 0.75=压缩压扁)
    float squash               = 1.0f; // 横向压缩比
    float instability          = 0.0f; // 机体物理失稳度/震颤抖动幅度 [0.0, 1.0]
    float spring_stiffness_mod = 1.0f; // 肌肉骨架刚度倍率 (受惊紧绷 1.65f, 沉睡松弛 0.62f)

    // 接触与附着力调控
    float movement_speed       = 0.0f; // 主动巡游/爬行期望速率 [0.0, 2.0]
    float wall_adhesion_mod    = 1.0f; // 壁面附着力倍率 (受惊脱附 0.32f, 稳定抓地 1.40f)
    float head_lead_bias       = 1.0f; // 头部节点牵引主导性 [0.5, 1.8]

    // 视觉/注意力协同
    float eye_attention        = 0.5f; // 眼神聚焦敏锐度 [0.0, 1.0]
};

// ─────────────────────────────────────────────────────────────
//  神经网络行为核心 (Neural Behavior Core)
// ─────────────────────────────────────────────────────────────
class NeuralDrive {
public:
    NeuralDrive();

    void init();
    void update(float dt, const PerceptionSignals &perc);

    // 感知信号直接激发接口 (供瞬态事件调用)
    void triggerSuddenMotion(float intensity = 1.0f);
    void triggerLoudSound(float intensity = 1.0f);
    void triggerTouchContact(float intensity = 1.0f);
    void triggerPositiveInteraction(float amount = 0.05f);

    // 试探捕抓结果反馈
    void triggerProbeOutcome(bool success, float reward = 1.0f);

    // 短期事件记录与查询
    void recordEvent(NeuralEventType type, float intensity = 1.0f);
    int countRecentEvents(NeuralEventType type, float window_sec = 4.0f) const;
    const RecentEvent& getLatestEvent() const;
    const char* getLatestEventName() const;

    // 习惯化阻尼系数查询 [0.25, 1.0]
    float getHabituationDampingFactor() const {
        return std::max(0.25f, 1.0f - drives.habituation * 0.75f);
    }

    // 运动意图外加冲量接口
    void applyMotorImpulse(float ix, float iy, float strength);

    // 状态与打分查询
    const DriveState& getDrives() const { return drives; }
    DriveState& getMutableDrives() { return drives; }
    const PerceptionSignals& getPerception() const { return perception; }
    const BehaviorScores& getScores() const { return scores; }
    const MotorIntent& getMotorIntent() const { return motor_intent; }

    NeuralBehaviorType getCurrentBehavior() const { return current_behavior; }
    NeuralBehaviorType getWinnerBehavior() const { return scores.winner; }
    float getBehaviorDuration() const { return current_behavior_duration; }
    const char* getCurrentBehaviorName() const { return getNeuralBehaviorName(current_behavior); }

    // 格式化输出 (用于串口与 HUD 调试)
    void formatDriveBar(char *buf, size_t buf_size, const char *label, float val, int bar_len = 8) const;
    void printDebugSummary() const;

private:
    DriveState drives;
    PerceptionSignals perception;
    BehaviorScores scores;
    MotorIntent motor_intent;

    // 短期事件环形历史缓冲区
    static const int EVENT_HISTORY_CAP = 8;
    RecentEvent event_history[EVENT_HISTORY_CAP];
    int event_history_head = 0;
    int event_count_total = 0;
    float global_time_sec = 0.0f;

    // 行为惯性与迟滞控制 (Behavior Commitment & Hysteresis)
    NeuralBehaviorType current_behavior = BEH_OBSERVE;
    float current_behavior_duration = 0.0f;
    float current_behavior_min_duration = 2.0f;

    // 习惯化内部辅助参数
    float tap_repetition_timer = 0.0f;
    int tap_streak_count = 0;

    void updateDriveDynamics(float dt, const PerceptionSignals &perc);
    void applyBehaviorFeedback(float dt);
    void evaluateBehaviorCompetition(float dt);
    void computeMotorIntent(float dt);
    float getMinDurationForBehavior(NeuralBehaviorType beh) const;
};
