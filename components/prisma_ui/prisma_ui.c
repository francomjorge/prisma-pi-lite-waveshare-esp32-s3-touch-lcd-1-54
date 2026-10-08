#include "prisma_ui.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "prisma_bsp.h"
#include "prisma_platform.h"

#define SCREEN_W 240
#define SCREEN_H 240
#define NAV_Y 212
#define NAV_H 28

static const char *TAG = "prisma_ui";

static bool s_splash = true;
static prisma_view_t s_view = PRISMA_VIEW_HOME;
static prisma_view_t s_resume_view = PRISMA_VIEW_HOME;
static bool s_update_checked = false;
static bool s_reset_banner = false;
static uint32_t s_diag_second = UINT32_MAX;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8U) << 8) |
                      ((g & 0xFCU) << 3) |
                      ((b & 0xF8U) >> 3));
}

static uint16_t color_bg(void)      { return rgb565(9, 6, 18); }
static uint16_t color_panel(void)   { return rgb565(24, 20, 38); }
static uint16_t color_active(void)  { return rgb565(42, 34, 64); }
static uint16_t color_text(void)    { return rgb565(242, 240, 248); }
static uint16_t color_muted(void)   { return rgb565(138, 132, 154); }
static uint16_t color_cyan(void)    { return rgb565(44, 218, 210); }
static uint16_t color_magenta(void) { return rgb565(218, 72, 196); }
static uint16_t color_ok(void)      { return color_cyan(); }
static uint16_t color_warn(void)    { return color_magenta(); }

static const uint8_t FONT[39][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},
    {0x01,0x01,0x01,0x01,0x11,0x11,0x0E},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},
    {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E},
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
    {0x00,0x04,0x04,0x00,0x04,0x04,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x04},
};

static const uint8_t GLYPH_SLASH[7] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10};
static const uint8_t GLYPH_BLANK[7] = {0,0,0,0,0,0,0};

static const uint8_t *glyph(char c)
{
    if (c >= 'A' && c <= 'Z') return FONT[c - 'A'];
    if (c >= '0' && c <= '9') return FONT[26 + (c - '0')];
    if (c == '-') return FONT[36];
    if (c == ':') return FONT[37];
    if (c == '.') return FONT[38];
    if (c == '/') return GLYPH_SLASH;
    return GLYPH_BLANK;
}

static int text_width(const char *text, int scale)
{
    size_t length = strlen(text);
    if (length == 0) return 0;
    return (int)(((length * 6U) - 1U) * (size_t)scale);
}

