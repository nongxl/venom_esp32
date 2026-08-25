#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"

enum BugType {
    BUG_CRAWLER = 0,    // 边缘爬虫 (甲虫形态)
    BUG_FLYER           // 空中飞虫 (萤火虫/飞蝇形态)
};

enum BugState {
    BUG_STATE_INACTIVE = 0,
    BUG_STATE_ROAMING,  // 自由活动中
    BUG_STATE_SLIMED,   // 被黏液黏住定身
    BUG_STATE_CAPTURED, // 被舌头或触手抓取移动中
    BUG_STATE_EATEN     // 已被吞食
};

struct Bug {
    bool active = false;
    BugType type = BUG_CRAWLER;
    BugState state = BUG_STATE_ROAMING;

    float x = 120.0f;
    float y = 50.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float base_speed = 18.0f;

    // 爬虫贴壁信息
    int wall_side = 0; // 0:top, 1:right, 2:bottom, 3:left
    float move_timer = 0.0f;
    float pause_timer = 0.0f;

    // 飞虫摆动相位与荧光
    float fly_phase = 0.0f;
    float wing_phase = 0.0f;
    float glow_intensity = 1.0f;

    float life_timer = 0.0f;
    float max_life = 15.0f; // 存活时间 (秒)
};

class BugSystem {
public:
    BugSystem();

    void init();
    void update(float dt);
    void draw(M5Canvas &canvas) const;

    bool hasActiveBug() const { return current_bug.active && current_bug.state != BUG_STATE_EATEN; }
    const Bug& getActiveBug() const { return current_bug; }

    void getBugPos(float &bx, float &by) const { bx = current_bug.x; by = current_bug.y; }

    // 外部进食交互接口
    void setBugSlimed();
    void setBugCaptured(float to_x, float to_y);
    void setBugEaten();

    void spawnBugManual(BugType type = BUG_FLYER);

private:
    Bug current_bug;
    float spawn_cooldown = 4.0f;

    void spawnRandomBug();
    void updateCrawlerPhysics(float dt);
    void updateFlyerPhysics(float dt);
};
