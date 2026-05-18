#include "../Venom.h"
#include <ArduinoJson.h>
#include <math.h>
#include "../Noise.h"
#include "../Skills.h"

void Venom::projectToFace(const Node& n, Face face, float& outX, float& outY, float& outZ, float ax, float ay) {
    projectPoint(n.x, n.y, n.z, face, outX, outY, outZ, ax, ay);
}

void Venom::projectPoint(float x, float y, float z, Face face, float& outX, float& outY, float& outZ, float ax, float ay) {
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
    
    // 【核心增强】显著提升透视位移系数与基础深度感
    const float tiltFactor = 4.5f; 
    u += depth * (ay * tiltFactor); 
    v -= depth * (ax * tiltFactor); 

    // 增强近大远小效果：将透视基准从 250 降至 180
    float perspective = 180.0f / (180.0f + depth); 
    outX = cx + u * baseScale * perspective;
    outY = cy + v * baseScale * perspective;
    outZ = depth;
}

void Venom::calculateField(Face face, float ax, float ay) {
    memset(field, 0, sizeof(field));
    float gMag = sqrtf(ax*ax + ay*ay);
    bool isStartled = (currentSkillName == "startled");
    
    auto drawBlob = [&](float mx, float my, float mz, float nodeRadius, float vx, float vy, float vz) {
        float px, py, pz;
        projectPoint(mx, my, mz, face, px, py, pz, ax, ay);
        if (pz > 150.0f) return;
        
        float perspective = 200.0f / (200.0f + pz);
        
        // 【物理对齐修复】获取投影后的 2D 速度矢量
        float px2, py2, pz2;
        projectPoint(mx + vx * 0.1f, my + vy * 0.1f, mz + vz * 0.1f, face, px2, py2, pz2, ax, ay);
        float v2dx = px2 - px;
        float v2dy = py2 - py;
        float vMag = sqrtf(v2dx*v2dx + v2dy*v2dy) * 10.0f;
        
        // 计算拉伸长度 (Trail Length)
        float trailScale = isStartled ? fmaxf(4.0f, fminf(15.0f, 1.0f + 0.6f * vMag)) : fminf(3.0f, 1.0f + 0.1f * vMag);
        // 动态瘦身：受惊时稍微变细以增加速度感，但保持基础厚度
        float thinning = 1.0f / sqrtf(1.0f + vMag * (isStartled ? 0.25f : 0.12f));
        if (isStartled) thinning = fmaxf(0.75f, thinning); // 确保即便高速下也不要细过 0.75
        
        // 引入呼吸起伏 (Breathing effect)
        float r = (nodeRadius * VISUAL_RADIUS_MULT + VISUAL_RADIUS_OFFSET) * vState.body_scale * thinning * breathingIntensity * powf(perspective, 0.8f) / FIELD_SCALE;

        float nx1 = px / FIELD_SCALE, ny1 = py / FIELD_SCALE;
        float nx0 = (px - v2dx * trailScale * 0.5f) / FIELD_SCALE;
        float ny0 = (py - v2dy * trailScale * 0.5f) / FIELD_SCALE;

        // 计算包围盒：包含整个线段及其半径
        int rx_min = (int)fminf(nx0, nx1) - (int)r - 4;
        int rx_max = (int)fmaxf(nx0, nx1) + (int)r + 4;
        int ry_min = (int)fminf(ny0, ny1) - (int)r - 4;
        int ry_max = (int)fmaxf(ny0, ny1) + (int)r + 4;

        float line_dx = nx1 - nx0;
        float line_dy = ny1 - ny0;
        float l2 = line_dx * line_dx + line_dy * line_dy;

        for (int y = fmaxf(0, ry_min); y < fminf(FIELD_H, ry_max); y++) {
            for (int x = fmaxf(0, rx_min); x < fminf(FIELD_W, rx_max); x++) {
                // 计算点到线段 (Capsule) 的距离
                float dist_px;
                if (l2 < 0.001f) {
                    float dx = x - nx1, dy = y - ny1;
                    dist_px = sqrtf(dx*dx + dy*dy);
                } else {
                    float t = fmaxf(0, fminf(1, ((x - nx0) * line_dx + (y - ny0) * line_dy) / l2));
                    float proj_x = nx0 + t * line_dx;
                    float proj_y = ny0 + t * line_dy;
                    float dx = x - proj_x, dy = y - proj_y;
                    dist_px = sqrtf(dx*dx + dy*dy);
                }

                float dist_ratio = dist_px / r;
                if (dist_ratio < 1.0f) {
                    float inv_ratio = 1.0f - dist_ratio * dist_ratio;
                    float val = inv_ratio * inv_ratio * 210.0f;
                    field[y * FIELD_W + x] = fminf(255, field[y * FIELD_W + x] + (int)val);
                }
            }
        }
    };

    // --- 1. 核心节点与插值路径 (主体场) ---
    for (int i = 0; i < MAX_NODES; i++) {
        const Node& curr = skeleton.getNode(i);
        drawBlob(curr.x, curr.y, curr.z, curr.radius, curr.vx, curr.vy, curr.vz);

        // 始终开启插值，确保在任何状态下身体都不会断裂成散点
        if (i > 0) {
            const Node& prev = skeleton.getNode(i-1);
            float dx = curr.x - prev.x, dy = curr.y - prev.y, dz = curr.z - prev.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            
            // 暴走时大幅增加插值密度，确保身体是一根连贯的针，而不是断开的球
            float stepSize = (isStartled) ? 2.5f : 4.5f;
            int steps = (int)(dist / stepSize); 
            
            for (int s = 1; s < steps; s++) {
                float t = (float)s / steps;
                float mx = prev.x + dx * t, my = prev.y + dy * t, mz = prev.z + dz * t;
                float interpolatedRadius = prev.radius * (1.0f - t) + curr.radius * t;
                float interpolatedVx = prev.vx * (1.0f - t) + curr.vx * t;
                float interpolatedVy = prev.vy * (1.0f - t) + curr.vy * t;
                float interpolatedVz = prev.vz * (1.0f - t) + curr.vz * t;
                drawBlob(mx, my, mz, interpolatedRadius, interpolatedVx, interpolatedVy, interpolatedVz);
            }
        }
    }

    // --- 2. 触手与爪部系统 (Cubic Bezier Arms) ---
    float gravMag = sqrtf(ax*ax + ay*ay);
    auto bezier = [](float p0, float p1, float p2, float p3, float t) -> float {
        float u = 1.0f - t;
        return u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3;
    };

    const Node& head = skeleton.getNode(0);
    float hpx, hpy, hpz;
    projectToFace(head, face, hpx, hpy, hpz, ax, ay);
    float hnx = hpx / FIELD_SCALE, hny = hpy / FIELD_SCALE;

    for (int hand = 0; hand < 2; hand++) {
        float prog = (hand == 0) ? handProgressL : handProgressR;
        if (prog >= 1.0f && hand != activeHand) continue;

        float hx = (hand == 0) ? handLX : handRX;
        float hy = (hand == 0) ? handLY : handRY;
        float hz = (hand == 0) ? handLZ : handRZ;

        float tx, ty, tz;
        projectPoint(hx, hy, hz, face, tx, ty, tz, ax, ay);
        float tnx = tx/FIELD_SCALE, tny = ty/FIELD_SCALE;
        float endX = hnx + (tnx-hnx)*prog, endY = hny + (tny-hny)*prog;

        // 计算重力下垂
        float midX = (hnx+endX)*0.5f, midY = (hny+endY)*0.5f;
        float sagX = ax * gravMag * 1.2f, sagY = ay * gravMag * 1.2f;
        float sagScale = 1.0f + (1.0f-bodyTension) * 2.5f;
        float cp1x = hnx*0.6f + midX*0.4f + sagX*sagScale;
        float cp1y = hny*0.6f + midY*0.4f + sagY*sagScale;
        float cp2x = endX*0.6f + midX*0.4f + sagX*sagScale;
        float cp2y = endY*0.6f + midY*0.4f + sagY*sagScale;

        float rootR = 2.0f + vState.surface_tension * 0.5f;
        float tipR  = 0.8f;
        int BEZ_STEPS = 14;
        float pbx = hnx, pby = hny;
        for (int s = 1; s <= BEZ_STEPS; s++) {
            float bt = (float)s / BEZ_STEPS;
            float bx = bezier(hnx, cp1x, cp2x, endX, bt);
            float by = bezier(hny, cp1y, cp2y, endY, bt);
            float r0 = rootR + (tipR-rootR)*(bt-1.0f/BEZ_STEPS);
            float r1 = rootR + (tipR-rootR)*bt;
            
            // 简单的场线段绘制
            float dist = sqrtf((bx-pbx)*(bx-pbx)+(by-pby)*(by-pby));
            int steps = (int)(dist/0.6f)+1;
            for(int k=0; k<=steps; k++) {
                float kt = (float)k/steps;
                float fx = pbx+(bx-pbx)*kt, fy = pby+(by-pby)*kt;
                float fr = r0+(r1-r0)*kt;
                int ir = (int)fmaxf(1, fr);
                for(int dy=-ir; dy<=ir; dy++) {
                    for(int dx=-ir; dx<=ir; dx++) {
                        if(dx*dx+dy*dy > fr*fr) continue;
                        int gx = (int)fx+dx, gy = (int)fy+dy;
                        if(gx>=0&&gx<FIELD_W&&gy>=0&&gy<FIELD_H) {
                            field[gy*FIELD_W+gx] = fminf(255, field[gy*FIELD_W+gx]+(int)(190*(1.0f-sqrtf(dx*dx+dy*dy)/(fr+0.1f))));
                        }
                    }
                }
            }
            pbx = bx; pby = by;
        }

        if (prog > 0.4f) {
            // 吸盘逻辑 (减小掌心半径，使其不那么明显)
            float padR = 1.2f + curiosity * 0.8f;
            for(int dy=-(int)padR; dy<=(int)padR; dy++) {
                for(int dx=-(int)padR; dx<=(int)padR; dx++) {
                    if(dx*dx+dy*dy > padR*padR) continue;
                    int gx=(int)endX+dx, gy=(int)endY+dy;
                    if(gx>=0&&gx<FIELD_W&&gy>=0&&gy<FIELD_H) {
                        field[gy*FIELD_W+gx] = fminf(255, field[gy*FIELD_W+gx]+(int)(195*(1.0f-sqrtf(dx*dx+dy*dy)/(padR+0.1f))));
                    }
                }
            }
            // 绘制生物爪指 (新爪形系统)
            drawTendrils(endX, endY, prog, hand, ax, ay);
        }
    }

    // --- 3. 尾部装饰胡须 (Decorative whiskers) ---
    for (int i = 2; i < MAX_NODES; i += 2) {
        const Node& n = skeleton.getNode(i);
        float px, py, pz;
        projectToFace(n, face, px, py, pz, ax, ay);
        if (pz > 120.0f) continue;
        float nx = px/FIELD_SCALE, ny = py/FIELD_SCALE;
        float tailDim = 1.0f - (float)i / (MAX_NODES * 1.2f);
        if (tailDim < 0.1f) continue;

        float baseAng = millis()*0.003f + i*0.9f;
        float gravAng = atan2f(ay, ax) + 3.14159f/2.0f;
        baseAng = baseAng*(1.0f-gravMag*0.5f) + gravAng*(gravMag*0.5f);
        float wLen = (8.0f + sinf(millis()*0.007f+i)*5.0f) * tailDim * (0.7f + vState.edge_activity*0.5f);

        float wx = nx, wy = ny;
        float wvx = cosf(baseAng), wvy = sinf(baseAng);
        int wsteps = (int)(wLen / 0.8f);
        for (int s = 0; s < wsteps; s++) {
            float st = (float)s / wsteps;
            wvx += ax * 0.05f; wvy += ay * 0.05f;
            float vm = sqrtf(wvx*wvx+wvy*wvy);
            if(vm>0.01f) { wvx/=vm; wvy/=vm; }
            wx += wvx*0.8f; wy += wvy*0.8f;
            int gx=(int)wx, gy=(int)wy;
            if(gx>=0&&gx<FIELD_W&&gy>=0&&gy<FIELD_H) {
                field[gy*FIELD_W+gx] = fminf(255, field[gy*FIELD_W+gx]+(int)(115*(1.0f-st)*tailDim));
            }
        }
    }

    // --- 4. 边缘尖刺与纤维场贡献 ---
    drawEdgeActivity(ax, ay);

    // --- 5. 液态符号表达层渲染 (置于顶层，具有最强的生命感) ---
    int symCount = symbolMgr.getPointCount();
    for (int i = 0; i < symCount; i++) {
        const auto& sp = symbolMgr.getPoint(i);
        float nx = sp.x / FIELD_SCALE, ny = sp.y / FIELD_SCALE;
        float r = sp.radius / FIELD_SCALE;
        
        // 符号点同样应用变形，纠正方向
        float gx = 0, gy = 0, gMag = sqrtf(ax*ax + ay*ay);
        if (gMag > 0.1f) { gx = ax / gMag; gy = ay / gMag; }
        float stretch = fminf(2.6f, 1.0f + 0.5f * gMag);
        int r_int = (int)(r * stretch) + 8;

        for (int y = fmaxf(0, ny-r_int); y < fminf(FIELD_H, ny+r_int); y++) {
            for (int x = fmaxf(0, nx-r_int); x < fminf(FIELD_W, nx+r_int); x++) {
                float dx = x - nx, dy = y - ny;
                float dist_px = sqrtf(dx*dx + dy*dy);
                float dotG = (dx * gx + dy * gy) / (dist_px + 0.001f);
                
                float scaledDist = dist_px;
                if (dotG > 0) scaledDist /= (1.0f + dotG * (stretch - 1.0f));
                else scaledDist *= (1.0f - dotG * 0.15f);

                float dist_ratio = scaledDist / r;
                if (dist_ratio < 1.0f) {
                    float val = (0.5f + 0.5f * cosf(dist_ratio * 3.14159f)) * 200.0f * sp.life * (1.0f + (dx*gx + dy*gy)/r * 0.5f);
                    field[y * FIELD_W + x] = fminf(255, field[y * FIELD_W + x] + (int)val);
                }
            }
        }
    }
}

