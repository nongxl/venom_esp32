#ifndef VENOM_H
#define VENOM_H

#include <M5Unified.h>
#include "physics/Skeleton.h"
#include "FluidSymbol.h"
#include <vector>
#include "config.h"

enum class VenomState { 
    IDLE, CRAWL, OBSERVE, STARTLED, HIDING, GRAPPLING, LEAP,
    RECOVERY, ALERT, CURIOUS_PROBE, CLING, GROOMING, EXPLORE, CAMOUFLAGE, TRACK_OBSERVER,
    HESITATION, WARNING_POUNCE, SILENT_WATCH, MIMICRY
};
enum class VenomCrawlState { REACHING, PULLING, STUCK };

// --- 面向渲染与外部调用的视觉状态结构 ---
struct VenomVisualState {
    float agitation_level  = 0.2f; // 烦躁度: 影响噪波速度和振幅
    float spike_intensity  = 0.0f; // 尖刺强度: 0 为光滑, 1 为海胆
    float viscosity        = 0.5f; // 粘稠度: 影响融合阈值
    float surface_detail   = 0.3f; // 表面细节频率
    float edge_activity    = 0.3f; // 边缘活性: 影响尖刺/液滴/细丝生成率
    float voronoi_scale    = 1.0f; // Voronoi 细胞尺度 (Calm→大, Stress→小)
    float surface_tension  = 0.5f; // 表面张力: 影响边缘平滑度
    float mass_fwd_bias    = 0.0f; // 质心前移偏置: 移动时前部堆积 [−1,1]
    float body_scale       = 1.0f; // 身体比例: 用于在受惊等状态下整体缩小
    uint32_t lastLeapTime  = 0;    // 记录最近一次起跳时间
    uint32_t lastMoveTime  = 0;    // [新增] 记录爬行开始时间，用于耐性牵引判断
};

// --- 边缘活性粒子 ---
struct EdgeParticle {
    float x, y;      // 屏幕坐标 (field grid 单位)
    float vx, vy;    // 速度
    float life;      // 1.0 → 0
    uint8_t type;    // 0=spike 尖刺, 1=drip 液滴, 2=filament 细丝
    float nx, ny;    // 边缘法线方向
    float len;       // 初始长度 (spike/filament)
};

struct AudioFeatures {
    float volume;    // 整体音量
    float harshness; // 高频比例/刺耳度
};

class Venom {
public:
    Venom();
    void update(float gx, float gy, float gz, float soundLevel = 0, float lux = 0);
    void setStartled();
    void draw(M5Canvas* canvas, float ax, float ay, float az);
    void toggleDebug() { showDebug = !showDebug; }
    void drawDebug(M5Canvas* canvas);

    // --- 外部接口 ---
    void triggerExpression(const String& type);
    void updateStateFromAudio(const AudioFeatures& features);
    void updateStateFromLLM(const String& jsonEmotion);
    String getPhysiologyJson();
    String getPerceptions();
    void addRecentEvent(const String& event);
    String getRecentEventsString();
    void triggerAISync() { isAIPendingSync = true; }
    void setAISyncInterval(uint32_t ms) { currentAISyncInterval = ms; }
    uint32_t getAISyncInterval() const { return currentAISyncInterval; }
    bool pullAIPendingSync() {
        bool pending = isAIPendingSync;
        isAIPendingSync = false;
        return pending;
    }

private:
    // --- [NEW] 神经意识核心心理状态 (LLM 欲望与情绪倾向驱动) ---
    std::vector<String> recentEvents;
    String lState_emotional_shift;
    String lState_focus_target;
    String lState_desire;
    float lState_surface_instability;
    float lState_impulse_strength;
    float lState_social_openness;
    float lState_curiosity_drift;
    String lState_notes;

    Skeleton skeleton;
    VenomState state;
    uint32_t stateTimer;
    float targetX, targetY, targetZ;
    float pupilX, pupilY;
    float pupilSize;        // 瞳孔大小: 0.2 (缩小) -> 1.0 (放大)
    float targetPupilSize; 
    uint8_t field[FIELD_W * FIELD_H];
    Face currentFace;
    VenomVisualState vState; // 核心视觉状态
    
    // 触手牵引与跳跃相关
    float grappleX, grappleY, grappleZ; 
    float grappleProgress;              
    bool isGrappling;                   
    bool isLeaping;                     
    uint32_t lastBlinkTime;
    bool isBlinking;

    VenomCrawlState crawlCycle;
    uint32_t crawlTimer;
    int activeHand; // 0: Left, 1: Right
    
    // 拟人肢体系统
    float handLX, handLY, handLZ;
    float handRX, handRY, handRZ;
    float handProgressL, handProgressR;

