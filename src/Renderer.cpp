#include "Renderer.h"
#include <mbedtls/base64.h>
#include <cmath>

Renderer::Renderer() {}

void Renderer::init(M5Canvas *target_canvas) {
    canvas = target_canvas;
}

void Renderer::setTheme(BackgroundTheme theme) {
    current_theme = (BackgroundTheme)((int)theme % THEME_COUNT);
}

void Renderer::nextTheme() {
    current_theme = (BackgroundTheme)(((int)current_theme + 1) % THEME_COUNT);
}

void Renderer::toggleHUD() {
    show_hud = !show_hud;
    if (show_hud) {
        consciousness_leak_active = ((rand() % 100) < 5);
    }
}

uint16_t Renderer::getBackgroundColor() const {
    switch (current_theme) {
        case THEME_DESIGN_BLUE: return 0x0116;
        case THEME_PURE_BLACK:  return 0x0000;
        case THEME_SLATE_GRAY:  return 0x2124;
        case THEME_NEON_PURPLE: return 0x1808;
        default:                return 0x0116;
    }
}

void Renderer::renderFieldAndVoronoi(const MetaballSystem &metaballs, const VoronoiSurface &voronoi, const PhysiologySystem &physiology) {
    const uint8_t *field = metaballs.getFieldBuffer();
    EmotionState emotion = physiology.getEmotion();
    float membrane_thresh = voronoi.getMembraneThreshold();
    int seed_count = voronoi.getSeedCount();

    for (int gy = 0; gy < GRID_H; ++gy) {
        int row_idx = gy * GRID_W;
        int sy = gy * GRID_SCALE;

        for (int gx = 0; gx < GRID_W; ++gx) {
            uint8_t val = field[row_idx + gx];
            if (val < MetaballSystem::THRESHOLD) continue;

            int sx = gx * GRID_SCALE;

            // 1. 保证 100% 坚实纯黑肉身填充（绝对不镂空）
            canvas->fillRect(sx, sy, GRID_SCALE, GRID_SCALE, COLOR_VENOM_CORE);

            // 2. 表面有机沥青噪波与活体神经微光（仅在表面 1 像素点缀微反光）
            if (val > 130 && val < 210) {
                float px = (float)(sx + 1);
                float py = (float)(sy + 1);

                float min_d1 = 999999.0f;
                float min_d2 = 999999.0f;

                for (int s = 0; s < seed_count; ++s) {
                    const VoronoiSeed &sd = voronoi.getSeed(s);
                    float dx = px - sd.x;
                    float dy = py - sd.y;
                    float d2 = dx * dx + dy * dy;

                    if (d2 < min_d1) {
                        min_d2 = min_d1;
                        min_d1 = d2;
                    } else if (d2 < min_d2) {
                        min_d2 = d2;
                    }
                }

                float diff = std::sqrt(min_d2) - std::sqrt(min_d1);

                if (diff < membrane_thresh) {
                    // 活体肌肉纤维神经微反光
                    uint16_t pulse_color = (emotion == EMOTION_ANGER) ? COLOR_GLOW_CYAN : COLOR_NEURO_PULSE;
                    canvas->drawPixel(sx + 1, sy + 1, pulse_color);
                } else if ((gx * 3 + gy * 7) % 8 == 0) {
                    // 沥青高光微噪点
                    canvas->drawPixel(sx + 1, sy + 1, COLOR_DITHER_GRAY);
                }
            }
        }
    }
}

void Renderer::renderMeniscusGlow(const MetaballSystem &metaballs) {
    const uint8_t *field = metaballs.getFieldBuffer();

    int bottom_gy = GRID_H - 1;
    int bottom_sy = SCREEN_H - 1;
    for (int gx = 0; gx < GRID_W; ++gx) {
        if (field[bottom_gy * GRID_W + gx] >= MetaballSystem::THRESHOLD) {
            int sx = gx * GRID_SCALE;
            canvas->drawFastHLine(sx, bottom_sy, GRID_SCALE, COLOR_GLOW_WHITE);
            canvas->drawFastHLine(sx, bottom_sy - 1, GRID_SCALE, COLOR_GLOW_CYAN);
        }
    }

    for (int gx = 0; gx < GRID_W; ++gx) {
        if (field[0 * GRID_W + gx] >= MetaballSystem::THRESHOLD) {
            int sx = gx * GRID_SCALE;
            canvas->drawFastHLine(sx, 0, GRID_SCALE, COLOR_GLOW_WHITE);
            canvas->drawFastHLine(sx, 1, GRID_SCALE, COLOR_GLOW_CYAN);
        }
    }

    for (int gy = 0; gy < GRID_H; ++gy) {
        if (field[gy * GRID_W + 0] >= MetaballSystem::THRESHOLD) {
            int sy = gy * GRID_SCALE;
            canvas->drawFastVLine(0, sy, GRID_SCALE, COLOR_GLOW_WHITE);
            canvas->drawFastVLine(1, sy, GRID_SCALE, COLOR_GLOW_CYAN);
        }
    }

    int right_gx = GRID_W - 1;
    int right_sx = SCREEN_W - 1;
    for (int gy = 0; gy < GRID_H; ++gy) {
        if (field[gy * GRID_W + right_gx] >= MetaballSystem::THRESHOLD) {
            int sy = gy * GRID_SCALE;
            canvas->drawFastVLine(right_sx, sy, GRID_SCALE, COLOR_GLOW_WHITE);
            canvas->drawFastVLine(right_sx - 1, sy, GRID_SCALE, COLOR_GLOW_CYAN);
        }
    }
}