void Venom::drawTendrils(float endX, float endY, float prog, int handIdx, float ax, float ay) {
    // 【生物级爪形系统】实现 5 根非对称、随机长度的纤细手指
    const int numFingers = 5;
    bool isGripping = (prog > 0.9f);
    
    // 爪部扇面分布：集中在 120 度左右的“爪状”扇区
    float baseAngle = (handIdx == 0 ? 3.14f + 0.8f : -0.8f) + sinf(millis() * 0.004f) * 0.2f;
    
    for (int f = 0; f < numFingers; f++) {
        // 非均匀角度分布
        float relativeAng = (f - 2) * 0.4f + sinf(f * 1.5f + millis()*0.001f) * 0.1f; 
        float fAng = baseAngle + relativeAng;

        // 随机且动态的长度 (缩减 1/3)
        float fLenBase = isGripping ? 7.0f : (4.0f + sinf(millis() * 0.008f + f) * 2.0f);
        float fLenVar = (float)((millis() + f*100) % 27) * 0.1f; 
        float fLen = fmaxf(4.0f, (fLenBase + fLenVar) * (0.7f + vState.edge_activity * 0.5f));

        for (float fl = 0; fl < fLen; fl += 0.8f) {
            float t = fl / fLen;
            float fx = endX + cosf(fAng) * fl;
            float fy = endY + sinf(fAng) * fl;
            
            // 极致纤细处理
            float fr = fmaxf(0.2f, 1.0f * (1.0f - t * 0.9f));
            int ir = (int)fmaxf(1, fr);
            
            for (int dy = -ir; dy <= ir; dy++) {
                for (int dx = -ir; dx <= ir; dx++) {
                    int ifx = (int)fx + dx, ify = (int)fy + dy;
                    if (ifx >= 0 && ifx < FIELD_W && ify >= 0 && ify < FIELD_H) {
                        float dist = sqrtf(dx * dx + dy * dy);
                        if (dist <= fr) {
                            float opacity = (1.0f - dist / (fr + 0.1f)) * (1.0f - t * 0.5f);
                            field[ify * FIELD_W + ifx] = fminf(255, field[ify * FIELD_W + ifx] + (int)(180 * opacity));
                        }
                    }
                }
            }
        }
    }
}

