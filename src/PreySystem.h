#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "config.h"

// 虫子类型
enum BugType {
    BUG_CRAWLER = 0, // 地面/玻璃爬虫 (6足交替爬行)
    BUG_FLYER        // 飞虫 (双翅高频扑棱轻盈飞行)
};

// 虫子被捕食状态
enum PreyState {
    PREY_FREE = 0,       // 自由移动/探索
    PREY_SNARED,         // 被黏液粘在玻璃上挣扎定身
    PREY_GRABBED,        // 被黑色触手抓牢拉向嘴边
    PREY_TONGUE_HOOKED,  // 被长舌头卷中拉回
    PREY_EATEN           // 被吞入腹中
};

struct PreyBug {
    bool active = false;
    BugType type = BUG_CRAWLER;
    PreyState state = PREY_FREE;

    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;

    float speed = 15.0f;
    float heading_angle = 0.0f;
    float anim_phase = 0.0f;
    float leg_phase = 0.0f;
    float wing_phase = 0.0f;

    float state_timer = 0.0f;
    float life_timer = 0.0f;
    float pause_timer = 0.0f;

    // 黏液定身参数
    float snare_timer = 0.0f;
    float snare_x = 0.0f;
    float snare_y = 0.0f;
};

// 飞向虫子的黏液射弹
struct SlimeProjectile {
    bool active = false;
    float x = 0.0f;
    float y = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float start_x = 0.0f;
    float start_y = 0.0f;
    float progress = 0.0f;
    float speed = 180.0f; // px/s
    int target_bug_idx = -1;
};

class PreySystem {
public:
    static constexpr int MAX_BUGS = 2;

    PreySystem();

    void init();
    void update(float dt);
    void draw(M5Canvas &canvas) const;

    // 猎物探测与交互
    int findClosestBug(float from_x, float from_y, float &out_dist, float max_radius = 160.0f) const;
    bool getBugPos(int bug_idx, float &out_x, float &out_y) const;
    PreyState getBugState(int bug_idx) const;

    // 捕食动作接口
    void hookBugWithTongue(int bug_idx);
    void grabBugWithTentacle(int bug_idx);
    void launchSlimeSnare(float from_x, float from_y, int bug_idx);
    void eatBug(int bug_idx);
    void updateSnaredBugPos(int bug_idx, float new_x, float new_y);

    int getActiveBugCount() const;

private:
    PreyBug bugs[MAX_BUGS];
    SlimeProjectile slime_proj;
    float spawn_timer = 0.0f;

    void spawnBug();
    void updateCrawler(PreyBug &bug, float dt);
    void updateFlyer(PreyBug &bug, float dt);
    void updateSlimeProjectile(float dt);

    void drawCrawler(M5Canvas &canvas, const PreyBug &bug) const;
    void drawFlyer(M5Canvas &canvas, const PreyBug &bug) const;
    void drawSnareEffect(M5Canvas &canvas, const PreyBug &bug) const;
    void drawSlimeProjectile(M5Canvas &canvas) const;
};
