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
        triggerMindEcho();
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

void Renderer::renderFieldAndVoronoi(const MetaballSystem &metaballs, const VoronoiSurface &voronoi, const PhysiologySystem &physiology, bool is_sleeping) {
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

                float sqrt_d1 = std::sqrt(min_d1);
                float diff = (min_d2 - min_d1) / (sqrt_d1 + std::sqrt(min_d2));

                if (diff < membrane_thresh) {
                    // 活体肌肉纤维神经微反光
                    if (is_sleeping) {
                        // 睡眠时神经微光频率极度放缓（周期超10秒慢呼吸隐现），呈现深邃静谧微波
                        if ((gx + gy + (millis() / 950)) % 5 == 0) {
                            canvas->drawPixel(sx + 1, sy + 1, COLOR_NEURO_PULSE);
                        }
                    } else {
                        uint16_t pulse_color = (emotion == EMOTION_ANGER) ? COLOR_GLOW_CYAN : COLOR_NEURO_PULSE;
                        canvas->drawPixel(sx + 1, sy + 1, pulse_color);
                    }
                } else {
                    // 沥青高光微噪点：清醒时每 8 点取 1；睡眠时降低频率至每 24 点取 1 且受极慢大周期呼吸调制
                    int dither_step = is_sleeping ? 24 : 8;
                    if ((gx * 3 + gy * 7) % dither_step == 0) {
                        if (!is_sleeping || ((gx + gy + (millis() / 1400)) % 2 == 0)) {
                            canvas->drawPixel(sx + 1, sy + 1, COLOR_DITHER_GRAY);
                        }
                    }
                }
            }
        }
    }
}

void Renderer::triggerMindEcho(const char *custom_text) {
    if (custom_text && strlen(custom_text) > 0) {
        strncpy(current_echo_text, custom_text, sizeof(current_echo_text) - 1);
    }
    echo_state = ECHO_TYPING;
    typed_char_count = 0;
    char_timer = 0.0f;
    state_timer = 0.0f;
}

void Renderer::updateMindEchoLifecycle(float dt, const ConsciousnessStateV3 &v3_state) {
    switch (echo_state) {
        case ECHO_IDLE:
            if (v3_state.has_new_update && strlen(v3_state.notes) > 0) {
                triggerMindEcho(v3_state.notes);
            } else {
                auto_trigger_cooldown -= dt;
                if (auto_trigger_cooldown <= 0.0f) {
                    auto_trigger_cooldown = 12.0f + (rand() % 8);
                    if (strlen(v3_state.notes) > 0) {
                        triggerMindEcho(v3_state.notes);
                    }
                }
            }
            break;

        case ECHO_TYPING: {
            char_timer += dt;
            int total_len = strlen(current_echo_text);
            if (char_timer >= 0.045f) {
                char_timer = 0.0f;
                typed_char_count += 2; // 每次推进 2 字符以保证流畅感
                if (typed_char_count >= total_len) {
                    typed_char_count = total_len;
                    echo_state = ECHO_SUSTAIN;
                    state_timer = 0.0f;
                }
            }
            break;
        }

        case ECHO_SUSTAIN:
            state_timer += dt;
            if (state_timer >= 2.6f) { // 停留 2.6 秒供观察者阅读
                echo_state = ECHO_FADING;
                state_timer = 0.0f;
            }
            break;

        case ECHO_FADING:
            state_timer += dt;
            if (state_timer >= 0.7f) { // 0.7 秒淡出结束
                echo_state = ECHO_IDLE;
                state_timer = 0.0f;
            }
            break;
    }
}

void Renderer::renderMindEchoPanel() {
    if (echo_state == ECHO_IDLE || typed_char_count <= 0) return;

    int panel_x = 2;
    int panel_y = 48;
    int panel_w = SCREEN_W - 4;
    int panel_h = 38;

    // 直接显存指针超高速半透明压暗 (零开销, 50% 透光)
    uint16_t *fb = (uint16_t*)canvas->getBuffer();
    if (fb) {
        for (int py = panel_y; py < panel_y + panel_h; ++py) {
            uint16_t *row = fb + py * SCREEN_W + panel_x;
            for (int px = 0; px < panel_w; ++px) {
                row[px] = ((row[px] >> 1) & 0x7BEF) | 0x0821;
            }
        }
    }

    uint16_t border_col = (echo_state == ECHO_FADING) ? 0x6000 : 0xF800; // 红色边框
    uint16_t title_col  = (echo_state == ECHO_FADING) ? 0x8800 : COLOR_GLOW_CYAN;
    uint16_t text_col   = (echo_state == ECHO_FADING) ? 0x7BEF : 0xFFFF;

    canvas->drawRect(panel_x, panel_y, panel_w, panel_h, border_col);

    canvas->setTextSize(1);
    canvas->setTextColor(title_col);
    canvas->setCursor(6, 51);
    canvas->print("[TELEPATHY] 意识心流:");

    // 动态截取已打出的字符
    char display_buf[128];
    strncpy(display_buf, current_echo_text, typed_char_count);
    display_buf[typed_char_count] = '\0';

    bool show_cursor = (echo_state == ECHO_TYPING) || ((echo_state == ECHO_SUSTAIN) && ((millis() / 250) % 2 == 0));

    // 分行绘制
    char line1[42] = "";
    char line2[42] = "";

    if (typed_char_count <= 36) {
        strncpy(line1, display_buf, 36);
        line1[36] = '\0';
        if (show_cursor) strncat(line1, "_", 2);
    } else {
        strncpy(line1, display_buf, 36);
        line1[36] = '\0';
        strncpy(line2, display_buf + 36, 36);
        line2[36] = '\0';
        if (show_cursor) strncat(line2, "_", 2);
    }

    canvas->setTextColor(text_col);
    canvas->setCursor(6, 62);
    canvas->print(line1);

    if (line2[0] != '\0') {
        canvas->setCursor(6, 73);
        canvas->print(line2);
    }
}