void Venom::draw(M5Canvas* canvas, float ax, float ay, float az) {
    float cax = fmaxf(-0.85f, fminf(0.85f, ax * 0.45f));
    float cay = fmaxf(-0.85f, fminf(0.85f, ay * 0.45f));

    currentFace = FRONT;

    calculateField(currentFace, cax, cay);
    float px = cay * 0.1f, py = -cax * 0.1f;
    drawContainer(canvas, cax, cay);
    drawBackground(canvas, px * 0.5f, py * 0.5f);
    drawBody(canvas, px, py, cax, cay);
    drawGloss(canvas, px * 1.1f, py * 1.1f, cax, cay);
    drawEye(canvas, px * 1.1f, py * 1.1f, cax, cay);

    if (showDebug) drawDebug(canvas);
}

void Venom::drawContainer(M5Canvas* canvas, float ax, float ay) {
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
    
    // --- 高亮所有与毒液有接触的 3D 面 ---
    bool touched[6] = {false};
    float threshold = 12.0f; // 接触判定阈值
    for (int i = 0; i < MAX_NODES; i++) {
        const Node& n = skeleton.getNode(i);
        if (abs(n.x - (-CUBE_W)) < threshold) touched[LEFT] = true;
        if (abs(n.x - CUBE_W) < threshold) touched[RIGHT] = true;
        if (abs(n.y - (-CUBE_H)) < threshold) touched[TOP] = true;
        if (abs(n.y - CUBE_H) < threshold) touched[BOTTOM] = true;
        if (abs(n.z - (-CUBE_D)) < threshold) touched[BACK] = true;
        if (abs(n.z - CUBE_D) < threshold) touched[FRONT] = true;
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
        canvas->drawLine(px[i], py[i], px[(i+1)%4], py[(i+1)%4], 0x5AEB); // 使用更亮一点的深青色作为线框
    }
    for (int i = 0; i < 4; i++) {
        canvas->drawLine(screen_v[i][0], screen_v[i][1], px[i], py[i], 0x5AEB);
    }
}

