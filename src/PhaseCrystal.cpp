#include "PhaseCrystal.h"
#include <math.h>

// ============================================================================
// 辅助：填充四边形（2个三角形）
// ============================================================================
void PhaseCrystal::fillQuad(M5Canvas* c,
    int x0,int y0, int x1,int y1, int x2,int y2, int x3,int y3, uint16_t col) {
    c->fillTriangle(x0,y0, x1,y1, x2,y2, col);
    c->fillTriangle(x0,y0, x2,y2, x3,y3, col);
}
void PhaseCrystal::edgeQuad(M5Canvas* c,
    int x0,int y0, int x1,int y1, int x2,int y2, int x3,int y3, uint16_t col) {
    c->drawLine(x0,y0, x1,y1, col); c->drawLine(x1,y1, x2,y2, col);
    c->drawLine(x2,y2, x3,y3, col); c->drawLine(x3,y3, x0,y0, col);
}

// ============================================================================
// 内部工具：将 3D 点投影到屏幕整数坐标
// ============================================================================
static void proj3D(Container* container, float x, float y, float z,
                   float ax, float ay, int& sx, int& sy) {
    float px, py, pz;
    container->projectPoint(x, y, z, FRONT, px, py, pz, ax, ay);
    sx = (int)px; sy = (int)py;
}

// ============================================================================
// 构造：随机初始化非对称手指布局
// ============================================================================
PhaseCrystal::PhaseCrystal()
    : showDebug(false), currentFPS(60), lastFrameTime(0),
      moveTimer(0), stepTimer(0), animTime(0), ambientLux(0), isMoving(false) {

    // 手部初始 3D 位置（锚定在容器底部，紧贴地面爬行，决不漂浮悬空）
    crawlX = 0; crawlY = CUBE_H - 10.0f; crawlZ = CUBE_D - 0.5f;
    nextCrawlX = crawlX; nextCrawlY = crawlY; nextCrawlZ = crawlZ;

    // 骨骼同步初始化（用于物理定位，且贴近容器底部和前面板）
    for (int i = 0; i < MAX_NODES; i++) {
        skeleton.getNode(i).x = 0;
        skeleton.getNode(i).y = CUBE_H - 10.0f;
        skeleton.getNode(i).z = CUBE_D - 0.5f;
        skeleton.getNode(i).radius = 5.0f;
    }

    // 5~6 根手指，随机非对称
    numFingers = 5 + (random(0,100) < 45 ? 1 : 0);
    float spread = 55.0f; // 横向展开（3D 容器单位）
    float step   = spread / (numFingers - 1);
    for (int f = 0; f < numFingers; f++) {
        fingers[f].perpOffset  = -spread * 0.5f + step * f + (float)random(-4,5);
        fingers[f].lengthScale = 0.70f + (float)random(0,65)*0.01f;
        fingers[f].liftAmount  = 0.0f;
        fingers[f].wigglePhase = (float)random(0,628)*0.01f;
        fingers[f].stepPhase   = (float)f / numFingers;
        for (int s = 0; s < 3; s++) {
            fingers[f].seg[s].brightness = 0.38f + (float)random(0,50)*0.01f;
            fingers[f].seg[s].slipX = fingers[f].seg[s].slipY = 0;
        }
    }

    // 手掌内部裂纹（12条高密度裂纹）
    for (int c = 0; c < NUM_CRACKS; c++) {
        cracks[c].px  = (float)random(-88,89)*0.01f;
        cracks[c].py  = (float)random(10,90)*0.01f;
        cracks[c].angle = (float)random(0,314)*0.01f;
        cracks[c].len   = 4.0f + (float)random(0,15);
        cracks[c].brightness = 0.35f + (float)random(0,55)*0.01f;
    }

    // 初始化碎裂粒子
    for (int i = 0; i < MAX_SHARDS; i++) {
        shards[i].active = false;
    }
}

