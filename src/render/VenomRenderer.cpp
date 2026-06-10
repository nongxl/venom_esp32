#include "../Venom.h"
#include <ArduinoJson.h>
#include <math.h>
#include "../Noise.h"
#include "../Skills.h"



void Venom::calculateField(Container* container, float ax, float ay) {
    memset(field, 0, sizeof(field));
    float gMag = sqrtf(ax*ax + ay*ay);
    bool isStartled = (currentSkillName == "startled");

    // 呼吸相位差辅助解算器
    auto calcNodeBreathing = [&](float nodeIdx) -> float {
        float breathRange = BREATH_RANGE_BASE + stress * 0.15f;
        if (millis() - lastStartledTime < 120000) {
            breathRange *= 0.5f;
        }
        if (observer_trust > 0.6f) {
            breathRange *= 1.3f;
        }
        return 1.0f + sinf(breathingPhase - nodeIdx * 0.45f) * breathRange;
    };
    
    auto drawBlob = [&](float mx, float my, float mz, float nodeRadius, float vx, float vy, float vz, float nodeBreathing, int nodeIdx) {
        const Node& n = skeleton.getNode(nodeIdx);
        float pressure = n.contactPressure;
        
        float render_mx = mx;
        float render_my = my;
        float render_mz = mz;
        float render_radius = nodeRadius;
        
        // -------------------------------------------------------------
        // 【非对称贴墙饱满膨胀与偏移机制 (Asymmetric Adhesion Swell & Offset)】
        // -------------------------------------------------------------
        if (pressure > 0.0f) {
            float dX_left = mx - (-CUBE_W);
            float dX_right = CUBE_W - mx;
            float dY_top = my - (-CUBE_H);
            float dY_bottom = CUBE_H - my;
            
            float threshold = 15.0f;
            float rx_offset = 0.0f;
            float ry_offset = 0.0f;
            
            if (dX_left < threshold) {
                // 受左侧挤压，中心朝右偏移
                rx_offset += (1.0f - dX_left / threshold) * nodeRadius * 0.45f;
            }
            if (dX_right < threshold) {
                // 受右侧挤压，中心朝左偏移
                rx_offset -= (1.0f - dX_right / threshold) * nodeRadius * 0.45f;
            }
            if (dY_top < threshold) {
                // 受顶侧挤压，中心朝下偏移
                ry_offset += (1.0f - dY_top / threshold) * nodeRadius * 0.45f;
            }
            if (dY_bottom < threshold) {
                // 受底侧挤压，中心朝上偏移
                ry_offset -= (1.0f - dY_bottom / threshold) * nodeRadius * 0.45f;
            }
            
            render_mx += rx_offset;
            render_my += ry_offset;
            render_radius = nodeRadius * (1.0f + 0.38f * pressure);
        }

        float px, py, pz;
        container->projectPoint(render_mx, render_my, render_mz, container->currentFace, px, py, pz, ax, ay);
        if (pz > 150.0f) return;
        
        float perspective = 200.0f / (200.0f + pz);
        
        // 【物理对齐修复】获取投影后的 2D 速度矢量
        float px2, py2, pz2;
        container->projectPoint(render_mx + vx * 0.1f, render_my + vy * 0.1f, render_mz + vz * 0.1f, container->currentFace, px2, py2, pz2, ax, ay);
        float v2dx = px2 - px;
        float v2dy = py2 - py;
        float vMag = sqrtf(v2dx*v2dx + v2dy*v2dy) * 10.0f;
        
        // 计算拉伸长度 (Trail Length)
        float trailScale = isStartled ? fmaxf(4.0f, fminf(15.0f, 1.0f + 0.6f * vMag)) : fminf(3.0f, 1.0f + 0.1f * vMag);
        // 动态瘦身：受惊时稍微变细以增加速度感，但保持基础厚度
        float thinning = 1.0f / sqrtf(1.0f + vMag * (isStartled ? 0.25f : 0.12f));
        if (isStartled) thinning = fmaxf(0.75f, thinning); // 确保即便高速下也不要细过 0.75
        
        // 引入呼吸起伏 (Breathing effect)
        float r = (render_radius * VISUAL_RADIUS_MULT + VISUAL_RADIUS_OFFSET) * vState.body_scale * thinning * nodeBreathing * powf(perspective, 0.8f) / FIELD_SCALE;

        // 张力引起的各向异性形变比例 (Tension-induced anisotropy)
        // 呼吸胀大时如果是强吸气 (nodeBreathing > 1.0)，自动拉伸，体现“张力拉伸”
        float breathTensionFactor = fmaxf(0.0f, nodeBreathing - 1.0f) * 0.8f;
        float finalTension = vState.surface_tension + breathTensionFactor;
        
        float tStretch = 1.0f + 0.35f * finalTension;
        float nSqueeze = 1.0f + 0.15f * finalTension;

        float nx1 = px / FIELD_SCALE, ny1 = py / FIELD_SCALE;
        float nx0 = (px - v2dx * trailScale * 0.5f) / FIELD_SCALE;
        float ny0 = (py - v2dy * trailScale * 0.5f) / FIELD_SCALE;

        // 计算包围盒：包含整个线段及其半径（贴墙挤压拉宽时自适应扩张包围盒边界）
        float r_bound = r;
        if (pressure > 0.0f) {
            r_bound *= SQUEEZE_TANGENT_EXPAND;
        }
        int rx_min = (int)fminf(nx0, nx1) - (int)r_bound - 4;
        int rx_max = (int)fmaxf(nx0, nx1) + (int)r_bound + 4;
        int ry_min = (int)fminf(ny0, ny1) - (int)r_bound - 4;
        int ry_max = (int)fmaxf(ny0, ny1) + (int)r_bound + 4;

        float line_dx = nx1 - nx0;
        float line_dy = ny1 - ny0;
        float l2 = line_dx * line_dx + line_dy * line_dy;

        for (int y = fmaxf(0, ry_min); y < fminf(FIELD_H, ry_max); y++) {
            for (int x = fmaxf(0, rx_min); x < fminf(FIELD_W, rx_max); x++) {
                // 计算点到线段 (Capsule) 的距离
                float dist_px;
                float proj_x, proj_y;
                if (l2 < 0.001f) {
                    proj_x = nx1;
                    proj_y = ny1;
                } else {
                    float t = fmaxf(0, fminf(1, ((x - nx0) * line_dx + (y - ny0) * line_dy) / l2));
                    proj_x = nx0 + t * line_dx;
                    proj_y = ny0 + t * line_dy;
                }
                
                float dx = x - proj_x;
                float dy = y - proj_y;
                
                float dist_motion = 0.0f;
                if (l2 > 0.001f) {
                    float len = sqrtf(l2);
                    float tx = line_dx / len;
                    float ty = line_dy / len;
                    float nx_dir = -ty;
                    float ny_dir = tx;
                    
                    float dotT = dx * tx + dy * ty;
                    float dotN = dx * nx_dir + dy * ny_dir;
                    
                    float scaledT = dotT / tStretch;
                    float scaledN = dotN * nSqueeze;
                    dist_motion = sqrtf(scaledT * scaledT + scaledN * scaledN);
                } else {
                    // 如果 l2 太小，以重力方向作为虚拟主轴进行微弱拉伸以凸显静态重力张力
                    float gMag = sqrtf(ax*ax + ay*ay);
                    if (gMag > 0.01f) {
                        float tx = ax / gMag;
                        float ty = ay / gMag;
                        float nx_dir = -ty;
                        float ny_dir = tx;
                        
                        float dotT = dx * tx + dy * ty;
                        float dotN = dx * nx_dir + dy * ny_dir;
                        
                        float scaledT = dotT / tStretch;
                        float scaledN = dotN * nSqueeze;
                        dist_motion = sqrtf(scaledT * scaledT + scaledN * scaledN);
                    } else {
                        dist_motion = sqrtf(dx*dx + dy*dy);
                    }
                }

                // 混合运动拉伸与贴墙挤压变形
                if (pressure > 0.0f) {
                    float distL = nx1;
                    float distR = (float)FIELD_W - nx1;
                    float distT = ny1;
                    float distB = (float)FIELD_H - ny1;
                    float minDist = fminf(fminf(distL, distR), fminf(distT, distB));
                    float wnx = 0.0f, wny = 0.0f;
                    if (minDist == distL) { wnx = 1.0f; wny = 0.0f; }
                    else if (minDist == distR) { wnx = -1.0f; wny = 0.0f; }
                    else if (minDist == distT) { wnx = 0.0f; wny = 1.0f; }
                    else { wnx = 0.0f; wny = -1.0f; }
                    
                    float wtx = -wny;
                    float wty = wnx;
                    
                    float dotW_N = dx * wnx + dy * wny;
                    float dotW_T = dx * wtx + dy * wty;
                    
                    float squeezeN;
                    if (dotW_N < 0.0f) {
                        // 贴墙侧：应用各向异性挤压拍扁，使边界紧贴墙面
                        squeezeN = 1.0f - pressure * (1.0f - SQUEEZE_NORMAL_DECAY);
                    } else {
                        // 远离墙壁侧：不予拍扁，并允许 18% 的物理下垂拉伸以抵消飞碟畸变，保持水滴饱满感
                        squeezeN = 1.0f + pressure * 0.18f; 
                    }
                    float squeezeT = 1.0f + pressure * (SQUEEZE_TANGENT_EXPAND - 1.0f);
                    
                    float scaledW_N = dotW_N / squeezeN;
                    float scaledW_T = dotW_T / squeezeT;
                    float dist_wall = sqrtf(scaledW_N * scaledW_N + scaledW_T * scaledW_T);
                    
                    dist_px = (1.0f - pressure) * dist_motion + pressure * dist_wall;
                } else {
                    dist_px = dist_motion;
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
    auto applyHeadArchBump = [&](float& x, float& y, float& z, int nodeIdx) {
        if (nodeIdx > 2) return;
        float dX = CUBE_W - abs(x);
        float dY = CUBE_H - abs(y);
        float dZ = (z > 0) ? (CUBE_D - z) : (z + CUBE_D);
        float minDist = fminf(dX, fminf(dY, dZ));
        if (minDist < 12.0f) {
            float flatFactor = (1.0f - minDist / 12.0f);
            float strength = flatFactor * 4.6f * (1.0f - (float)nodeIdx * 0.4f);
            if (dY == minDist) {
                if (y > 0) y -= strength;
                else y += strength;
            } else if (dX == minDist) {
                if (x > 0) x -= strength;
                else x += strength;
            } else if (dZ == minDist) {
                if (z > 0) z -= strength * 0.8f;
                else z += strength * 0.8f;
            }
        }
    };

    for (int i = 0; i < MAX_NODES; i++) {
        const Node& curr = skeleton.getNode(i);
        float cx = curr.x, cy = curr.y, cz = curr.z;
        applyHeadArchBump(cx, cy, cz, i);
        float nodeBreathing = calcNodeBreathing((float)i);
        drawBlob(cx, cy, cz, curr.radius, curr.vx, curr.vy, curr.vz, nodeBreathing, i);

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
                
                // 为插值点计算过渡的头部耸起
                float interpNodeIdx = (float)(i-1) + t;
                applyHeadArchBump(mx, my, mz, (interpNodeIdx < 1.0f) ? 0 : 1);
                
                float interpBreathing = calcNodeBreathing(interpNodeIdx);
                drawBlob(mx, my, mz, interpolatedRadius, interpolatedVx, interpolatedVy, interpolatedVz, interpBreathing, (int)interpNodeIdx);
            }
        }
    }
 
    // --- 1.25. [新增] 绘制重力漏砂解离滴粒 (Gravity Drips Rendering) ---
    for (int i = 0; i < MAX_DRIPS; i++) {
        if (drips[i].active) {
            float dripBreathing = calcNodeBreathing(0.0f);
            drawBlob(drips[i].x, drips[i].y, drips[i].z, drips[i].radius, drips[i].vx, drips[i].vy, drips[i].vz, dripBreathing, 0);
        }
    }

    // --- 1.3. [新增] 绘制眼眶隆起场以随眼球转动而动态凸起包裹 (Orbital Bulge Rendering) ---
    if (currentSkillName != "sleep") {
        // 直接用骨骼头节点 (Node 0) 的屏幕投影坐标作为基准
        const Node& headNode = skeleton.getNode(0);
        float hx, hy, hz;
        container->projectToFace(headNode, container->currentFace, hx, hy, hz, ax, ay);

        // 计算与 drawBlob 相同尺度和透视感下的 headVisualRadius
        float perspective = 200.0f / (200.0f + hz);
        float vMag = sqrtf(headNode.vx*headNode.vx + headNode.vy*headNode.vy) * 10.0f;
        float thinning = 1.0f / sqrtf(1.0f + vMag * (isStartled ? 0.25f : 0.12f));
        if (isStartled) thinning = fmaxf(0.75f, thinning);
        float headBreathing = calcNodeBreathing(0.0f);
        float r_head = (headNode.radius * VISUAL_RADIUS_MULT + VISUAL_RADIUS_OFFSET) * vState.body_scale * thinning * headBreathing * powf(perspective, 0.8f);

        // 贴屏因子：越靠近屏幕（hz越小），clingFactor越接近 1.0f
        float clingFactor = fmaxf(0.0f, fminf(1.0f, (6.0f - hz) / 6.0f));

        // 眼睛的 2D 投影中心 (与 drawEye 的偏移算法完全对齐)
        float ex = hx + pupilX * 8.0f;
        float ey = hy + pupilY * 6.0f;

        if (clingFactor > 0.0f) {
            float peekEyeX = hx + pupilX * 4.0f;
            float peekEyeY = hy - r_head * 0.72f + pupilY * 2.0f;
            ex = (1.0f - clingFactor) * ex + clingFactor * peekEyeX;
            ey = (1.0f - clingFactor) * ey + clingFactor * (hy - r_head * 0.72f * headLowerProgress + pupilY * (6.0f - 4.0f * headLowerProgress));
        }

        // 眼睛眼眶隆起的半径：正常为头部半径 of 75%，确保有充足的身体黑色组织包裹眼白
        float er = r_head * 0.75f;
        // 与 drawEye 中的背面隐藏条件完全对齐：当贴屏且未主动低头时，眼睛隐藏在背面，则不绘制眼眶隆起
        if (clingFactor > 0.8f && headLowerProgress < 0.15f) {
            er = 0.0f;
        }

        if (er > 0.5f) {
            float ex_grid = ex / FIELD_SCALE;
            float ey_grid = ey / FIELD_SCALE;
            float er_grid = er / FIELD_SCALE;

            int rx_min = (int)(ex_grid - er_grid) - 2;
            int rx_max = (int)(ex_grid + er_grid) + 2;
            int ry_min = (int)(ey_grid - er_grid) - 2;
            int ry_max = (int)(ey_grid + er_grid) + 2;

            for (int y = fmaxf(0, ry_min); y < fminf(FIELD_H, ry_max); y++) {
                for (int x = fmaxf(0, rx_min); x < fminf(FIELD_W, rx_max); x++) {
                    float dx = x - ex_grid;
                    float dy = y - ey_grid;
                    float dist_px = sqrtf(dx*dx + dy*dy);
                    float dist_ratio = dist_px / er_grid;
                    if (dist_ratio < 1.0f) {
                        float inv_ratio = 1.0f - dist_ratio * dist_ratio;
                        float val = inv_ratio * inv_ratio * 195.0f; // 稍低于主体的 210.0f 场强以保持圆润和过渡平滑
                        field[y * FIELD_W + x] = fminf(255, field[y * FIELD_W + x] + (int)val);
                    }
                }
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
    container->projectToFace(head, container->currentFace, hpx, hpy, hpz, ax, ay);
    float hnx = hpx / FIELD_SCALE, hny = hpy / FIELD_SCALE;

    for (int hand = 0; hand < 2; hand++) {
        float prog = (hand == 0) ? handProgressL : handProgressR;
        if (prog >= 1.0f && hand != activeHand) continue;

        float hx = (hand == 0) ? handLX : handRX;
        float hy = (hand == 0) ? handLY : handRY;
        float hz = (hand == 0) ? handLZ : handRZ;

        // 计算手部距头部的 3D 距离，如果已重吸收贴近，则彻底隐藏触手和手指
        float armDX = hx - head.x;
        float armDY = hy - head.y;
        float armDZ = hz - head.z;
        float armDist = sqrtf(armDX*armDX + armDY*armDY + armDZ*armDZ);
        if (armDist < 2.0f) continue;

        float tx, ty, tz;
        container->projectPoint(hx, hy, hz, container->currentFace, tx, ty, tz, ax, ay);
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
            drawTendrils(container, endX, endY, prog, hand, ax, ay);
        }
    }

    // --- 3. 尾部装饰胡须 (Decorative whiskers) ---
    for (int i = 2; i < MAX_NODES; i += 2) {
        const Node& n = skeleton.getNode(i);
        float px, py, pz;
        container->projectToFace(n, container->currentFace, px, py, pz, ax, ay);
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
                    // 显著提升符号的势能贡献，并注入核心偏置防止低于身体阈值(45)导致空心圈噪点
                    float val = (0.5f + 0.5f * cosf(dist_ratio * 3.14159f)) * 270.0f * sp.life * (1.0f + (dx*gx + dy*gy)/r * 0.5f);
                    if (dist_ratio < 0.85f) {
                        val = fmaxf(val, 55.0f * sp.life);
                    }
                    field[y * FIELD_W + x] = fminf(255, field[y * FIELD_W + x] + (int)val);
                }
            }
        }
    }
}

void Venom::drawTendrils(Container* container, float endX, float endY, float prog, int handIdx, float ax, float ay) {
    // 【生物级自适应爪形系统】实现 5 根非对称、随机长度、具有壁面吸附折弯与手臂延伸对齐的手指
    const int numFingers = 5;
    bool isGripping = (prog > 0.9f);
    
    // -------------------------------------------------------------
    // 【自适应基准角度解算 (对齐手臂延伸与吸附壁面)】
    // -------------------------------------------------------------
    // 获取手臂从头 (hnx, hny) 到手心 (endX, endY) 的延伸方向
    const Node& head = skeleton.getNode(0);
    float hpx, hpy, hpz;
    container->projectToFace(head, container->currentFace, hpx, hpy, hpz, ax, ay);
    float hnx = hpx / FIELD_SCALE, hny = hpy / FIELD_SCALE;
    
    // 计算手臂延伸矢量角度，作为手指在空中探寻游动时的朝向
    float armAngle = atan2f(endY - hny, endX - hnx);
    float baseAngle = armAngle; 
    
    // 如果手掌进入吸附墙壁状态，自适应转动爪部展开面朝向，使其面朝对应侧壁完全摊开！
    if (isGripping) {
        float distX_left = endX;
        float distX_right = (float)FIELD_W - endX;
        float distY_top = endY;
        float distY_bottom = (float)FIELD_H - endY;
        float minDist = fminf(distX_left, fminf(distX_right, fminf(distY_top, distY_bottom)));
        
        if (minDist < 6.0f) {
            if (minDist == distX_left) {
                baseAngle = 3.14159f; // 贴附左壁：朝左张开
            } else if (minDist == distX_right) {
                baseAngle = 0.0f; // 贴附右壁：朝右张开
            } else if (minDist == distY_top) {
                baseAngle = -3.14159f / 2.0f; // 贴附顶壁：朝上张开
            } else {
                baseAngle = 3.14159f / 2.0f; // 贴附底壁：朝下张开
            }
        }
    }
    
    // -------------------------------------------------------------
    // 【物理重构：贴壁抓附手指完全静止机制】
    // -------------------------------------------------------------
    // 如果手掌已经粘住壁面（isGripping），手指必须百分之百稳固地吸在墙上动弹不得。
    // 只有在空中游动没有抓墙时，才允许手指有呼吸般的正弦波晃动。
    if (!isGripping) {
        baseAngle += sinf(millis() * 0.004f) * 0.15f;
    }
    
    for (int f = 0; f < numFingers; f++) {
        // 贴壁时扇形角度绝对恒定，空中时添加 sin 微弱起伏
        float relativeAng = (f - 2) * 0.45f;
        if (!isGripping) {
            relativeAng += sinf(f * 1.5f + millis() * 0.001f) * 0.1f; 
        }
        float fAng = baseAngle + relativeAng;

        // 随机且动态的长度 (抓附时指骨伸展紧绷且完全静止，空中时灵动呼吸收缩与微颤)
        float fLenBase = isGripping ? 4.75f : (2.75f + sinf(millis() * 0.008f + f) * 1.0f);
        float fLenVar = isGripping ? 0.0f : (float)((millis() + f * 100) % 27) * 0.05f; 
        float fLen = fmaxf(2.0f, (fLenBase + fLenVar) * (0.7f + vState.edge_activity * 0.5f));

        for (float fl = 0; fl < fLen; fl += 0.8f) {
            float t = fl / fLen;
            float fx = endX + cosf(fAng) * fl;
            float fy = endY + sinf(fAng) * fl;
            
            // -------------------------------------------------------------
            // 【几何碰壁物理折弯平贴算法】
            // -------------------------------------------------------------
            // 如果手指尖在生长延伸中碰到了玻璃四壁边缘，我们将溢出的长度(overLen)物理偏折并沿该壁面切线抹平，重现完美吸附贴紧！
            if (isGripping) {
                float margin = 1.0f;
                // 左壁碰壁：限制 fx，并将溢出长度向 y 轴折弯
                if (fx < margin) {
                    float overLen = margin - fx;
                    fx = margin;
                    fy += (sinf(fAng) > 0.0f ? 1.0f : -1.0f) * overLen;
                }
                // 右壁碰壁
                else if (fx > (float)FIELD_W - margin) {
                    float overLen = fx - ((float)FIELD_W - margin);
                    fx = (float)FIELD_W - margin;
                    fy += (sinf(fAng) > 0.0f ? 1.0f : -1.0f) * overLen;
                }
                
                // 顶壁碰壁
                if (fy < margin) {
                    float overLen = margin - fy;
                    fy = margin;
                    fx += (cosf(fAng) > 0.0f ? 1.0f : -1.0f) * overLen;
                }
                // 底壁碰壁
                else if (fy > (float)FIELD_H - margin) {
                    float overLen = fy - ((float)FIELD_H - margin);
                    fy = (float)FIELD_H - margin;
                    fx += (cosf(fAng) > 0.0f ? 1.0f : -1.0f) * overLen;
                }
            }
            
            // 边界剪切强保护：确保折弯后也绝不超出 field 缓冲区
            fx = fmaxf(0.0f, fminf((float)FIELD_W - 1.0f, fx));
            fy = fmaxf(0.0f, fminf((float)FIELD_H - 1.0f, fy));

            // -------------------------------------------------------------
            // 【树蛙吸盘渲染模型 (Frog-like Suction Pads v6.5)】
            // -------------------------------------------------------------
            // 指骨前半段 (t < 0.7f) 自然渐细，后半段 (t >= 0.7f) 迅速平滑膨大，形成圆润可爱的吸盘小球！
            float fr;
            if (t < 0.7f) {
                fr = 1.3f * (1.0f - t * 0.45f); // 渐细指骨 (从 1.3f 缩减到 0.7f)
            } else {
                float padMax = isGripping ? 2.3f : 1.7f; // 吸附态下吸盘会因为 Metaball 压强瞬间拍扁变宽
                float padT = (t - 0.7f) / 0.3f; // 归一化吸盘段进度 (0.0 到 1.0)
                fr = 0.7f + padT * (padMax - 0.7f); // 平滑膨大到 padMax 半径
            }
            
            int ir = (int)fmaxf(1, fr);
            
            for (int dy = -ir; dy <= ir; dy++) {
                for (int dx = -ir; dx <= ir; dx++) {
                    int ifx = (int)fx + dx, ify = (int)fy + dy;
                    if (ifx >= 0 && ifx < FIELD_W && ify >= 0 && ify < FIELD_H) {
                        float dist = sqrtf(dx * dx + dy * dy);
                        if (dist <= fr) {
                            float opacity;
                            if (t < 0.7f) {
                                opacity = (1.0f - dist / (fr + 0.1f)) * (1.0f - t * 0.3f);
                            } else {
                                // 吸盘膨大部分保持高饱和度的实心度（不随 t 衰减），确保渲染出绝对饱满可爱的圆形圆轮
                                opacity = (1.0f - dist / (fr + 0.1f)) * 0.95f;
                            }
                            field[ify * FIELD_W + ifx] = fminf(255, field[ify * FIELD_W + ifx] + (int)(190 * opacity));
                        }
                    }
                }
            }
        }
    }
}

void Venom::draw(M5Canvas* canvas, Container* container, float ax, float ay, float az) {
    float cax = fmaxf(-0.85f, fminf(0.85f, ax * 0.45f));
    float cay = fmaxf(-0.85f, fminf(0.85f, ay * 0.45f));

    container->currentFace = FRONT;

    calculateField(container, cax, cay);
    float px = cay * 0.1f, py = -cax * 0.1f;
    container->drawContainer(canvas, cax, cay, skeleton);
    drawBackground(canvas, px * 0.5f, py * 0.5f);
    drawBody(canvas, px, py, cax, cay);
    drawGloss(canvas, px * 1.1f, py * 1.1f, cax, cay);
    drawEye(canvas, container, px * 1.1f, py * 1.1f, cax, cay);

    if (showDebug) drawDebug(canvas);
}



void Venom::drawBackground(M5Canvas* canvas, float px, float py) {
    uint32_t t = millis() / 80;
    for (int y = 0; y < FIELD_H; y++) {
        for (int x = 0; x < FIELD_W; x++) {
            int val = field[y * FIELD_W + x];
            if (val > 38) {
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

void Venom::drawEye(M5Canvas* canvas, Container* container, float px, float py, float ax, float ay) {
    if (currentSkillName != "sleep" && isBlinking && millis() - lastBlinkTime < 120) return;

    // 对头节点应用与渲染身体时完全一致的 applyHeadArchBump 和贴墙偏移，确保眼睛与身体位置像素级重合
    const Node& head = skeleton.getNode(0);
    float h_x = head.x;
    float h_y = head.y;
    float h_z = head.z;

    // 1. 头部耸起偏移 (与 calculateField 一致)
    float dX = CUBE_W - abs(h_x);
    float dY = CUBE_H - abs(h_y);
    float dZ = (h_z > 0) ? (CUBE_D - h_z) : (h_z + CUBE_D);
    float minDistBump = fminf(dX, fminf(dY, dZ));
    if (minDistBump < 12.0f) {
        float flatFactor = (1.0f - minDistBump / 12.0f);
        float strength = flatFactor * 4.6f;
        if (dY == minDistBump) {
            if (h_y > 0) h_y -= strength;
            else h_y += strength;
        } else if (dX == minDistBump) {
            if (h_x > 0) h_x -= strength;
            else h_x += strength;
        } else if (dZ == minDistBump) {
            if (h_z > 0) h_z -= strength * 0.8f;
            else h_z += strength * 0.8f;
        }
    }

    // 2. 贴墙膨胀偏移 (与 drawBlob 一致)
    float pressure = head.contactPressure;
    if (pressure > 0.0f) {
        float dX_left = h_x - (-CUBE_W);
        float dX_right = CUBE_W - h_x;
        float dY_top = h_y - (-CUBE_H);
        float dY_bottom = CUBE_H - h_y;
        float threshold = 15.0f;
        float rx_offset = 0.0f;
        float ry_offset = 0.0f;
        if (dX_left < threshold) rx_offset += (1.0f - dX_left / threshold) * head.radius * 0.45f;
        if (dX_right < threshold) rx_offset -= (1.0f - dX_right / threshold) * head.radius * 0.45f;
        if (dY_top < threshold) ry_offset += (1.0f - dY_top / threshold) * head.radius * 0.45f;
        if (dY_bottom < threshold) ry_offset -= (1.0f - dY_bottom / threshold) * head.radius * 0.45f;
        
        h_x += rx_offset;
        h_y += ry_offset;
    }

    float hx, hy, hz;
    container->projectPoint(h_x, h_y, h_z, container->currentFace, hx, hy, hz, ax, ay);

    if (currentSkillName == "sleep") {
        // [修复] 锁定眼部位置在头部中心，完全忽略 pupilX/Y 的偏移
        int ix = (int)hx, iy = (int)hy;
        int sw = 10 * vState.body_scale;
        // 绘制可爱的向下弯曲弧线 (眯眯眼)
        canvas->drawArc(ix, iy - 2, sw, sw, 30, 150, TFT_DARKGREY);
        canvas->drawArc(ix, iy - 1, sw-1, sw-1, 35, 145, 0x3186); 
        return;
    }

    // 计算与 drawBlob 相同尺度和透视感下的 headVisualRadius
    float breathRange = BREATH_RANGE_BASE + stress * 0.15f;
    if (millis() - lastStartledTime < 120000) breathRange *= 0.5f;
    if (observer_trust > 0.6f) breathRange *= 1.3f;
    float headBreathing = 1.0f + sinf(breathingPhase) * breathRange;

    bool isStartled = (currentSkillName == "startled");
    float perspective = 200.0f / (200.0f + hz);
    float vMag = sqrtf(head.vx*head.vx + head.vy*head.vy) * 10.0f;
    float thinning = 1.0f / sqrtf(1.0f + vMag * (isStartled ? 0.25f : 0.12f));
    if (isStartled) thinning = fmaxf(0.75f, thinning);
    float r_head = (head.radius * VISUAL_RADIUS_MULT + VISUAL_RADIUS_OFFSET) * vState.body_scale * thinning * headBreathing * powf(perspective, 0.8f);

    // 贴屏因子：越靠近屏幕（hz越小），clingFactor越接近 1.0f
    float clingFactor = fmaxf(0.0f, fminf(1.0f, (6.0f - hz) / 6.0f));

    // 如果贴屏且没有主动低头窥视，眼睛完全隐藏在背面
    if (clingFactor > 0.8f && headLowerProgress < 0.15f) {
        return;
    }

    // 眼睛位置解算
    float normalEyeX = hx + pupilX * 8.0f;
    float normalEyeY = hy + pupilY * 6.0f;
    float eyeX = normalEyeX;
    float eyeY = normalEyeY;

    if (clingFactor > 0.0f) {
        // 自适应最近墙面法线的低头窥视偏移
        float peekOffsetX = 0.0f;
        float peekOffsetY = 0.0f;
        float dX_left = h_x - (-CUBE_W);
        float dX_right = CUBE_W - h_x;
        float dY_top = h_y - (-CUBE_H);
        float dY_bottom = CUBE_H - h_y;
        float minDistWall = fminf(fminf(dX_left, dX_right), fminf(dY_top, dY_bottom));
        
        if (minDistWall == dX_left) {
            peekOffsetX = r_head * 0.65f; // 靠近左墙，往右滑动
        } else if (minDistWall == dX_right) {
            peekOffsetX = -r_head * 0.65f; // 靠近右墙，往左滑动
        } else if (minDistWall == dY_top) {
            peekOffsetY = r_head * 0.65f; // 靠近顶墙，往下滑动
        } else {
            peekOffsetY = -r_head * 0.65f; // 靠近底墙，往上滑动
        }

        float peekEyeX = hx + peekOffsetX * headLowerProgress + pupilX * (8.0f - 4.0f * headLowerProgress);
        float peekEyeY = hy + peekOffsetY * headLowerProgress + pupilY * (6.0f - 4.0f * headLowerProgress);
        
        eyeX = (1.0f - clingFactor) * normalEyeX + clingFactor * peekEyeX;
        eyeY = (1.0f - clingFactor) * normalEyeY + clingFactor * peekEyeY;
    }

    // 眼睛形变尺寸
    float eyeW = 14.0f;
    float eyeH = 10.0f;
    
    if (clingFactor > 0.0f) {
        float maxPeekH = 6.5f;
        float minPeekH = 2.0f;
        float peekH = minPeekH + (maxPeekH - minPeekH) * headLowerProgress;
        
        eyeH = (1.0f - clingFactor) * 10.0f + clingFactor * peekH;
        eyeW = (1.0f - clingFactor) * 14.0f + clingFactor * (10.0f + 4.0f * headLowerProgress);
    }

    // 纯白色眼白
    canvas->fillEllipse((int)eyeX, (int)eyeY, (int)eyeW, (int)eyeH, COLOR_EYE_WHITE);
    canvas->drawEllipse((int)eyeX, (int)eyeY, (int)eyeW, (int)eyeH, COLOR_VENOM_BODY);

    // 瞳孔大小与偏移
    float pSize = 3.5f * pupilSize;
    if (clingFactor > 0.0f) {
        pSize = pSize * (0.4f + 0.6f * headLowerProgress);
    }
    if (pSize < 1.0f) pSize = 1.0f;

    float pOffsetX = pupilX * 3.5f;
    float pOffsetY = pupilY * 2.5f;
    if (clingFactor > 0.0f) {
        pOffsetX = pupilX * (1.5f + 2.0f * headLowerProgress);
        pOffsetY = pupilY * (1.0f + 1.5f * headLowerProgress);
    }
    canvas->fillCircle((int)(eyeX + pOffsetX),
                       (int)(eyeY + pOffsetY),
                       (int)pSize, COLOR_VENOM_BODY);

    // 高压反光点
    canvas->drawPixel((int)(eyeX - eyeW * 0.28f), (int)(eyeY - eyeH * 0.3f), 0xFFFF);

    // 紧张状态：极轻微血丝 (仅在未贴屏或贴屏但完全低头展开时绘制，以保证高对比度)
    if (stress > 0.72f && (clingFactor < 0.8f || headLowerProgress > 0.5f)) { 
        int dotCount = (int)((stress - 0.72f) * 15.0f) + 2;
        for (int i = 0; i < dotCount; i++) {
            float ang = (float)random(360) * 0.0174f;
            float dist = (eyeW * 0.5f) + (float)random(3);
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

    // --- [彻底重构] 毒液生命感意识神经核心 (Procedural Neural Core) ---
    {
        int ux = x2 + 55, uy = ry + 25; // 居右下方定位
        neuralCore.draw(canvas, ux, uy);
        
        // 标注神经核心 (通过它的 procedual 状态行为可以直接识别意识状态)
        drawGlowText("NEURAL_CORE", ux - 32, uy + 13, 0x4208);
    }

    // --- [新增意识层 V3] 意识泄漏 HUD 覆盖劫持逻辑 (Cognitive Leak Hijack Overlay) ---
    if (isConsciousnessLeak && !lState_notes.isEmpty()) {
        uint32_t age = millis() - leakStartTime;
        if (age < leakDuration) {
            // 在 HUD 面板内进行高亮覆盖
            int lx = bx + 4;
            int ly = by + 4;
            int lw = bw - 8;
            int lh = bh - 8;
            
            // 绘制深色极客质感底座
            canvas->fillRect(lx, ly, lw, lh, canvas->color565(6, 12, 10));
            canvas->drawRect(lx, ly, lw, lh, canvas->color565(40, 180, 160)); // 青色发光边框
            
            // 绘制闪烁的红色警告头
            uint16_t warnColor = (millis() / 200) % 2 == 0 ? 0xF800 : 0x059d;
            canvas->setTextColor(warnColor);
            canvas->setCursor(lx + 8, ly + 8);
            canvas->print("WARNING: COGNITIVE LEAK DETECTED!");
            
            // 解码器状态标题
            canvas->setTextColor(0x028A); // 暗绿色 glow
            canvas->setCursor(lx + 8 + 1, ly + 20 + 1);
            canvas->print("SYMBIOSE RAW THOUGHTS DECODED:");
            canvas->setTextColor(0x059d); // 青色 text
            canvas->setCursor(lx + 8, ly + 20);
            canvas->print("SYMBIOSE RAW THOUGHTS DECODED:");
            
            // 动态打字机效果
            int charsToShow = age / 30; // 30ms 逐字显现
            int len = lState_notes.length();
            if (charsToShow > len) charsToShow = len;
            String visibleText = lState_notes.substring(0, charsToShow);
            if (charsToShow < len || (millis() / 250) % 2 == 0) {
                visibleText += "_"; // 光标闪烁
            }
            
            canvas->setTextColor(canvas->color565(200, 240, 230)); // 柔和的亮色文字
            canvas->setCursor(lx + 8, ly + 36);
            canvas->print(visibleText.c_str());
        } else {
            isConsciousnessLeak = false; // 自动关闭
        }
    }

    // --- 4. 动态装饰: 扫描线 ---
    static float scanY = 0;
    scanY += 0.8f;
    if (scanY > bh) scanY = 0;
    canvas->drawFastHLine(bx + 2, by + (int)scanY, bw - 4, 0x18C3); // 极淡的扫描线
}

void Venom::drawBox(M5Canvas* canvas, Container* container, float ax, float ay) {
    // 绘制 3D 盒子的线框以增强空间感
    static const float corners[8][3] = {
        {-CUBE_W, -CUBE_H, -CUBE_D}, {CUBE_W, -CUBE_H, -CUBE_D},
        {CUBE_W, CUBE_H, -CUBE_D}, {-CUBE_W, CUBE_H, -CUBE_D},
        {-CUBE_W, -CUBE_H, CUBE_D}, {CUBE_W, -CUBE_H, CUBE_D},
        {CUBE_W, CUBE_H, CUBE_D}, {-CUBE_W, CUBE_H, CUBE_D}
    };
    
    float px[8], py[8], pz[8];
    for (int i = 0; i < 8; i++) {
        container->projectPoint(corners[i][0], corners[i][1], corners[i][2], container->currentFace, px[i], py[i], pz[i], ax, ay);
    }
    
    uint16_t boxColor = 0x4208; // 暗灰色线框
    // 绘制 12 条棱 (px/py 已经是绝对屏幕坐标)
    for (int i = 0; i < 4; i++) {
        canvas->drawLine(px[i], py[i], px[(i+1)%4], py[(i+1)%4], boxColor);
        canvas->drawLine(px[i+4], py[i+4], px[((i+1)%4)+4], py[((i+1)%4)+4], boxColor);
        canvas->drawLine(px[i], py[i], px[i+4], py[i+4], boxColor);
    }
}