void Venom::drawBackground(M5Canvas* canvas, float px, float py) {
    uint32_t t = millis() / 80;
    for (int y = 0; y < FIELD_H; y++) {
        for (int x = 0; x < FIELD_W; x++) {
            int val = field[y * FIELD_W + x];
            if (val > 30) {
                float jx = (float)((x * 17 + y * 31 + t) % 7 - 3) * 0.15f;
                float jy = (float)((x * 23 + y * 13 + t) % 7 - 3) * 0.15f;
                // 稍微增加阴影半径确保融合
                float r = (FIELD_SCALE * 0.6f) + (val / 255.0f) * 2.2f;
                canvas->fillCircle((int)(x * FIELD_SCALE + px + 4 + jx), 
                                   (int)(y * FIELD_SCALE + py + 4 + jy), 
                                   (int)r, COLOR_VENOM_SHADOW);
            }
        }
    }
}

void Venom::drawBody(M5Canvas* canvas, float px, float py, float ax, float ay) {
    uint32_t t = millis() / 60;
    static const int THRESHOLD = 45; 

    for (int fy = 0; fy < FIELD_H; fy++) {
        int seg_start = -1;
        for (int fx = 0; fx < FIELD_W; fx++) {
            bool active = (field[fy * FIELD_W + fx] > THRESHOLD);
            
            if (active && seg_start < 0) {
                seg_start = fx;
            } else if (!active && seg_start >= 0) {
                // 绘制当前识别到的连续线段
                int x_min = seg_start, x_max = fx - 1;
                int sy = (int)(fy * FIELD_SCALE + py);
                for (int dy = 0; dy < FIELD_SCALE; dy++) {
                    int screen_y = sy + dy;
                    if (screen_y < 0 || screen_y >= SCREEN_H) continue;
                    int sx_start = (int)(x_min * FIELD_SCALE + px);
                    int sx_end   = (int)(x_max * FIELD_SCALE + px) + FIELD_SCALE - 1;
                    canvas->drawFastHLine(sx_start, screen_y, sx_end - sx_start + 1, COLOR_VENOM_BODY);
                }
                seg_start = -1;
            }
        }
        // 处理行尾未闭合的线段
        if (seg_start >= 0) {
            int x_min = seg_start, x_max = FIELD_W - 1;
            int sy = (int)(fy * FIELD_SCALE + py);
            for (int dy = 0; dy < FIELD_SCALE; dy++) {
                int screen_y = sy + dy;
                if (screen_y < 0 || screen_y >= SCREEN_H) continue;
                int sx_start = (int)(x_min * FIELD_SCALE + px);
                int sx_end   = (int)(x_max * FIELD_SCALE + px) + FIELD_SCALE - 1;
                canvas->drawFastHLine(sx_start, screen_y, sx_end - sx_start + 1, COLOR_VENOM_BODY);
            }
        }
    }

    // --- 表面次生光泽与细节 (TA 层) ---
    float lightX = -0.7f, lightY = -0.7f;
    for (int fy = 1; fy < FIELD_H - 1; fy++) {
        for (int fx = 1; fx < FIELD_W - 1; fx++) {
            int val = field[fy * FIELD_W + fx];
            if (val < 65) continue;
            float nx = (float)(field[fy * FIELD_W + fx + 1] - field[fy * FIELD_W + fx - 1]);
            float ny = (float)(field[(fy + 1) * FIELD_W + fx] - field[(fy - 1) * FIELD_W + fx]);
            float mag = sqrtf(nx*nx + ny*ny + 0.01f);
            nx /= mag; ny /= mag;
            float dot = -(nx * lightX + ny * lightY);
            int sx = (int)(fx * FIELD_SCALE + px) + 1;
            int sy = (int)(fy * FIELD_SCALE + py) + 1;
            if (dot > 0.85f && val > 160) {
                canvas->drawPixel(sx, sy, COLOR_VENOM_GLOSS);
            } else if (dot > 0.70f && (fx+fy+t/8)%2==0) {
                canvas->drawPixel(sx, sy, 0xBDF7);
            } else if (dot < -0.80f && (fx+fy)%2==0) {
                canvas->drawPixel(sx, sy, 0x4208);
            }
        }
    }
}