void Renderer::renderHUD(const CreatureAI &ai, const PhysiologySystem &physiology,
                         const RelationshipSystem &relationship, const ExpressionLayer &expression,
                         const ConsciousnessStateV3 &v3_state, float fps) {
    canvas->setTextSize(1);
    canvas->setTextColor(TFT_WHITE, 0x0000);

    canvas->fillRect(2, 2, SCREEN_W - 4, 38, 0x0841);
    canvas->drawRect(2, 2, SCREEN_W - 4, 38, COLOR_GLOW_CYAN);

    canvas->setCursor(6, 5);
    canvas->printf("AI: %s | Emo: %s | FPS: %.1f", ai.getStateName(), physiology.getEmotionName(), fps);

    canvas->setCursor(6, 16);
    canvas->printf("Trust: %.2f | Resent: %.2f | Expr: %s",
                   relationship.getTrust(), relationship.getResentment(), expression.getExpressionName());

    canvas->setCursor(6, 27);
    canvas->printf("Intent: %s | Social: %.2f", v3_state.emotional_shift, relationship.getSocialOpenness());

    if (consciousness_leak_active || (v3_state.notes[0] != '\0' && (rand() % 100) < 5)) {
        canvas->fillRect(2, 44, SCREEN_W - 4, 34, 0x0000);
        canvas->drawRect(2, 44, SCREEN_W - 4, 34, 0xF800);
        canvas->setTextColor(COLOR_GLOW_CYAN, 0x0000);
        canvas->setCursor(6, 47);
        canvas->print("[LEAK] Mind Echo:");
        canvas->setTextColor(TFT_WHITE, 0x0000);
        canvas->setCursor(6, 58);
        char snippet[48];
        strncpy(snippet, v3_state.notes, 44);
        snippet[44] = '\0';
        canvas->printf("\"%s...\"", snippet);
    }
}

void Renderer::render(const SkeletonSystem &skeleton, const MetaballSystem &metaballs,
                      const EyeSystem &eye, const TentacleRenderer &tentacles,
                      const CreatureAI &ai, const PhysiologySystem &physiology,
                      const VoronoiSurface &voronoi, const FluidSymbolSystem &fluid_symbols,
                      const RelationshipSystem &relationship, const ExpressionLayer &expression,
                      const ConsciousnessStateV3 &v3_state, float fps) {
    if (!canvas) return;

    canvas->fillSprite(getBackgroundColor());

    // 绘制坚实饱满的黑色主体与表面沥青噪波
    renderFieldAndVoronoi(metaballs, voronoi, physiology);

    // 绘制贴墙荧光
    renderMeniscusGlow(metaballs);

    // 绘制七肢桶活体液态墨水符号
    fluid_symbols.draw(*canvas);

    // 绘制触手与眼睛
    tentacles.draw(*canvas);
    eye.draw(*canvas, physiology);

    if (show_hud) {
        renderHUD(ai, physiology, relationship, expression, v3_state, fps);
    }
}

void Renderer::sendScreenshotSerial() {
    if (!canvas) return;

    Serial.println("\n==VENOM_B64_START==");

    int row_stride = (SCREEN_W * 3 + 3) & ~3;
    int image_size = row_stride * SCREEN_H;
    int file_size = 54 + image_size;

    uint8_t *bmp_buf = (uint8_t *)malloc(file_size);
    if (!bmp_buf) {
        Serial.println(">>> Err: BMP malloc failed");
        Serial.println("==VENOM_B64_END==");
        return;
    }

    memset(bmp_buf, 0, file_size);

    bmp_buf[0] = 'B'; bmp_buf[1] = 'M';
    bmp_buf[2] = (uint8_t)(file_size);
    bmp_buf[3] = (uint8_t)(file_size >> 8);
    bmp_buf[4] = (uint8_t)(file_size >> 16);
    bmp_buf[5] = (uint8_t)(file_size >> 24);
    bmp_buf[10] = 54;

    bmp_buf[14] = 40;
    bmp_buf[18] = (uint8_t)(SCREEN_W);
    bmp_buf[19] = (uint8_t)(SCREEN_W >> 8);
    bmp_buf[22] = (uint8_t)(SCREEN_H);
    bmp_buf[23] = (uint8_t)(SCREEN_H >> 8);
    bmp_buf[26] = 1;
    bmp_buf[28] = 24;
    bmp_buf[34] = (uint8_t)(image_size);
    bmp_buf[35] = (uint8_t)(image_size >> 8);

    for (int y = 0; y < SCREEN_H; ++y) {
        int bmp_y = SCREEN_H - 1 - y;
        uint8_t *row_ptr = bmp_buf + 54 + bmp_y * row_stride;
        for (int x = 0; x < SCREEN_W; ++x) {
            uint16_t rgb565 = canvas->readPixel(x, y);
            uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
            uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
            uint8_t b = (rgb565 & 0x1F) << 3;

            row_ptr[x * 3 + 0] = b;
            row_ptr[x * 3 + 1] = g;
            row_ptr[x * 3 + 2] = r;
        }
    }

    size_t b64_len = 0;
    mbedtls_base64_encode(nullptr, 0, &b64_len, bmp_buf, file_size);
    uint8_t *b64_out = (uint8_t *)malloc(b64_len + 1);
    if (b64_out) {
        mbedtls_base64_encode(b64_out, b64_len + 1, &b64_len, bmp_buf, file_size);
        b64_out[b64_len] = '\0';

        const char *p = (const char *)b64_out;
        while (*p) {
            char chunk[129];
            strncpy(chunk, p, 128);
            chunk[128] = '\0';
            Serial.println(chunk);
            p += strlen(chunk);
            delay(2);
        }
        free(b64_out);
    }
    free(bmp_buf);

    Serial.println("==VENOM_B64_END==");
}