// ============================================================================
// 更新：爬行步态
// ============================================================================
void PhaseCrystal::update(float gx, float gy, float gz, float soundLevel, float lux) {
    uint32_t now = millis();
    if (lastFrameTime > 0 && (now - lastFrameTime) > 0)
        currentFPS = currentFPS*0.9f + (1000.0f/(now-lastFrameTime))*0.1f;
    lastFrameTime = now;
    animTime = now;
    ambientLux = lux;

    // 每 7.5 秒选取新目标（在前面板底面范围内游走爬行）
    if (now - moveTimer > 7500) {
        moveTimer = now;
        nextCrawlX = (float)random(-(int)CUBE_W + 30, (int)CUBE_W - 30);
        nextCrawlY = CUBE_H - 10.0f; // 始终锚定在容器底面
        nextCrawlZ = CUBE_D - 0.5f; // 贴近前面板
        isMoving = true;
    }

    // 平滑插值当前位置 → 目标（始终在底部滑动爬行）
    float dCX = nextCrawlX - crawlX;
    float dCY = nextCrawlY - crawlY;
    float dist = sqrtf(dCX*dCX + dCY*dCY);
    if (dist > 2.0f) {
        crawlX += dCX * 0.012f;
        crawlY = CUBE_H - 10.0f; // 始终强制触地，决不漂空
        isMoving = true;
    } else {
        isMoving = false;
    }
    crawlZ = CUBE_D - 0.5f;

    float t = now * 0.001f;

    // --- 晶体粒子系统动力学更新 ---
    for (int i = 0; i < MAX_SHARDS; i++) {
        if (shards[i].active) {
            shards[i].x += shards[i].vx;
            shards[i].y += shards[i].vy;
            shards[i].z += shards[i].vz;
            
            // 重力下落与阻尼
            shards[i].vy += 0.05f; // 微弱向下掉落
            shards[i].vx *= 0.98f;
            shards[i].vz *= 0.98f;

            if (now - shards[i].birth > shards[i].life) {
                shards[i].active = false;
            }
        }
    }

    // --- 运动时粒子喷射（碎裂玻璃渣）与高频裂缝重组 ---
    if (isMoving) {
        // 1. 碎玻璃喷溅
        if (random(100) < 30) {
            for (int i = 0; i < MAX_SHARDS; i++) {
                if (!shards[i].active) {
                    shards[i].active = true;
                    int f = random(numFingers);
                    
                    // 粒子发射源（在手指各指节和手掌区域）
                    shards[i].x = crawlX + fingers[f].perpOffset + (float)random(-6, 7);
                    shards[i].y = crawlY - 10.0f + (float)random(-10, 30); 
                    shards[i].z = crawlZ + fingers[f].liftAmount + (float)random(-2, 3);
                    
                    // 爆裂发射速度
                    shards[i].vx = (float)random(-15, 16) * 0.06f;
                    shards[i].vy = (float)random(-8, 12) * 0.06f;
                    shards[i].vz = (float)random(-15, 10) * 0.06f;
                    
                    shards[i].size = 1.0f + (float)random(0, 3);
                    shards[i].birth = now;
                    shards[i].life = 250 + random(0, 350); // 极速消散闪烁
                    break;
                }
            }
        }

        // 2. 高频玻璃裂纹瞬间重新洗牌重组（碎裂重组质感）
        if (random(100) < 18) {
            int c = random(NUM_CRACKS);
            cracks[c].px = (float)random(-88, 89) * 0.01f;
            cracks[c].py = (float)random(10, 90) * 0.01f;
            cracks[c].angle = (float)random(0, 314) * 0.01f;
            cracks[c].len = 4.0f + (float)random(0, 16);
            cracks[c].brightness = 0.45f + (float)random(0, 55) * 0.01f;
        }
    }

    // 手指动画
    for (int f = 0; f < numFingers; f++) {
        for (int s = 0; s < 3; s++)
            fingers[f].seg[s].brightness =
                0.32f + 0.30f * sinf(t*(0.9f+s*0.15f) + fingers[f].wigglePhase + s*1.1f);

        if (isMoving) {
            float phase = fmodf(t * 2.5f + fingers[f].stepPhase * 6.2832f, 6.2832f);
            float lift  = sinf(phase);
            // 抬起方向：沿 Z 轴抬离前面板（3D 容器单位 5）
            fingers[f].liftAmount = (lift > 0.2f) ? (lift - 0.2f) * 5.0f : 0.0f;

            float errAmp = fminf(dist * 0.03f, 1.5f);
            fingers[f].seg[0].slipX = errAmp * sinf(t*4.0f + f*1.5f);
            fingers[f].seg[0].slipY = errAmp * sinf(t*3.5f + f*1.2f);
            fingers[f].seg[1].slipX = fingers[f].seg[0].slipX * 0.5f;
            fingers[f].seg[2].slipX = fingers[f].seg[0].slipX * 0.2f;
        } else {
            fingers[f].liftAmount  *= 0.82f;
            for (int s = 0; s < 3; s++) {
                fingers[f].seg[s].slipX *= 0.88f;
                fingers[f].seg[s].slipY *= 0.88f;
            }
        }
    }
}