void Venom::drawEdgeActivity(float ax, float ay) {
    uint32_t now = millis();
    float gMag = sqrtf(ax*ax + ay*ay);
    int aliveCount = 0;
    
    // 1. 处理存量粒子并渲染到场中
    for (int i = 0; i < edgeParticleCount; i++) {
        EdgeParticle& ep = edgeParticles[i];
        if (ep.life <= 0) continue;
        ep.life -= (ep.type == 1) ? 0.024f : 0.034f; 
        if (ep.life <= 0) continue;

        if (ep.type == 0 || ep.type == 2) {
            // 尖刺/纤维：在 field 中生成连贯的能量点
            float spLen = ep.len * ep.life * (NOISE_SPIKE_SCALE / 80.0f);
            for (float l = 0; l < spLen; l += 0.5f) {
                float t = l / fmaxf(0.1f, spLen);
                float fx = ep.x + ep.nx * l;
                float fy = ep.y + ep.ny * l;
                if (ep.type == 2) { 
                    float wob = sinf(l * 2.0f + now * 0.012f) * 0.4f;
                    fx += ep.ny * wob; fy -= ep.nx * wob;
                }
                int ix = (int)fx, iy = (int)fy;
                if (ix >= 0 && ix < FIELD_W && iy >= 0 && iy < FIELD_H) {
                    int val = (int)(220.0f * ep.life * (1.0f - t * 0.65f));
                    field[iy * FIELD_W + ix] = fminf(255, field[iy * FIELD_W + ix] + val);
                }
            }
        } else if (ep.type == 1) {
            // 飞溅的水滴：同样写入 field，产生靠近身体时的粘连效果
            ep.vx += ax * 0.015f; ep.vy += ay * 0.015f;
            ep.x += ep.vx; ep.y += ep.vy;
            int ix = (int)ep.x, iy = (int)ep.y;
            if (ix >= 0 && ix < FIELD_W && iy >= 0 && iy < FIELD_H) {
                int val = (int)(165.0f * ep.life);
                field[iy * FIELD_W + ix] = fminf(255, field[iy * FIELD_W + ix] + val);
            }
        }
        edgeParticles[aliveCount++] = ep;
    }
    edgeParticleCount = aliveCount;

    // 2. 生成新粒子 (从 field 边缘激发出尖刺)
    if (now - lastEdgeSpawnTime > (uint32_t)(100.0f / (0.1f + vState.edge_activity)) && edgeParticleCount < 25) {
        lastEdgeSpawnTime = now;
        int cx = -1, cy = -1;
        for (int a = 0; a < 20; a++) {
            int tx = random(1, FIELD_W - 1), ty = random(1, FIELD_H - 1);
            int v = field[ty * FIELD_W + tx];
            if (v >= 40 && v <= 80) { cx = tx; cy = ty; break; }
        }
        if (cx >= 0) {
            float gnx = -(float)(field[cy * FIELD_W + (cx + 1)] - field[cy * FIELD_W + (cx - 1)]);
            float gny = -(float)(field[(cy + 1) * FIELD_W + cx] - field[(cy - 1) * FIELD_W + cx]);
            float m = sqrtf(gnx * gnx + gny * gny);
            if (m > 0.1f) {
                EdgeParticle& ep = edgeParticles[edgeParticleCount++];
                ep.x = cx; ep.y = cy; ep.nx = gnx / m; ep.ny = gny / m; ep.vx = 0; ep.vy = 0; ep.life = 1.0f;
                float roll = (float)random(100) / 100.0f;
                if (roll < 0.45f + vState.spike_intensity * 0.35f) {
                    ep.type = 0; ep.len = 1.8f + vState.spike_intensity * 4.0f + vState.edge_activity * 2.5f;
                } else if (roll < 0.75f + vState.spike_intensity * 0.3f + gMag * 0.15f) {
                    ep.type = 1; ep.vx = ep.nx * 0.05f; ep.vy = ep.ny * 0.05f; ep.len = 0;
                } else {
                    ep.type = 2; ep.len = 2.5f + vState.edge_activity * 3.5f;
                }
            }
        }
    }
}