    // --- 生理与情绪系统 (增强型) ---
    float energy;           // 0.0: 疲劳, 1.0: 活跃
    float stress;           // 0.0: 放松, 1.0: 恐慌
    float curiosity;        // 0.0: 冷漠, 1.0: 探求
    float comfort;          // 0.0: 不适, 1.0: 安逸
    float attachment;       // 0.0: 疏离, 1.0: 粘人
    float fatigue;          // 疲劳度 (由高强度活动积累)
    float vigilance;        // 警觉度 (由环境刺激积累)
    float irritation;       // 烦躁度 (由连续干扰积累)
    float muscleTension;    // 肌肉张力 (影响动作刚性)
    float overstimulation;  // 过度刺激 (导致行为混乱)
    float recoveryProgress; // 恢复进度
    float expressionDesire; // 表达欲望: 积累后触发符号表达
    float rhythmScore;      // 节奏匹配得分
    uint32_t lastRhythmTime; 
    int rhythmCount;

    // --- 身体控制参数 (由技能驱动) ---
    float movementSpeed;      // 移动速率
    float bodyTension;        // 身体张力 (影响 Metaball 聚合度与骨骼刚度)
    float bodyDamping;        // 身体阻尼 (骨骼运动保留率)
    float movementPause;      // 新增：运动间歇停顿时间 (ms)
    float pullForceScale;     // 新增：拉力强度缩放
    float edgeAffinity;       // 边缘吸引力
    float tentacleActivity;   // 触手活跃度
    float eyeFocus;           // 眼睛聚焦度 (0: 游离, 1: 凝视)
    
    // --- 呼吸与微动作 ---
    float breathingPhase;
    float breathingIntensity; // 呼吸幅度
    uint32_t lastTwitchTime;
    float twitchX, twitchY;   // 微抽动位移
    
    // --- 互动感增强与记忆系统 ---
    struct BehaviorMemory {
        uint32_t lastPanicTime = 0;
        uint32_t lastObserveTime = 0;
        uint32_t lastQuietTime = 0;
        uint32_t lastShakeTime = 0;
        float territoryX = 0, territoryY = 0, territoryZ = 0; // 偏好区域
    } memory;

    uint32_t stableTimer;      // IMU 稳定计时
    uint32_t observeStartTime; // 开始观察的时间
    uint32_t perceptionTimer;  // 感知-观察-犹豫延迟链计时器
    bool isShy;                // 是否处于躲避状态
    bool showDebug;            // 是否显示调试信息
    float lastGX, lastGY, lastGZ; 
    float lastSoundLevel;         
    float lastLux;
    uint32_t lastLLMResponseTime; // [新增] 最近一次接收 AI 指令的时间
    uint32_t currentAISyncInterval; // 自适应同步周期
    bool isAIPendingSync;           // 主动请求心跳标志

    // --- 技能系统 ---
    uint32_t skillTimer;
    uint32_t skillDuration;
    int startledJumpCount;     
    int startledPhase;         // 蛇式动作阶段: 0=立起, 1=后仰, 2=爆射, 3=飞行
    uint32_t phaseTimer;       // 阶段计时器
    float leapTargetX, leapTargetY, leapTargetZ; // 蛇式爆射目标
    String currentSkillName;
    float currentFPS;          // 帧率
    uint32_t lastFrameTime;    // 上一帧时间
    FluidSymbolManager symbolMgr;
    bool isBeingObserved() const { return observeStartTime > 0; }
    void selectSkill();
    void applySkillEffects();
    void updatePhysiology(float soundLevel, float gx, float gy, float gz, float lux);
    void selectNewCrawlTarget();

    void updateAI();
    void updateEye();
    void calculateField(Face face, float ax, float ay); 
    void drawBox(M5Canvas* canvas, float ax, float ay);
    void drawContainer(M5Canvas* canvas, float ax, float ay); 
    void drawBackground(M5Canvas* canvas, float px, float py);
    void drawBody(M5Canvas* canvas, float px, float py, float ax, float ay);
    void drawEdgeActivity(float ax, float ay);
    void drawGloss(M5Canvas* canvas, float px, float py, float ax, float ay);
    void drawEye(M5Canvas* canvas, float px, float py, float ax, float ay);
    void drawTendrils(float endX, float endY, float prog, int handIdx, float ax, float ay);
    void projectToFace(const Node& n, Face face, float& outX, float& outY, float& outZ, float ax, float ay);
    void projectPoint(float x, float y, float z, Face face, float& outX, float& outY, float& outZ, float ax, float ay);

    // 边缘粒子池
    EdgeParticle edgeParticles[24];
    int edgeParticleCount;
    uint32_t lastEdgeSpawnTime;
    // Voronoi 漂移中心 (6 个)
    float voronoiCX[6], voronoiCY[6];
    float voronoiVX[6], voronoiVY[6];
};

#endif