void Renderer::renderHUD(const CreatureAI &ai, const PhysiologySystem &physiology,
                         const RelationshipSystem &relationship, const ExpressionLayer &expression,
                         const ConsciousnessStateV3 &v3_state, float fps) {
    int panel_x = 2;
    int panel_y = 2;
    int panel_w = SCREEN_W - 4;
    int panel_h = 44;

    // 1. 直接显存指针超高速半透明压暗 (零开销, 50% 透光, 毒液与背景若隐若现)
    uint16_t *fb = (uint16_t*)canvas->getBuffer();
    if (fb) {
        for (int py = panel_y; py < panel_y + panel_h; ++py) {
            uint16_t *row = fb + py * SCREEN_W + panel_x;
            for (int px = 0; px < panel_w; ++px) {
                row[px] = ((row[px] >> 1) & 0x7BEF) | 0x0821;
            }
        }
    }
    canvas->drawRect(panel_x, panel_y, panel_w, panel_h, COLOR_GLOW_CYAN);

    // 2. 文字采用无背景色透明绘制模式，自然悬浮于半透明毛玻璃之上
    canvas->setTextSize(1);
    canvas->setTextColor(TFT_WHITE);

    // 视觉刷新节流与显示缓冲 (每 350ms 更新一次，提供沉稳、清晰、极高可读性，消灭高频跳字)
    if (millis() - last_hud_refresh_ms >= 350) {
        last_hud_refresh_ms = millis();

        snprintf(hud_line1_ai, sizeof(hud_line1_ai), "AI: %-6s", ai.getStateName());
        snprintf(hud_line1_emo, sizeof(hud_line1_emo), "EMO: %-5s", physiology.getEmotionName());
        snprintf(hud_line1_fps, sizeof(hud_line1_fps), "FPS: %4.1f", fps);

        snprintf(hud_line2_nrg, sizeof(hud_line2_nrg), "NRG: %4.2f", physiology.getEnergy());
        snprintf(hud_line2_tru, sizeof(hud_line2_tru), "TRU: %4.2f", relationship.getTrust());
        snprintf(hud_line2_exp, sizeof(hud_line2_exp), "EXP: %-5s", expression.getExpressionName());

        snprintf(hud_line3_mic, sizeof(hud_line3_mic), "MIC: %2.0fdB", physiology.getMicDecibels());
        snprintf(hud_line3_res, sizeof(hud_line3_res), "RES: %4.2f", relationship.getResentment());
        snprintf(hud_line3_soc, sizeof(hud_line3_soc), "SOC: %4.2f", relationship.getSocialOpenness());

        snprintf(hud_line4_int, sizeof(hud_line4_int), "INT: %-18s", v3_state.emotional_shift);
    }

    // 行 1 (Y=4): [AI 行为] | [EMO 情绪] | [FPS 帧率]
    canvas->setCursor(6, 4);
    canvas->print(hud_line1_ai);
    canvas->setCursor(84, 4);
    canvas->print(hud_line1_emo);
    canvas->setCursor(162, 4);
    canvas->print(hud_line1_fps);

    // 行 2 (Y=14): [NRG 体力/能量] | [TRU 信任度] | [EXP 面部微表情]
    canvas->setCursor(6, 14);
    canvas->print(hud_line2_nrg);
    canvas->setCursor(84, 14);
    canvas->print(hud_line2_tru);
    canvas->setCursor(162, 14);
    canvas->print(hud_line2_exp);

    // 行 3 (Y=24): [MIC 麦克风分贝] | [RES 压力/怨念] | [SOC 社交开放度]
    canvas->setCursor(6, 24);
    canvas->print(hud_line3_mic);
    canvas->setCursor(84, 24);
    canvas->print(hud_line3_res);
    canvas->setCursor(162, 24);
    canvas->print(hud_line3_soc);

    // 行 4 (Y=34): [INT 心声意图]
    canvas->setCursor(6, 34);
    canvas->print(hud_line4_int);

    // 动态打字机心声涌现面板
    renderMindEchoPanel();
}

void Renderer::render(const SkeletonSystem &skeleton, const MetaballSystem &metaballs,
                    const EyeSystem &eye, const TentacleRenderer &tentacles,
                    const CreatureAI &ai, const PhysiologySystem &physiology,
                    const VoronoiSurface &voronoi, const FluidSymbolSystem &fluid_symbols,
                    const RelationshipSystem &relationship, const ExpressionLayer &expression,
                    const ConsciousnessStateV3 &v3_state, float fps,
                    const PreyBugSystem *bugs,
                    const PredatorSystem *predator) {
    if (!canvas) return;

    float dt = (fps > 5.0f) ? (1.0f / fps) : 0.033f;
    updateMindEchoLifecycle(dt, v3_state);

    // 清屏与背景
    canvas->fillSprite(getBackgroundColor());

    // 绘制小虫子生态 (Prey Bugs)
    if (bugs) {
        bugs->draw(*canvas);
    }

    // 绘制标量场肉身与 Voronoi 细胞
    renderFieldAndVoronoi(metaballs, voronoi, physiology, ai.isSleeping());

    // 绘制触手、捕食特效与眼睛
    tentacles.draw(*canvas, skeleton);
    if (predator) {
        predator->draw(*canvas);
    }
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