// ============================================================================
// 受惊
// ============================================================================
void PhaseCrystal::setStartled() {
    for (int f = 0; f < numFingers; f++) {
        fingers[f].liftAmount = 4.0f + (float)random(0,4);
        for (int s = 0; s < 3; s++) {
            fingers[f].seg[s].slipX = (float)random(-4,5);
            fingers[f].seg[s].slipY = (float)random(-3,4);
        }
    }
    nextCrawlX = (float)random(-(int)CUBE_W+30, (int)CUBE_W-30);
    nextCrawlY = CUBE_H - 10.0f; // 始终锚定在容器底面
    isMoving = true;
}

// ============================================================================
// 核心渲染：全 3D 坐标系的晶体手
// 所有顶点在 3D 容器坐标系定义，通过 projectPoint 投影，保证与容器同步透视变化
// ============================================================================
void PhaseCrystal::drawCrystalHand(M5Canvas* canvas, Container* container, float ax, float ay) {
    // ---- 3D 深度透视空间布局 ----
    // 爬行底面固定为容器底面 (CUBE_H - 10.0f)，前面板为 (Z = crawlZ)
    // 为实现极致的3D空间深邃感，我们为手部建立渐进式的3D深度：
    // - 手腕深置于容器最深处：wristZ = Z_FACE - 18.0f (具有极强且敏感的旋转视差)
    // - 手掌中部：palmZ = Z_FACE - 10.0f
    // - 手指根部：fBaseZ = palmZ = Z_FACE - 10.0f
    // - 指尖向前扣在前面板玻璃上：fingertipZ = Z_FACE - liftAmount (抬起时深入Z轴，落地时紧贴前面板)
    
    const float PALM_H    = 20.0f;   // 手掌高（腕→掌底，沿 -Y 方向）
    const float WRIST_HW  =  8.0f;   // 手腕半宽（沿 X）
    const float PALM_HW   = 28.0f;   // 掌底半宽（沿 X）
    const float SEG3D_H[3]= {12.0f, 10.0f, 7.5f}; // 三段指节高（沿 +Y）
    const float SEG3D_HW[4]= {4.5f, 3.5f, 2.6f, 1.6f}; // 宽度（逐段收窄）
    
    const float Z_FACE    = crawlZ;  // 前面板基准 Z 坐标 (CUBE_D - 0.5f)
    const float wristZ    = Z_FACE - 18.0f; // 手腕深度（深层视差）
    const float palmZ     = Z_FACE - 10.0f; // 手掌深度（中层视差）

    float t = animTime * 0.001f;

    // 颜色定义
    uint16_t cHL      = canvas->color565(195, 230, 255); // 冰蓝高亮边缘
    uint16_t cDiag    = canvas->color565(75, 140, 210);  // 几何面切线

    // ---- 手掌及手腕 3D 绘制 ----
    float palmCX = crawlX;
    float palmCY = crawlY - PALM_H; // 掌底在手腕上方 (Y 轴负向)

    // 3D 投影手腕和手掌的 4 个角
    int wx0, wy0, wx1, wy1, pbx0, pby0, pbx1, pby1;
    proj3D(container, crawlX - WRIST_HW, crawlY, wristZ, ax, ay, wx0, wy0);
    proj3D(container, crawlX + WRIST_HW, crawlY, wristZ, ax, ay, wx1, wy1);
    proj3D(container, palmCX - PALM_HW,  palmCY, palmZ,  ax, ay, pbx0, pby0);
    proj3D(container, palmCX + PALM_HW,  palmCY, palmZ,  ax, ay, pbx1, pby1);

    // 填充冰蓝色半透明硬质晶体手掌
    float palmBr = 0.32f + 0.12f * sinf(t * 0.7f);
    uint16_t cPalm = canvas->color565((uint8_t)(12*palmBr), (uint8_t)(45*palmBr), (uint8_t)(98*palmBr));
    fillQuad(canvas, wx0,wy0, wx1,wy1, pbx1,pby1, pbx0,pby0, cPalm);
    edgeQuad(canvas, wx0,wy0, wx1,wy1, pbx1,pby1, pbx0,pby0, cHL);

    // ---- [核心视觉重构] 蛛网状硬质玻璃撞击碎裂裂隙 (Webbed Glass Fractures) ----
    // 计算手掌几何中心点（撞击核心源）
    float pCenterX = crawlX;
    float pCenterY = (crawlY + palmCY) * 0.5f;
    float pCenterZ = (wristZ + palmZ) * 0.5f;
    int pcX, pcY;
    proj3D(container, pCenterX, pCenterY, pCenterZ, ax, ay, pcX, pcY);

    // 投影另外两个边缘中点，以构造 6 方向对称的玻璃碎裂辐射轴
    int wMidX, wMidY, pMidX, pMidY;
    proj3D(container, crawlX, crawlY, wristZ, ax, ay, wMidX, wMidY);
    proj3D(container, palmCX, palmCY, palmZ,  ax, ay, pMidX, pMidY);

    // 定义 6 个辐射终端点 (0:左腕, 1:腕中, 2:右腕, 3:右掌, 4:掌中, 5:左掌)
    int radX[6] = { wx0, wMidX, wx1, pbx1, pMidX, pbx0 };
    int radY[6] = { wy0, wMidY, wy1, pby1, pMidY, pby0 };

    // 绘制 6 条由中心辐射出的尖锐、笔直的直线玻璃裂缝
    float flashMult = isMoving ? (1.5f + 0.4f * sinf(t * 80.0f)) : 1.0f;
    for (int i = 0; i < 6; i++) {
        uint8_t br = (uint8_t)fminf(255, 160 * cracks[i % NUM_CRACKS].brightness * flashMult);
        uint16_t cLine = canvas->color565(br/3, (uint8_t)(br * 0.75f), 255);
        canvas->drawLine(pcX, pcY, radX[i], radY[i], cLine);
    }

    // 绘制 concentric rings (同心多边形碎裂环，完美贴合透视)
    // 在辐射线 42% 与 75% 长度处插值得到两层折线同心圆环，展现硬玻璃抗击碎裂的经典视觉
    float ringScales[2] = { 0.42f, 0.75f };
    for (int r = 0; r < 2; r++) {
        float rScale = ringScales[r];
        int ringPtX[6], ringPtY[6];
        for (int i = 0; i < 6; i++) {
            ringPtX[i] = (int)(pcX + (radX[i] - pcX) * rScale);
            ringPtY[i] = (int)(pcY + (radY[i] - pcY) * rScale);
        }
        // 封闭环绘制
        for (int i = 0; i < 6; i++) {
            int nextIdx = (i + 1) % 6;
            uint8_t br = (uint8_t)fminf(255, 140 * cracks[(i+r*3)%NUM_CRACKS].brightness * flashMult);
            uint16_t cRing = canvas->color565(br/3, (uint8_t)(br * 0.75f), 255);
            canvas->drawLine(ringPtX[i], ringPtY[i], ringPtX[nextIdx], ringPtY[nextIdx], cRing);
        }
    }

    // 撞击源核心绘制超高亮度的钻石白聚光裂斑 (Fracture Impact Point)
    uint8_t centerFlash = isMoving ? 255 : (uint8_t)(180 + 40 * sinf(t * 5.0f));
    uint16_t cImpact = canvas->color565(centerFlash, centerFlash, 255);
    canvas->fillCircle(pcX, pcY, isMoving ? 2 : 1, cImpact);
    canvas->drawLine(pcX - 3, pcY, pcX + 3, pcY, cImpact);
    canvas->drawLine(pcX, pcY - 3, pcX, pcY + 3, cImpact);

    // ---- 手指节段绘制（3D 步态，抬起时弯折触地） ----
    for (int f = 0; f < numFingers; f++) {
        // 指根在手掌上
        float fBaseX = palmCX + fingers[f].perpOffset;
        float fBaseY = palmCY;
        float fBaseZ = Z_FACE - 10.0f; // 根部深度固定为手掌深度

        // 抬起状态检测与手指物理收缩（弯折）
        float lift = fingers[f].liftAmount;
        float bendScale = (lift > 0.0f) ? (1.0f - (lift / 5.0f) * 0.18f) : 1.0f; // 抬起时手指长度物理缩短

        float wig = sinf(t * 1.6f + fingers[f].wigglePhase) * 0.4f;

        float segCurrX = fBaseX;
        float segCurrY = fBaseY;
        float segCurrZ = fBaseZ;

        // 3D 关节 Z 轴坐标分配 (从 -0.5f 逐渐伸展到最外侧 FRONT 玻璃)
        float jointZ[4];
        jointZ[0] = fBaseZ;
        jointZ[1] = Z_FACE - 7.0f;
        jointZ[2] = Z_FACE - 3.5f;
        jointZ[3] = Z_FACE - lift; // 最终端指尖：落地时紧扣 Z_FACE，抬手时缩回

        for (int s = 0; s < 3; s++) {
            float slipX = fingers[f].seg[s].slipX;
            float slipY = fingers[f].seg[s].slipY;
            
            float segH = SEG3D_H[s] * fingers[f].lengthScale * bendScale;
            float hw0  = SEG3D_HW[s];
            float hw1  = SEG3D_HW[s+1];

            // 终端关节
            float segEndX = segCurrX + wig;
            float segEndY = segCurrY + segH;
            float segEndZ = jointZ[s+1];

            // 投影 4 个顶点，体现指节的 3D 厚度变化与倾斜视差
            int q0x, q0y, q1x, q1y, q2x, q2y, q3x, q3y;
            proj3D(container, segCurrX - hw0 + slipX, segCurrY + slipY, segCurrZ, ax, ay, q0x, q0y);
            proj3D(container, segCurrX + hw0 + slipX, segCurrY + slipY, segCurrZ, ax, ay, q1x, q1y);
            proj3D(container, segEndX  + hw1,          segEndY,          segEndZ,  ax, ay, q2x, q2y);
            proj3D(container, segEndX  - hw1,          segEndY,          segEndZ,  ax, ay, q3x, q3y);

            // 指骨填充色（硬质半透水晶）
            float br = fingers[f].seg[s].brightness;
            uint8_t ri = (uint8_t)((14 + s * 6) * br + 6);
            uint8_t gi = (uint8_t)((45 + s * 10) * br);
            uint8_t bi = (uint8_t)((125 + s * 16) * br);
            uint16_t cSeg = canvas->color565(ri, gi, bi);

            fillQuad(canvas, q0x,q0y, q1x,q1y, q2x,q2y, q3x,q3y, cSeg);
            edgeQuad(canvas, q0x,q0y, q1x,q1y, q2x,q2y, q3x,q3y, cHL);

            // 绘制明亮的、尖锐几何对角线，表现坚硬水晶的切面折光 (而非闪电线)
            canvas->drawLine(q0x, q0y, q2x, q2y, cDiag);
            
            // 关节过渡横线
            if (s < 2) {
                canvas->drawLine(q3x, q3y, q2x, q2y, canvas->color565(150, 195, 255));
            }

            // 指尖触地高亮闪烁（表示与前面板玻璃强力交互）
            if (s == 2) {
                uint8_t tipBr = (uint8_t)(fmaxf(0, 1.0f - (lift / 5.0f)) * 255);
                if (tipBr > 30) {
                    uint16_t cTip = canvas->color565(tipBr/2, tipBr, 255);
                    canvas->drawLine(q2x, q2y, q3x, q3y, cTip);
                    canvas->drawPixel((q2x + q3x)/2, (q2y + q3y)/2, canvas->color565(255, 255, 255));
                }
            }

            segCurrX = segEndX;
            segCurrY = segEndY;
            segCurrZ = segEndZ;
        }
    }

    // ---- 绘制飞溅的 3D 碎玻璃粒子 (Crystal Shards) ----
    uint32_t now = millis();
    for (int i = 0; i < MAX_SHARDS; i++) {
        if (shards[i].active) {
            int sx, sy;
            proj3D(container, shards[i].x, shards[i].y, shards[i].z, ax, ay, sx, sy);
            
            // 确保粒子在屏幕可视范围内
            if (sx >= 0 && sx < 135 && sy >= 240 - 240) { // 修正y坐标上下限判定范围
                if (sx < 135 && sy < 240) {
                    // 计算剩余寿命比例
                    uint32_t age = now - shards[i].birth;
                    float lifePct = 1.0f - ((float)age / shards[i].life);
                    if (lifePct < 0) lifePct = 0;
                    
                    // 随着消逝，颜色从钻石白渐变到冰蓝，最后变暗淡出
                    uint8_t r = (uint8_t)(lifePct * 200 + 55);
                    uint8_t g = (uint8_t)(lifePct * 220 + 35);
                    uint8_t b = 255;
                    uint16_t cShard = canvas->color565(r, g, b);
                    
                    float sz = shards[i].size * lifePct;
                    if (sz > 0.5f) {
                        // 绘制晶莹剔透的小三角形或菱形
                        int isz = (int)sz;
                        canvas->fillTriangle(sx, sy - isz - 1, sx - isz, sy + isz, sx + isz, sy + isz, cShard);
                        // 中心高亮白点
                        canvas->drawPixel(sx, sy, canvas->color565(255, 255, 255));
                    }
                }
            }
        }
    }
}

// ============================================================================
// draw 主入口
// ============================================================================
void PhaseCrystal::draw(M5Canvas* canvas, Container* container, float ax, float ay, float az) {
    container->currentFace = FRONT;
    container->drawContainer(canvas, ax, ay, skeleton);
    drawCrystalHand(canvas, container, ax, ay);
    if (showDebug) drawDebug(canvas);
}

// ============================================================================
// 调试
// ============================================================================
void PhaseCrystal::drawDebug(M5Canvas* canvas) {
    canvas->setTextColor(TFT_CYAN);
    canvas->setTextSize(1);
    canvas->drawString(String("Crystal 3D FPS:")+String(currentFPS,0), 10, 10);
    canvas->drawString(String("Fingers:")+String(numFingers), 10, 20);
    canvas->drawString(isMoving ? "CRAWLING" : "STILL", 10, 30);
    canvas->drawString(String("3D:")+String(crawlX,0)+","+String(crawlY,0)+","+String(crawlZ,0), 10, 40);
}
