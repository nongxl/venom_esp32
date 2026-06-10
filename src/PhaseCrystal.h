#ifndef PHASE_CRYSTAL_H
#define PHASE_CRYSTAL_H

#include "IOrganism.h"
#include "Container.h"
#include "physics/Skeleton.h"

#define MAX_FINGERS 6
#define NUM_CRACKS  12
#define MAX_SHARDS  24

struct CrystalSeg {
    float brightness;
    float slipX, slipY;
};

struct CrystalFinger {
    float perpOffset;    // lateral offset from palm center (screen px)
    float lengthScale;   // 0.68 - 1.36
    float liftAmount;    // pixels lifted off ground (opposite gravity)
    float wigglePhase;   // per-finger animation phase
    float stepPhase;     // gait phase offset (0-1)
    CrystalSeg seg[3];   // 3 phalanx segments
};

struct CrystalCrack {
    float px, py;    // palm-relative position
    float angle, len;
    float brightness;
};

struct CrystalShard {
    float x, y, z;      // 3D container space
    float vx, vy, vz;   // velocities
    float size;
    uint32_t birth;
    uint32_t life;
    bool active;
};

class PhaseCrystal : public IOrganism {
public:
    PhaseCrystal();

    void update(float gx, float gy, float gz, float soundLevel = 0, float lux = 0) override;
    void draw(M5Canvas* canvas, Container* container, float ax, float ay, float az) override;

    void setStartled() override;
    void toggleDebug() override { showDebug = !showDebug; }
    bool isDebugVisible() const override { return showDebug; }
    void drawDebug(M5Canvas* canvas) override;

    void updateStateFromAudio(const AudioFeatures& features) override {}
    void updateStateFromLLM(const String& jsonEmotion) override {}
    String getPhysiologyJson() override { return "{\"type\":\"Phase_Crystalline_Organism\"}"; }
    String getPerceptions() override { return "- I am a phase-crystal life form. I crawl with crystalline fingers."; }
    void addRecentEvent(const String& event) override {}
    String getRecentEventsString() override { return ""; }

    bool pullAIPendingSync() override { return false; }
    void triggerAISync() override {}
    void setAISyncInterval(uint32_t ms) override {}
    uint32_t getAISyncInterval() const override { return 60000; }

private:
    Skeleton skeleton;
    bool showDebug;
    float currentFPS;
    uint32_t lastFrameTime;
    uint32_t moveTimer;
    uint32_t stepTimer;
    uint32_t animTime;
    float ambientLux;
    // 爬行位置（3D 容器坐标系，贴在前面板上）
    float crawlX, crawlY, crawlZ;
    float nextCrawlX, nextCrawlY, nextCrawlZ;
    bool isMoving;
    bool isStepping;

    int numFingers;
    CrystalFinger fingers[MAX_FINGERS];
    CrystalCrack cracks[NUM_CRACKS];
    CrystalShard shards[MAX_SHARDS];

    void drawCrystalHand(M5Canvas* canvas, Container* container, float ax, float ay);
    void fillQuad(M5Canvas* canvas, int x0, int y0, int x1, int y1,
                  int x2, int y2, int x3, int y3, uint16_t color);
    void edgeQuad(M5Canvas* canvas, int x0, int y0, int x1, int y1,
                  int x2, int y2, int x3, int y3, uint16_t color);
};

#endif // PHASE_CRYSTAL_H
