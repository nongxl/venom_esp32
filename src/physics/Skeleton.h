#ifndef SKELETON_H
#define SKELETON_H

#include "config.h"

struct Node {
    float x, y, z;
    float vx, vy, vz;
    float radius;       // 动态渲染半径，每帧基于体积守恒重算
    float baseRadius;   // [新增] 基础物理半径，防体积重算污染
    float tension;      // 节点张力 [0.0 - 1.0]
    float externalForce;// [新增] 触手牵引力大小，用于驱动剥离逻辑
    int impactGrace;    // 着陆保护计数器
    uint32_t lastStuckTime; // [新增] 上次粘附时间点
    float contactPressure;  // [新增] 接触压力场 [0.0 - 1.0]，基于距离表面位置
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
    const Node& getNode(int index) const { return nodes[index]; } 
    void setRestLengthScale(float scale) { restLengthScale = scale; }
    int getShapeArchetype() const { return shapeArchetype; } // [新增] 形态原型获取接口
    
    float getAndClearMaxCollisionSpeed() {
        float speed = maxCollisionSpeed;
        maxCollisionSpeed = 0.0f;
        return speed;
    }
    
    bool forceDetach; // [新增] 强制脱离标志，必须公开以便外部控制
    bool isSwinging;  // [新增] 荡秋千状态物理覆盖标志，若为 true 则全身强力解除 stuck 贴壁，允许重力悬摆

private:
    Node nodes[MAX_NODES];
    float restLengthScale; // [新增] 控制骨骼节段的基础静止长度比例
    float targetX[MAX_NODES];
    float targetY[MAX_NODES];
    float targetZ[MAX_NODES];
    float targetStrength[MAX_NODES];
    bool  hasTarget[MAX_NODES];
    int   shapeArchetype;  // [新增] 形态原型: 0:FREE, 1:GROUND, 2:WALL, 3:CEILING, 4:CORNER

    float getMinDistToSurface(float x, float y, float z) const; // [新增] 距离四壁的最小距离
    void applyAntigravityBehavior(float gx, float gy, float gz);
    void applyInternalForces();
    void applyConstraints();
    void applyWallAttraction(); 

    float lastGx, lastGy, lastGz;
    uint32_t fallingTimer;
    bool isFalling;

    float stiffnessMultiplier;
    float dampingMultiplier;
    float maxCollisionSpeed;
};

#endif