static void draw_char(int x, int y, char c, int scale, uint16_t color)
{
    const uint8_t *rows = glyph(c);
    for (int row = 0; row < 7; ++row) {
        int col = 0;
        while (col < 5) {
            while (col < 5 && (rows[row] & (1U << (4 - col))) == 0) ++col;
            if (col >= 5) break;
            int start = col;
            while (col < 5 && (rows[row] & (1U << (4 - col))) != 0) ++col;
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
    draw_text((SCREEN_W - text_width(text, scale)) / 2, y, text, scale, color);
}

static void draw_text_centered_in(int x, int width, int y,
                                  const char *text, int scale, uint16_t color)
{
    draw_text(x + (width - text_width(text, scale)) / 2, y, text, scale, color);
}

static void draw_outline_rect(int x, int y, int width, int height,
                              int thickness, uint16_t color)
{
    prisma_bsp_display_fill_rect(x, y, width, thickness, color);
    prisma_bsp_display_fill_rect(x, y + height - thickness, width, thickness, color);
    prisma_bsp_display_fill_rect(x, y, thickness, height, color);
    prisma_bsp_display_fill_rect(x + width - thickness, y, thickness, height, color);
}

static void draw_header(const char *title)
{
    draw_text_centered(8, title, 2, color_text());
    prisma_bsp_display_fill_rect(14, 31, 212, 2, color_cyan());
    prisma_bsp_display_fill_rect(92, 31, 56, 2, color_magenta());
}

static void draw_nav(void)
{
    static const char *labels[4] = {"HOME", "INPUT", "OUTPUT", "SYSTEM"};
    for (int i = 0; i < 4; ++i) {
        int x = i * 60;
        bool active = (s_view == (prisma_view_t)i);
        prisma_bsp_display_fill_rect(x, NAV_Y, 60, NAV_H,
                                     active ? color_active() : color_panel());
        if (active) prisma_bsp_display_fill_rect(x, NAV_Y, 60, 3, color_cyan());
        draw_text_centered_in(x, 60, NAV_Y + 11, labels[i], 1,
                              active ? color_text() : color_muted());
    }
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
    prisma_bsp_display_fill_rect(100, 106, 9, 30, color_cyan());
    prisma_bsp_display_fill_rect(109, 106, 24, 8, color_cyan());
    prisma_bsp_display_fill_rect(126, 114, 8, 11, color_cyan());
    prisma_bsp_display_fill_rect(109, 124, 19, 8, color_magenta());

    draw_text_centered(162, "M3 READY", 2, color_text());
    draw_text_centered(207, "TOUCH TO START", 1, color_muted());
    draw_text(205, 226, PRISMA_FW_VERSION, 1, color_muted());
}

static void draw_card(int x, int y, int width, int height,
                      const char *title, const char *subtitle, uint16_t accent)
{
    prisma_bsp_display_fill_rect(x, y, width, height, color_panel());
    draw_outline_rect(x, y, width, height, 2, color_active());
    prisma_bsp_display_fill_rect(x, y, 4, height, accent);
    draw_text_centered_in(x, width, y + 13, title, 2, color_text());
    draw_text_centered_in(x, width, y + 40, subtitle, 1, color_muted());
}

static void draw_home(void)
{
    prisma_bsp_display_fill(color_bg());
    draw_header("PRISMA");
    draw_text_centered(36, s_reset_banner ? "RESET OK" : "DEVICE READY", 1, color_ok());
    s_reset_banner = false;

    draw_card(10, 51, 105, 58, "INPUT",  "LOCAL FILE", color_cyan());
    draw_card(125, 51, 105, 58, "OUTPUT", "DISPLAY", color_magenta());
    draw_card(10, 121, 105, 58, "SYSTEM", "SETTINGS", color_magenta());
    draw_card(125, 121, 105, 58, "DIAG", "STATUS", color_cyan());
    draw_nav();
}

static void draw_input(void)
{
    prisma_bsp_display_fill(color_bg());
    draw_header("INPUT");
    prisma_bsp_display_fill_rect(14, 47, 212, 129, color_panel());
    draw_outline_rect(14, 47, 212, 129, 2, color_active());
    draw_text(28, 63, "LOCAL FILE", 2, color_text());
    draw_text(28, 91, "IMAGE / VIDEO", 1, color_muted());
    prisma_bsp_display_fill_rect(28, 112, 184, 2, color_active());
    draw_text(28, 128, "STATUS", 1, color_muted());
    draw_text(28, 148, "NOT LOADED", 2, color_warn());
    draw_text_centered(190, "MEDIA LATER", 1, color_muted());
    draw_nav();
}

static void draw_output(void)
{
    prisma_bsp_display_fill(color_bg());
    draw_header("OUTPUT");
    prisma_bsp_display_fill_rect(14, 47, 212, 58, color_panel());
    draw_outline_rect(14, 47, 212, 58, 2, color_active());
    draw_text(28, 61, "DISPLAY", 2, color_text());
    draw_text(28, 84, "READY", 1, color_ok());

    prisma_bsp_display_fill_rect(14, 116, 212, 60, color_panel());
    draw_outline_rect(14, 116, 212, 60, 2, color_active());
    draw_text(28, 130, "NETWORK STREAM", 1, color_text());
    draw_text(28, 151, "LATER", 1, color_muted());
    draw_text_centered(190, "LOCAL DISPLAY ACTIVE", 1, color_muted());
    draw_nav();
}

static void draw_system_row(int y, const char *label, const char *value,
                            uint16_t value_color)
{
    prisma_bsp_display_fill_rect(14, y, 212, 42, color_panel());
    draw_outline_rect(14, y, 212, 42, 1, color_active());
    draw_text(25, y + 8, label, 1, color_text());
    draw_text(25, y + 24, value, 1, value_color);
}

static void draw_system(void)
{
    prisma_bsp_display_fill(color_bg());
    draw_header("SYSTEM");
    draw_system_row(44, "SOFTWARE VERSION", PRISMA_FW_VERSION, color_cyan());
    draw_system_row(93, "UPDATE STATUS",
                    s_update_checked ? "NO NETWORK" : "TAP TO CHECK",
                    s_update_checked ? color_warn() : color_muted());
    draw_system_row(142, "RESET SETTINGS", "TAP TO RESET", color_muted());
    draw_text_centered(194, "NO OTA IN LITE V1", 1, color_muted());
    draw_nav();
}

static void draw_diagnostics(void)
{
    char line[32];
    prisma_bsp_status_t status = prisma_bsp_get_status();
    prisma_bsp_display_fill(color_bg());
    draw_header("DIAG");

    draw_text(22, 48, "DISPLAY", 1, color_text());
    draw_text(152, 48, status.display_ready ? "OK" : "FAIL", 1,
              status.display_ready ? color_ok() : color_warn());
    draw_text(22, 70, "TOUCH", 1, color_text());
    draw_text(152, 70, status.touch_ready ? "OK" : "FAIL", 1,
              status.touch_ready ? color_ok() : color_warn());
    draw_text(22, 92, "FW", 1, color_text());
    draw_text(82, 92, PRISMA_FW_VERSION, 1, color_cyan());

    snprintf(line, sizeof(line), "UPTIME %lu S",
             (unsigned long)prisma_platform_uptime_seconds());
    draw_text(22, 120, line, 1, color_text());
    snprintf(line, sizeof(line), "HEAP %lu KB",
             (unsigned long)(prisma_platform_free_heap_bytes() / 1024U));
    draw_text(22, 144, line, 1, color_text());

    prisma_settings_t settings = prisma_platform_get_settings();
    snprintf(line, sizeof(line), "BRIGHT %u", (unsigned)settings.brightness_percent);
    draw_text(22, 168, line, 1, color_muted());
    draw_text_centered(194, "LIVE STATUS", 1, color_cyan());
    draw_nav();
    s_diag_second = prisma_platform_uptime_seconds();
}

static void draw_current(void)
{
    switch (s_view) {
        case PRISMA_VIEW_HOME: draw_home(); break;
        case PRISMA_VIEW_INPUT: draw_input(); break;
        case PRISMA_VIEW_OUTPUT: draw_output(); break;
        case PRISMA_VIEW_SYSTEM: draw_system(); break;
        case PRISMA_VIEW_DIAGNOSTICS: draw_diagnostics(); break;
        default:
            s_view = PRISMA_VIEW_HOME;
            draw_home();
            break;
    }
}

static void enter_view(prisma_view_t view)
{
    if (view < PRISMA_VIEW_HOME || view > PRISMA_VIEW_DIAGNOSTICS) {
        view = PRISMA_VIEW_HOME;
    }
    s_view = view;
    if (view <= PRISMA_VIEW_SYSTEM) {
        (void)prisma_platform_set_last_view(view);
    }
    draw_current();
    ESP_LOGI(TAG, "View=%u", (unsigned)view);
}

static void touch_feedback(uint16_t x, uint16_t y)
{
    uint16_t fx = (x > 2U) ? (uint16_t)(x - 2U) : 0U;
    uint16_t fy = (y > 2U) ? (uint16_t)(y - 2U) : 0U;
    if (fx > SCREEN_W - 5) fx = SCREEN_W - 5;
    if (fy > SCREEN_H - 5) fy = SCREEN_H - 5;
    prisma_bsp_display_fill_rect(fx, fy, 5, 5, color_magenta());
    vTaskDelay(pdMS_TO_TICKS(35));
}

void prisma_ui_init(void)
{
    prisma_settings_t settings = prisma_platform_get_settings();
    s_splash = true;
    s_view = PRISMA_VIEW_HOME;
    s_resume_view = settings.last_view;
    s_update_checked = false;
    s_reset_banner = false;
    s_diag_second = UINT32_MAX;
    prisma_bsp_set_brightness(settings.brightness_percent);
    draw_splash();
    ESP_LOGI(TAG, "M3 shell ready; resume_view=%u brightness=%u",
             (unsigned)s_resume_view, (unsigned)settings.brightness_percent);
}

void prisma_ui_handle_touch(uint16_t x, uint16_t y)
{
    if (s_splash) {
        s_splash = false;
        enter_view(s_resume_view);
        return;
    }

    touch_feedback(x, y);

    if (y >= NAV_Y) {
        int nav = x / 60U;
        if (nav >= 0 && nav <= 3) enter_view((prisma_view_t)nav);
        return;
    }

    if (s_view == PRISMA_VIEW_HOME) {
        if (y >= 51 && y < 109) {
            enter_view(x < 120 ? PRISMA_VIEW_INPUT : PRISMA_VIEW_OUTPUT);
            return;
        }
        if (y >= 121 && y < 179) {
            enter_view(x < 120 ? PRISMA_VIEW_SYSTEM : PRISMA_VIEW_DIAGNOSTICS);
            return;
        }
    }

    if (s_view == PRISMA_VIEW_SYSTEM) {
        if (y >= 93 && y < 135) {
            s_update_checked = true;
            draw_system();
            ESP_LOGI(TAG, "Manual update status check: network layer not present in M3");
            return;
        }
        if (y >= 142 && y < 184) {
            bool ok = prisma_platform_reset_settings();
            prisma_settings_t settings = prisma_platform_get_settings();
            prisma_bsp_set_brightness(settings.brightness_percent);
            s_reset_banner = ok;
            enter_view(PRISMA_VIEW_HOME);
            ESP_LOGI(TAG, "Settings reset: %s", ok ? "ok" : "failed");
            return;
        }
    }

    draw_current();
}

void prisma_ui_tick(void)
{
    if (s_splash || s_view != PRISMA_VIEW_DIAGNOSTICS) return;
    uint32_t now = prisma_platform_uptime_seconds();
    if (now != s_diag_second) draw_diagnostics();
}
