#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"

enum BugType {
    BUG_CRAWLER = 0,    // 爬虫：带6条细腿步态，忽快忽慢贴壁爬行
    BUG_FLYER           // 飞虫：透明微翅高频振动，空中盘旋折线飞行
};

enum BugState {
    BUG_FREE = 0,       // 自由活动
    BUG_SNARED,         // 被黏液弹定身，原地疯狂挣扎
    BUG_CAUGHT,         // 被舌头/触手咬中，拖拽位移中
    BUG_DEAD            // 已被吞噬
};

struct PreyBug {
    bool active = false;
    BugType type = BUG_CRAWLER;
    BugState state = BUG_FREE;

    float x = 120.0f;
    float y = 40.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float base_speed = 18.0f;
    float current_speed = 18.0f;
    float angle = 0.0f;

    // 动作与步态动效
    float leg_phase = 0.0f;
    float wing_phase = 0.0f;
    float speed_change_timer = 0.0f;
    float state_timer = 0.0f;
    float glow_phase = 0.0f;

    // 定身与被抓取信息
    float snare_timer = 0.0f;
    float snare_duration = 45.0f; // 蛛网自然挣扎脱困时限
};

class PreyBugSystem {
public:
    static constexpr int MAX_BUGS = 4; // 支持场上同时存在多只小虫

    PreyBugSystem();

    void init();
    void update(float dt, float venom_hx, float venom_hy);
    void draw(M5Canvas &canvas) const;

    // 查询可用活虫子 (支持优先查询被蛛网定身的储备粮)
    int getNearestBug(float x, float y, float &bug_x, float &bug_y, BugState &state, bool prefer_snared = false) const;

    // 捕食状态干预
    void snareBug(int idx);
    void catchBug(int idx, float at_x, float at_y);
    void killBug(int idx);

    bool hasActiveBug() const;
    int getActiveBugCount() const;
    void spawnBugImmediate();
    const PreyBug& getBug(int idx) const { return bugs[idx]; }

private:
    PreyBug bugs[MAX_BUGS];
    float spawn_cooldown = 2.0f;

    void spawnNewBug();
    void updateCrawler(PreyBug &b, float dt, float v_hx, float v_hy);
    void updateFlyer(PreyBug &b, float dt, float v_hx, float v_hy);

    void drawCrawler(M5Canvas &canvas, const PreyBug &b) const;
    void drawFlyer(M5Canvas &canvas, const PreyBug &b) const;
    void drawHollowSpiderWeb(M5Canvas &canvas, const PreyBug &b) const;
};
