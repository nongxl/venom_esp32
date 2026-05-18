#ifndef SKELETON_H
#define SKELETON_H

#include "config.h"

struct Node {
    float x, y, z;
    float vx, vy, vz;
    float radius;
    float tension;      // 节点张力 [0.0 - 1.0]
    float externalForce;// [新增] 触手牵引力大小，用于驱动剥离逻辑
    int impactGrace;    // 着陆保护计数器
    uint32_t lastStuckTime; // [新增] 上次粘附时间点
    bool is_stuck;
};

class Skeleton {
public:
    Skeleton();
    void update(float gx, float gy, float gz);
    void setDynamicPhysics(float stiffnessMult, float dampingMult);
    void setTargetNode(int index, float x, float y, float z, float strength);
    void clearTargetNode(int index);
    Node& getNode(int index) { return nodes[index]; } 
    void setRestLengthScale(float scale) { restLengthScale = scale; }
    
    bool forceDetach; // [新增] 强制脱离标志，必须公开以便外部控制

private:
    Node nodes[MAX_NODES];
    float restLengthScale; // [新增] 控制骨骼节段的基础静止长度比例
    float targetX[MAX_NODES];
    float targetY[MAX_NODES];
    float targetZ[MAX_NODES];
    float targetStrength[MAX_NODES];
    bool  hasTarget[MAX_NODES];

    void applyAntigravityBehavior(float gx, float gy, float gz);
    void applyInternalForces();
    void applyConstraints();
    void applyWallAttraction(); 

    float lastGx, lastGy, lastGz;
    uint32_t fallingTimer;
    bool isFalling;

    float stiffnessMultiplier;
    float dampingMultiplier;
};

#endif