void Venom::drawGloss(M5Canvas* canvas, float px, float py, float ax, float ay) {
    uint32_t t = millis();
    for (int y = 5; y < FIELD_H - 5; y += 4) {
        for (int x = 5; x < FIELD_W - 5; x += 4) {
            if (field[y * FIELD_W + x] > 200 && (x * 7 + y * 13 + t / 200) % 17 == 0) {
                float jx = sinf(t * 0.005f + x) * 2.0f;
                float jy = cosf(t * 0.005f + y) * 2.0f;
                canvas->fillCircle((int)(x * FIELD_SCALE + px + jx), (int)(y * FIELD_SCALE + py + jy), 1, COLOR_VENOM_GLOSS);
            }
        }
    }
}

void Venom::drawEye(M5Canvas* canvas, float px, float py, float ax, float ay) {
    if (currentSkillName != "sleep" && isBlinking && millis() - lastBlinkTime < 120) return;

    // 直接用骨骼头节点 (Node 0) 的屏幕投影坐标作为眼睛位置
    const Node& head = skeleton.getNode(0);
    float hx, hy, hz;
    projectToFace(head, currentFace, hx, hy, hz, ax, ay);

    // 眼睛偏移：跟随瞳孔视线方向
    float eyeX = hx + pupilX * 8.0f;
    float eyeY = hy + pupilY * 6.0f;

    if (currentSkillName == "sleep") {
        // [修复] 锁定眼部位置在头部中心，完全忽略 pupilX/Y 的偏移
        int ix = (int)hx, iy = (int)hy;
        int sw = 10 * vState.body_scale;
        // 绘制可爱的向下弯曲弧线 (眯眯眼)
        canvas->drawArc(ix, iy - 2, sw, sw, 30, 150, TFT_DARKGREY);
        canvas->drawArc(ix, iy - 1, sw-1, sw-1, 35, 145, 0x3186); 
        return;
    }

    // 纯白色眼白
    canvas->fillEllipse((int)eyeX, (int)eyeY, 14, 10, COLOR_EYE_WHITE);
    canvas->drawEllipse((int)eyeX, (int)eyeY, 14, 10, COLOR_VENOM_BODY);

    // 瞳孔
    float pSize = 3.5f * pupilSize;
    if (pSize < 1.0f) pSize = 1.0f;
    canvas->fillCircle((int)(eyeX + pupilX * 3.5f),
                       (int)(eyeY + pupilY * 2.5f),
                       (int)pSize, COLOR_VENOM_BODY);

    // 高压反光点
    canvas->drawPixel((int)(eyeX - 4), (int)(eyeY - 3), 0xFFFF);

    // 紧张状态：极轻微血丝
    if (stress > 0.72f) { 
        int dotCount = (int)((stress - 0.72f) * 15.0f) + 2;
        for (int i = 0; i < dotCount; i++) {
            float ang = (float)random(360) * 0.0174f;
            float dist = 7.0f + (float)random(5);
            canvas->drawPixel((int)(eyeX + cosf(ang) * dist),
                             (int)(eyeY + sinf(ang) * dist * 0.75f), 0xF800);
        }
    }
}

