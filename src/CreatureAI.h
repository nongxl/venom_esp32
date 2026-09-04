#pragma once
#include <Arduino.h>
#include "config.h"
#include "PhysiologySystem.h"
#include "RelationshipSystem.h"
#include "SkeletonSystem.h"
#include "MetaballSystem.h"
#include "TentacleRenderer.h"
#include "ExpressionLayer.h"
#include "LLMClient.h"
#include "PreyBugSystem.h"
#include "FluidSymbolSystem.h"

enum CreatureState {
    STATE_IDLE = 0,
    STATE_CRAWL,        // 粗壮触手远距离大步爬行
    STATE_CREEP,        // 表皮细小触手近距离缓慢蠕动
    STATE_OBSERVE,
    STATE_SLEEP,
    STATE_STARTLED,
    STATE_HESITATING,
    STATE_JOLTING,
    STATE_EXPRESSING,
    STATE_SWING,        // 高空蛛丝悬挂荡秋千状态
    STATE_CATCH_DUST,   // 扑抓灰尘玩耍
    STATE_ROLL,         // 软体蜷缩翻滚玩耍
    STATE_BOUNCE,       // 自娱自乐原地蹦床
    STATE_BAT_HANG,     // 倒挂金钩发呆或睡觉
    STATE_BALL_PLAY,    // 自体分裂弹球颠球自娱自乐
    STATE_PEEK          // 边缘暗中观察/潜行窥视 (只露双眼和头尖，警惕探索)
};

enum BallPhase {
    BALL_PLAYING = 0,    // 正常空中飞舞颠球
    BALL_SNATCH,         // 触手破空伸出抓球 (0.28s)
    BALL_RETRACT,        // 触手抓着球强力拉回身体 (0.45s)
    BALL_FUSING          // 贴合头部肉体，缓慢液态渗透吞咽融合 (0.75s)
};

struct SymbioteBall {
    bool active = false;
    float x = 120.0f;
    float y = 60.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float radius = 7.5f;
    int bounce_count = 0;
    int max_bounces = 5;

    BallPhase phase = BALL_PLAYING;
    float phase_timer = 0.0f;
    float hit_cooldown = 0.0f; // 击球防连击/抽搐冷却

    // 触手抓取与拉回插值坐标
    float snatch_start_x = 0.0f;
    float snatch_start_y = 0.0f;
};

class CreatureAI {
public:
    CreatureAI();

    void init();
    void update(float dt, SkeletonSystem &skeleton, MetaballSystem &metaballs,
                TentacleRenderer &tentacles, PhysiologySystem &physiology,
                RelationshipSystem &relationship, ExpressionLayer &expression,
                const ConsciousnessStateV3 &v3_state,
                const PreyBugSystem *bugs = nullptr,
                FluidSymbolSystem *fluid_symbols = nullptr);

    // 状态与模式查询
    CreatureState getState() const { return current_state; }
    const char* getStateName() const;

    void getCrawlBias(float &bx, float &by) const { bx = crawl_force_x; by = crawl_force_y; }
    void getLookTarget(float &lx, float &ly) const { lx = target_look_x; ly = target_look_y; }
    float getRespiration() const { return respiration_factor; }

    bool isStartled() const { return current_state == STATE_STARTLED || current_state == STATE_JOLTING; }
    bool isSleeping() const {
        return (current_state == STATE_SLEEP) || (current_state == STATE_BAT_HANG && is_bat_hang_sleeping) || (current_state == STATE_ROLL);
    }
    bool isSleepPeeking() const { return is_sleep_peeking; }
    bool isPeeking() const { return current_state == STATE_PEEK; }

    // 动作测试模式与自然过渡接口 (网页后台及串口调用)
    void requestDemoAction(const String &action_name, SkeletonSystem &skeleton, TentacleRenderer &tentacles, PhysiologySystem &physiology, ExpressionLayer &expression);
    bool isDemoTransitioning() const { return demo_transitioning; }

    bool hasActiveBall() const { return symbiote_ball.active; }
    void getBallPos(float &bx, float &by, float &br) const {
        bx = symbiote_ball.x;
        by = symbiote_ball.y;
        br = symbiote_ball.radius;
    }

    void triggerStartle(float intensity = 1.0f, SkeletonSystem *skeleton = nullptr, TentacleRenderer *tentacles = nullptr);

    void triggerActionState(CreatureState state, TentacleRenderer *tentacles = nullptr, SkeletonSystem *skeleton = nullptr, float hx = 120.0f, float hy = 100.0f) {
        enterState(state, tentacles, skeleton, hx, hy);
    }

    // 敲击互动处理接口
    void handleSingleTap(FluidSymbolSystem &symbols, ExpressionLayer &expr, PhysiologySystem &phys);
    void handleDoubleTap(FluidSymbolSystem &symbols, ExpressionLayer &expr, PhysiologySystem &phys, SkeletonSystem &skeleton, TentacleRenderer &tentacles);
    void handleMultiTapIrritate(FluidSymbolSystem &symbols, ExpressionLayer &expr, PhysiologySystem &phys, SkeletonSystem &skeleton);
    void triggerJolt(SkeletonSystem &skeleton, MetaballSystem &metaballs, float intensity);
    void triggerReactiveCrawl(SkeletonSystem &skeleton, TentacleRenderer &tentacles);
    void triggerInteraction(SkeletonSystem *skeleton = nullptr, TentacleRenderer *tentacles = nullptr);
    void updateSensors(float imu_gx, float imu_gy, float imu_gz, const PhysiologySystem &physiology, bool btn_a_pressed, SkeletonSystem *skeleton = nullptr, TentacleRenderer *tentacles = nullptr);

private:
    CreatureState current_state = STATE_IDLE;
    CreatureState last_state = STATE_IDLE;
    CreatureState pending_state = STATE_IDLE;
    float state_timer = 0.0f;
    float state_duration = 4.0f;
    float hesitation_timer = 0.0f;
    bool is_bat_hang_sleeping = false;

