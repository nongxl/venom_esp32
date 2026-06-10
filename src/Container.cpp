#include "Container.h"
#include <math.h>

Container::Container() : currentFace(FRONT) {}

void Container::projectToFace(const Node& n, Face face, float& outX, float& outY, float& outZ, float ax, float ay) const {
    projectPoint(n.x, n.y, n.z, face, outX, outY, outZ, ax, ay);
}

void Container::projectPoint(float x, float y, float z, Face face, float& outX, float& outY, float& outZ, float ax, float ay) const {
    float u = 0, v = 0, depth = 0;
    const float baseScale = (float)SCREEN_W / (CUBE_W * 2.5f); 
    const float cx = SCREEN_W / 2.0f, cy = SCREEN_H / 2.0f;

    switch (face) {
        case FRONT:  u = x;  v = y;  depth = CUBE_D - z; break;
        case BACK:   u = -x; v = y;  depth = z + CUBE_D; break;
        case LEFT:   u = z;  v = y;  depth = x + CUBE_W; break;
        case RIGHT:  u = -z; v = y;  depth = CUBE_W - x; break;
        case TOP:    u = x;  v = -z; depth = CUBE_D - y; break;
        case BOTTOM: u = x;  v = z;  depth = y + CUBE_H; break;
    }
    
    // 【深度校正】降低倾斜透视位移系数，防止形成过深隧道感
    const float tiltFactor = 1.4f; 
    u += depth * (ay * tiltFactor); 
    v -= depth * (ax * tiltFactor); 

    // 平缓近大远小效果：将透视基准从 180 提高到 220
    float perspective = 220.0f / (220.0f + depth); 
    
    // 【终极纠偏：自适应正交投影融合 (Perspective Boundary Guard)】
    float orthoX = cx + u * baseScale;
    float orthoY = cy + v * baseScale;
    float perspX = cx + u * baseScale * perspective;
    float perspY = cy + v * baseScale * perspective;
    
    // 计算 3D 边界盒贴近比率，忽略 Z 轴深度感对屏幕四周物理边缘贴合的混淆
    float distX = CUBE_W - abs(x);
    float distY = CUBE_H - abs(y);
    
    outX = perspX;
    outY = perspY;
    if (distX < 12.0f) {
        float guardX = (1.0f - distX / 12.0f);
        outX = orthoX * guardX + perspX * (1.0f - guardX);
    }
    if (distY < 12.0f) {
        float guardY = (1.0f - distY / 12.0f);
        outY = orthoY * guardY + perspY * (1.0f - guardY);
    }
    
    outZ = depth;
}

void Container::drawContainer(M5Canvas* canvas, float ax, float ay, const Skeleton& skeleton) const {
    static const float inner_v[4][3] = {
        {-CUBE_W, -CUBE_H, -CUBE_D}, {CUBE_W, -CUBE_H, -CUBE_D},
        {CUBE_W, CUBE_H, -CUBE_D}, {-CUBE_W, CUBE_H, -CUBE_D}
    };
    static const float screen_v[4][2] = {
        {0, 0}, {SCREEN_W, 0}, {SCREEN_W, SCREEN_H}, {0, SCREEN_H}
    };
    float px[4], py[4], pz[4];
    for (int i = 0; i < 4; i++) {
        projectPoint(inner_v[i][0], inner_v[i][1], inner_v[i][2], currentFace, px[i], py[i], pz[i], ax, ay);
    }
    
    // --- 高亮所有与生物有接触的 3D 面 (贴屏与贴后壁节点优先过滤以防边缘冲突) ---
    bool touched[6] = {false};
    float threshold = 4.0f; // 接触判定阈值下调
    for (int i = 0; i < MAX_NODES; i++) {
        const Node& n = skeleton.getNode(i);
        
        // 优先响应屏幕和后壁的紧密平贴，避免其滑到边缘时连带点亮3D侧壁投影
        if (abs(n.z - CUBE_D) < threshold) {
            touched[FRONT] = true;
            continue; 
        }
        if (abs(n.z - (-CUBE_D)) < threshold) {
            touched[BACK] = true;
            continue;
        }
        
        if (abs(n.x - (-CUBE_W)) < threshold) touched[LEFT] = true;
        if (abs(n.x - CUBE_W) < threshold) touched[RIGHT] = true;
        if (abs(n.y - (-CUBE_H)) < threshold) touched[TOP] = true;
        if (abs(n.y - CUBE_H) < threshold) touched[BOTTOM] = true;
    }

    uint16_t highlightColor = 0x0115;
    if (touched[FRONT]) {
        canvas->drawRect(0, 0, SCREEN_W, SCREEN_H, highlightColor);
        canvas->drawRect(1, 1, SCREEN_W-2, SCREEN_H-2, highlightColor);
    }
    if (touched[BACK]) {
        canvas->fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], highlightColor);
        canvas->fillTriangle(px[0], py[0], px[2], py[2], px[3], py[3], highlightColor);
    }
    // 侧面高亮
    for (int f = 0; f < 4; f++) {
        Face sideFace;
        if (f == 0) sideFace = TOP;
        else if (f == 1) sideFace = RIGHT;
        else if (f == 2) sideFace = BOTTOM;
        else sideFace = LEFT;

        if (touched[sideFace]) {
            int v1 = f;
            int v2 = (f + 1) % 4;
            canvas->fillTriangle(px[v1], py[v1], px[v2], py[v2], screen_v[v2][0], screen_v[v2][1], highlightColor);
            canvas->fillTriangle(px[v1], py[v1], screen_v[v2][0], screen_v[v2][1], screen_v[v1][0], screen_v[v1][1], highlightColor);
        }
    }

    // 绘制容器线框
    for (int i = 0; i < 4; i++) {
        canvas->drawLine(px[i], py[i], px[(i+1)%4], py[(i+1)%4], 0x5AEB); // 深青色线框
    }
    for (int i = 0; i < 4; i++) {
        canvas->drawLine(screen_v[i][0], screen_v[i][1], px[i], py[i], 0x5AEB);
    }
}