void Venom::drawDebug(M5Canvas* canvas) {
    if (!showDebug) return;

    // --- 1. 面板定位 (加宽以实现左右边距对称) ---
    int bw = 230, bh = 115;
    int bx = (SCREEN_W - bw) / 2; // 居中定位: bx = 5
    int by = 6;
    
    // 恢复全透明样式，但保留柔和配色以减少残影
    uint16_t cyan = 0x059d; 
    uint16_t glow = 0x028A; 
    uint16_t red  = 0x9800;
    
    // --- 2. 绘制装饰角标 (HUD Frame Corners) ---
    // 左上
    canvas->drawLine(bx, by, bx+10, by, cyan);
    canvas->drawLine(bx, by, bx, by+10, cyan);
    // 右上
    canvas->drawLine(bx+bw, by, bx+bw-10, by, cyan);
    canvas->drawLine(bx+bw, by, bx+bw, by+10, cyan);
    // 左下
    canvas->drawLine(bx, by+bh, bx+10, by+bh, cyan);
    canvas->drawLine(bx, by+bh, bx, by+bh-10, cyan);
    // 右下
    canvas->drawLine(bx+bw, by+bh, bx+bw-10, by+bh, cyan);
    canvas->drawLine(bx+bw, by+bh, bx+bw, by+bh-10, cyan);

    // --- 3. 辅助绘制函数 ---
    char buf[64];
    auto drawGlowText = [&](const char* text, int x, int y, uint16_t color) {
        canvas->setTextColor(glow);
        canvas->setCursor(x+1, y+1); canvas->print(text);
        canvas->setTextColor(color);
        canvas->setCursor(x, y); canvas->print(text);
    };

    // --- 增强型 HUD 诊断信息 (已整合) ---
    char diagBuf[64];
    
    // 计算物理抖动度 (各节点速度的标准差)
    float v_sum = 0, v_sq_sum = 0;
    for(int i=0; i<MAX_NODES; i++) {
        float v_mag = sqrtf(skeleton.getNode(i).vx*skeleton.getNode(i).vx + skeleton.getNode(i).vy*skeleton.getNode(i).vy);
        v_sum += v_mag;
        v_sq_sum += v_mag * v_mag;
    }
    float v_mean = v_sum / MAX_NODES;
    float jitter = sqrtf(fmaxf(0, v_sq_sum / MAX_NODES - v_mean * v_mean));

    snprintf(diagBuf, sizeof(diagBuf), "BEHAVIOR: %s", currentSkillName.c_str());
    drawGlowText(diagBuf, bx+12, by+10, cyan); 
    
    snprintf(diagBuf, sizeof(diagBuf), "FPS: %d", (int)currentFPS);
    drawGlowText(diagBuf, bx+bw-85, by+10, TFT_WHITE); 
    
    snprintf(diagBuf, sizeof(diagBuf), "JIT: %.3f", jitter);
    drawGlowText(diagBuf, bx+bw-85, by+22, (jitter < 0.5f) ? 0x07E0 : red); 

    // 如果抖动过大且处于闲置，在串口输出预警
    if (jitter > 1.5f && currentSkillName == "idle") {
        M5.Log.printf(">>> [Diag] High Jitter: %.3f\n", jitter);
    }

    auto drawDataBar = [&](int x, int y, float val, uint16_t color) {
        int barW = 50;
        canvas->drawRect(x, y, barW, 4, glow);
        canvas->fillRect(x, y, (int)(val * barW), 4, color);
    };

    // --- 4. 渲染数据项 ---
    canvas->setTextSize(1);
    int x = bx + 6, y = by + 8;
    int x2 = bx + bw - 85; // 右侧列左起始 (保持左对齐)
    
    // --- 左列：核心生理参数 ---
    int ly = y + 16;
    drawGlowText("NRG", x, ly, cyan); drawDataBar(x + 30, ly + 2, energy, cyan);
    
    ly += 10; drawGlowText("STR", x, ly, (stress > 0.6f) ? red : cyan);
    drawDataBar(x + 30, ly + 2, stress, (stress > 0.6f) ? red : cyan);
    
    ly += 10; drawGlowText("FAT", x, ly, (fatigue > 0.6f) ? red : cyan);
    drawDataBar(x + 30, ly + 2, fatigue, (fatigue > 0.6f) ? red : cyan);
    
    ly += 10; drawGlowText("VIG", x, ly, cyan);
    drawDataBar(x + 30, ly + 2, vigilance, cyan);

    ly += 10; drawGlowText("COM", x, ly, cyan);
    drawDataBar(x + 30, ly + 2, comfort, cyan);

    ly += 10; drawGlowText("TNS", x, ly, cyan);
    drawDataBar(x + 30, ly + 2, muscleTension, cyan);

    ly += 10; drawGlowText("OVR", x, ly, (overstimulation > 0.5f) ? red : cyan);
    drawDataBar(x + 30, ly + 2, overstimulation, (overstimulation > 0.5f) ? red : cyan);

    // --- 右列：传感器与系统 ID (整列居右，内容左对齐) ---
    int ry = y + 24;
    float roll = atan2f(lastGY, lastGZ) * 57.2958f;
    float pitch = atan2f(-lastGX, sqrtf(lastGY*lastGY + lastGZ*lastGZ)) * 57.2958f;
    
    sprintf(diagBuf, "IMU R:%dP:%d", (int)roll, (int)pitch); 
    drawGlowText(diagBuf, x2, ry, 0x9630);
    
    ry += 12;
    sprintf(buf, "MIC %.1fdB", lastSoundLevel); 
    drawGlowText(buf, x2, ry, 0x9630);
    
    ry += 12;
    sprintf(buf, "LUX %.1f", lastLux);
    drawGlowText(buf, x2, ry, (lastLux > 1000) ? 0xF800 : 0xFFE0); 

    // --- [新增] AI 驱动指示器 (黑海胆动态图标) ---
    uint32_t now = millis();
    // 判定逻辑：最近 5 分钟内收到过 AI 指令则显示
    if (lastLLMResponseTime > 0 && (now - lastLLMResponseTime < 300000)) {
        int ux = x2 + 55, uy = ry + 25; // 居右下方
        float time = now * 0.003f;
        
        // 1. 绘制核心
        canvas->fillCircle(ux, uy, 4, COLOR_VENOM_BODY);
        
        // 2. 绘制动态尖刺 (12 根不规则尖刺)
        for(int i=0; i<12; i++) {
            float angle = (i * 30 + (int)(sinf(time + i) * 12)) * 0.0174f;
            float len = 6 + sinf(time * 2.0f + i * 1.5f) * 5; // 长度伸缩
            int x1 = ux + cosf(angle) * 3;
            int y1 = uy + sinf(angle) * 3;
            int x2_p = ux + cosf(angle) * len;
            int y2_p = uy + sinf(angle) * len;
            canvas->drawLine(x1, y1, x2_p, y2_p, COLOR_VENOM_BODY);
            canvas->drawPixel(x2_p, y2_p, 0x4208); 
        }
        
        // 3. 标注 AI 状态
        drawGlowText("NEURAL_LINK", ux - 35, uy + 12, 0x4208);
    }

    // --- 4. 动态装饰: 扫描线 ---
    static float scanY = 0;
    scanY += 0.8f;
    if (scanY > bh) scanY = 0;
    canvas->drawFastHLine(bx + 2, by + (int)scanY, bw - 4, 0x18C3); // 极淡的扫描线
}