    float respiration_phase = 0.0f;
    float respiration_factor = 0.0f;
    float twitch_timer = 0.0f;
    float twitch_offset = 0.0f;

    float micro_behavior_timer = 0.0f;

    float crawl_force_x = 0.0f;
    float crawl_force_y = 0.0f;
    int crawl_step_count = 0;
    float crawl_target_x = 120.0f;
    float crawl_target_y = 100.0f;
    int crawl_perimeter_edge = 0;
    float crawl_shoot_timer = 0.0f;

    float target_look_x = 120.0f;
    float target_look_y = 67.0f;

    float last_imu_gx = 0.0f;
    float last_imu_gy = 1.0f;
    float last_total_g = 1.0f;
    float sleep_zz_timer = 0.0f;
    bool is_sleep_peeking = false;
    float sleep_peek_timer = 0.0f;
    float sleep_peek_cooldown = 0.0f;
    float interaction_wake_timer = 0.0f; // 互动唤醒保鲜期，互动期间绝对禁止秒睡

    float startle_energy = 0.0f;
    SymbioteBall symbiote_ball;

    // 蹦蹦床特技状态 (Elastic Trampoline Slime Jumping)
    int bounce_jump_count = 0;
    int bounce_max_jumps = 3;
    int bounce_phase = 0; // 0: Squash, 1: Launch, 2: Apex float, 3: Land
    float bounce_timer = 0.0f;

    // 软体球形翻滚状态 (Sonic Roll)
    float roll_vx = 38.0f;
    int roll_bounces = 0;

    // 边缘暗中观察/窥视状态参数 (State Peek)
    int   peek_edge = 2;         // 0:顶, 1:右, 2:底, 3:左
    float peek_target_x = 120.0f;
    float peek_target_y = 100.0f;
    float peek_move_dir = 1.0f;
    float peek_raise_timer = 0.0f;       // 探头与缩回时间计时
    float peek_submerge_offset = 0.0f;   // 探出身躯高度偏移 (负值向上探出)
    bool  peek_is_raised = false;        // 当前是否好奇多探出头
    float peek_raise_interval = 3.0f;    // 下一次探头间隔时间

    // 动作演示与自然过渡状态
    bool demo_transitioning = false;
    float demo_transition_timer = 0.0f;
    CreatureState demo_target_state = STATE_IDLE;
    String demo_action_name = "";

    CreatureState decideNextState(CreatureState from_state, float hx, float hy, const PhysiologySystem &physiology);
    void enterState(CreatureState new_state, TentacleRenderer *tentacles = nullptr, SkeletonSystem *skeleton = nullptr, float hx = 120.0f, float hy = 100.0f);
    void enterHesitation(CreatureState target_state, float delay_sec);
    void updateOrganicBreathing(float dt, const PhysiologySystem &physiology, const ExpressionLayer &expression);
    void updateMicroBehaviors(float dt, SkeletonSystem &skeleton, const PhysiologySystem &physiology);
    void updateDemoTransition(float dt, SkeletonSystem &skeleton, TentacleRenderer &tentacles, float hx, float hy);

    void updateIdle(float dt, float hx, float hy, const PhysiologySystem &physiology, const RelationshipSystem &relationship, TentacleRenderer &tentacles, SkeletonSystem &skeleton);
    void updateCrawl(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton);
    void updateCreep(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton);
    void updateObserve(float dt, float hx, float hy, const PhysiologySystem &physiology, TentacleRenderer &tentacles, SkeletonSystem &skeleton);
    void updateSleep(float dt, float hx, float hy, const PhysiologySystem &physiology, FluidSymbolSystem *fluid_symbols = nullptr);
    void updateStartled(float dt, float hx, float hy, const PhysiologySystem &physiology);
    void updateHesitating(float dt, float hx, float hy, const ExpressionLayer &expression);
    void updateJolting(float dt, float hx, float hy, const PhysiologySystem &physiology);
    void updateExpressing(float dt, float hx, float hy, const ExpressionLayer &expression);
    void updateSwing(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, const PhysiologySystem &physiology);
    void updateCatchDust(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, ExpressionLayer &expression, PhysiologySystem &physiology, MetaballSystem *metaballs = nullptr, const PreyBugSystem *bugs = nullptr, FluidSymbolSystem *fluid_symbols = nullptr);
    void updateRoll(float dt, float hx, float hy, SkeletonSystem &skeleton, ExpressionLayer &expression, PhysiologySystem &physiology, MetaballSystem *metaballs = nullptr);
    void updateBounce(float dt, float hx, float hy, SkeletonSystem &skeleton, MetaballSystem &metaballs, PhysiologySystem &physiology, ExpressionLayer &expression);
    void updateBatHang(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, PhysiologySystem &physiology, FluidSymbolSystem *fluid_symbols = nullptr);
    void updateBallPlay(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, MetaballSystem &metaballs, ExpressionLayer &expression, PhysiologySystem &physiology, FluidSymbolSystem *fluid_symbols = nullptr);
    void updatePeek(float dt, float hx, float hy, SkeletonSystem &skeleton, TentacleRenderer &tentacles, PhysiologySystem &physiology, ExpressionLayer &expression);
};
