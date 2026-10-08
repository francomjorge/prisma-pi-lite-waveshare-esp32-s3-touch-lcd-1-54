#include "prisma_ui.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "prisma_bsp.h"

#define SCREEN_W 240

static const char *TAG = "prisma_ui";

typedef enum {
    PRISMA_UI_SPLASH = 0,
    PRISMA_UI_HOME,
} prisma_ui_screen_t;

static prisma_ui_screen_t s_screen = PRISMA_UI_SPLASH;
static int s_selected_module = -1;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8U) << 8) |
                      ((g & 0xFCU) << 3) |
                      ((b & 0xF8U) >> 3));
}

static uint16_t color_bg(void)      { return rgb565(9, 6, 18); }
static uint16_t color_panel(void)   { return rgb565(24, 20, 38); }
static uint16_t color_active(void)  { return rgb565(38, 31, 58); }
static uint16_t color_text(void)    { return rgb565(242, 240, 248); }
static uint16_t color_muted(void)   { return rgb565(138, 132, 154); }
static uint16_t color_cyan(void)    { return rgb565(44, 218, 210); }
static uint16_t color_magenta(void) { return rgb565(218, 72, 196); }

static const uint8_t GLYPH_A[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
static const uint8_t GLYPH_C[7] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E};
static const uint8_t GLYPH_D[7] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
static const uint8_t GLYPH_E[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
static const uint8_t GLYPH_H[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
static const uint8_t GLYPH_I[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F};
static const uint8_t GLYPH_L[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
static const uint8_t GLYPH_M[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11};
static const uint8_t GLYPH_O[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
static const uint8_t GLYPH_P[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10};
static const uint8_t GLYPH_R[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
static const uint8_t GLYPH_S[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
static const uint8_t GLYPH_T[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
static const uint8_t GLYPH_U[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
static const uint8_t GLYPH_V[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
static const uint8_t GLYPH_Y[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04};
static const uint8_t GLYPH_0[7] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E};
static const uint8_t GLYPH_2[7] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F};
static const uint8_t GLYPH_DOT[7] = {0x00,0x00,0x00,0x00,0x00,0x00,0x04};
static const uint8_t GLYPH_BLANK[7] = {0,0,0,0,0,0,0};

static const uint8_t *glyph(char c)
{
    switch (c) {
        case 'A': return GLYPH_A;
        case 'C': return GLYPH_C;
        case 'D': return GLYPH_D;
        case 'E': return GLYPH_E;
        case 'H': return GLYPH_H;
        case 'I': return GLYPH_I;
        case 'L': return GLYPH_L;
        case 'M': return GLYPH_M;
        case 'O': return GLYPH_O;
        case 'P': return GLYPH_P;
        case 'R': return GLYPH_R;
        case 'S': return GLYPH_S;
        case 'T': return GLYPH_T;
        case 'U': return GLYPH_U;
        case 'V': return GLYPH_V;
        case 'Y': return GLYPH_Y;
        case '0': return GLYPH_0;
        case '2': return GLYPH_2;
        case '.': return GLYPH_DOT;
        default:  return GLYPH_BLANK;
    }
}

static int text_width(const char *text, int scale)
{
    size_t length = strlen(text);
    if (length == 0) {
        return 0;
    }
    return (int)(((length * 6U) - 1U) * (size_t)scale);
}

static void draw_char(int x, int y, char c, int scale, uint16_t color)
{
    const uint8_t *rows = glyph(c);

    for (int row = 0; row < 7; ++row) {
        int col = 0;
        while (col < 5) {
            while (col < 5 && (rows[row] & (1U << (4 - col))) == 0) {
                ++col;
            }
            if (col >= 5) {
                break;
            }

            int start = col;
            while (col < 5 && (rows[row] & (1U << (4 - col))) != 0) {
                ++col;
            }

            prisma_bsp_display_fill_rect(
                (uint16_t)(x + start * scale),
                (uint16_t)(y + row * scale),
                (uint16_t)((col - start) * scale),
                (uint16_t)scale,
                color);
        }
    }
}

static void draw_text(int x, int y, const char *text, int scale, uint16_t color)
{
    while (*text != '\0') {
        draw_char(x, y, *text, scale, color);
        x += 6 * scale;
        ++text;
    }
}

static void draw_text_centered(int y, const char *text, int scale, uint16_t color)
{
    int width = text_width(text, scale);
    int x = (SCREEN_W - width) / 2;
    draw_text(x, y, text, scale, color);
}

static void draw_outline_rect(int x, int y, int width, int height, int thickness, uint16_t color)
{
    prisma_bsp_display_fill_rect(x, y, width, thickness, color);
    prisma_bsp_display_fill_rect(x, y + height - thickness, width, thickness, color);
    prisma_bsp_display_fill_rect(x, y, thickness, height, color);
    prisma_bsp_display_fill_rect(x + width - thickness, y, thickness, height, color);
}

static void draw_splash(void)
{
    prisma_bsp_display_fill(color_bg());

    draw_text_centered(18, "PRISMA", 3, color_text());
    prisma_bsp_display_fill_rect(48, 47, 144, 2, color_cyan());
    prisma_bsp_display_fill_rect(94, 47, 52, 2, color_magenta());
    draw_text_centered(58, "PI LITE", 2, color_muted());

    prisma_bsp_display_fill_rect(82, 92, 76, 58, color_panel());
    draw_outline_rect(82, 92, 76, 58, 2, color_active());

    /* Minimal PRISMA P mark. */
    prisma_bsp_display_fill_rect(100, 106, 9, 30, color_cyan());
    prisma_bsp_display_fill_rect(109, 106, 24, 8, color_cyan());
    prisma_bsp_display_fill_rect(126, 114, 8, 11, color_cyan());
    prisma_bsp_display_fill_rect(109, 124, 19, 8, color_magenta());

    draw_text_centered(162, "READY", 2, color_text());
    draw_text_centered(207, "TOUCH TO START", 1, color_muted());
    draw_text(211, 226, "0.2", 1, color_muted());
}

static void draw_module_card(int y, const char *label, bool selected)
{
    uint16_t fill = selected ? color_active() : color_panel();
    uint16_t border = selected ? color_cyan() : color_active();

    prisma_bsp_display_fill_rect(18, y, 204, 38, fill);
    draw_outline_rect(18, y, 204, 38, 2, border);
    draw_text_centered(y + 12, label, 2, selected ? color_text() : color_muted());

    if (selected) {
        prisma_bsp_display_fill_rect(18, y, 5, 38, color_magenta());
    }
}

static void draw_home(void)
{
    prisma_bsp_display_fill(color_bg());
    draw_text_centered(8, "PRISMA", 2, color_text());
    prisma_bsp_display_fill_rect(18, 31, 204, 2, color_cyan());

    draw_module_card(45,  "VISUAL", s_selected_module == 0);
    draw_module_card(96,  "AUDIO",  s_selected_module == 1);
    draw_module_card(147, "SYSTEM", s_selected_module == 2);

    draw_text_centered(211, "TOUCH A MODULE", 1, color_muted());
    draw_text(211, 226, "0.2", 1, color_muted());
}

void prisma_ui_init(void)
{
    s_screen = PRISMA_UI_SPLASH;
    s_selected_module = -1;
    draw_splash();
    ESP_LOGI(TAG, "M2 splash ready");
}

void prisma_ui_handle_touch(uint16_t x, uint16_t y)
{
    if (s_screen == PRISMA_UI_SPLASH) {
        s_screen = PRISMA_UI_HOME;
        draw_home();
        ESP_LOGI(TAG, "Entered home screen");
        return;
    }

    if (x < 18 || x >= 222) {
        return;
    }

    int selected = -1;
    if (y >= 45 && y < 83) {
        selected = 0;
    } else if (y >= 96 && y < 134) {
        selected = 1;
    } else if (y >= 147 && y < 185) {
        selected = 2;
    }

    if (selected >= 0 && selected != s_selected_module) {
        s_selected_module = selected;
        draw_home();
        ESP_LOGI(TAG, "Module selected: %d", s_selected_module);
    }
}