void Venom::drawBox(M5Canvas* canvas, float ax, float ay) {
    // 绘制 3D 盒子的线框以增强空间感
    static const float corners[8][3] = {
        {-CUBE_W, -CUBE_H, -CUBE_D}, {CUBE_W, -CUBE_H, -CUBE_D},
        {CUBE_W, CUBE_H, -CUBE_D}, {-CUBE_W, CUBE_H, -CUBE_D},
        {-CUBE_W, -CUBE_H, CUBE_D}, {CUBE_W, -CUBE_H, CUBE_D},
        {CUBE_W, CUBE_H, CUBE_D}, {-CUBE_W, CUBE_H, CUBE_D}
    };
    
    float px[8], py[8], pz[8];
    for (int i = 0; i < 8; i++) {
        projectPoint(corners[i][0], corners[i][1], corners[i][2], currentFace, px[i], py[i], pz[i], ax, ay);
    }
    
    uint16_t boxColor = 0x4208; // 暗灰色线框
    // 绘制 12 条棱 (px/py 已经是绝对屏幕坐标)
    for (int i = 0; i < 4; i++) {
        canvas->drawLine(px[i], py[i], px[(i+1)%4], py[(i+1)%4], boxColor);
        canvas->drawLine(px[i+4], py[i+4], px[((i+1)%4)+4], py[((i+1)%4)+4], boxColor);
        canvas->drawLine(px[i], py[i], px[i+4], py[i+4], boxColor);
    }
}
