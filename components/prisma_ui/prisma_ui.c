#include "prisma_ui.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "prisma_app.h"
#include "prisma_bsp.h"
#include "prisma_network.h"
#include "prisma_platform.h"

#define SCREEN_W 240

static prisma_view_t s_view = PRISMA_VIEW_INTRO;
static uint32_t s_seen_revision = 0;
static char s_seen_ip[16] = {0};

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | ((b & 0xF8U) >> 3));
}

/* Fixed eight-color palette: BSP caches eight immutable DMA color buffers. */
static uint16_t c_bg(void)      { return rgb565(7, 8, 15); }
static uint16_t c_panel(void)   { return rgb565(17, 29, 52); }
static uint16_t c_text(void)    { return rgb565(244, 244, 242); }
static uint16_t c_muted(void)   { return rgb565(103, 120, 148); }
static uint16_t c_cyan(void)    { return rgb565(34, 226, 224); }
static uint16_t c_magenta(void) { return rgb565(236, 43, 185); }
static uint16_t c_yellow(void)  { return rgb565(255, 214, 54); }
static uint16_t c_green(void)   { return rgb565(38, 232, 113); }

static const uint8_t FONT[39][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},{0x01,0x01,0x01,0x01,0x11,0x11,0x0E},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},{0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},{0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},
    {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E},
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},{0x00,0x04,0x04,0x00,0x04,0x04,0x00},
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
    size_t n = strlen(text);
    return n ? (int)(((n * 6U) - 1U) * (size_t)scale) : 0;
}

static void draw_char(int x, int y, char c, int scale, uint16_t color)
{
    const uint8_t *rows = glyph(c);
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (rows[row] & (1U << (4 - col))) {
                prisma_bsp_display_fill_rect((uint16_t)(x + col * scale), (uint16_t)(y + row * scale),
                                             (uint16_t)scale, (uint16_t)scale, color);
            }
        }
    }
}

static void draw_text(int x, int y, const char *text, int scale, uint16_t color)
{
    while (*text) { draw_char(x, y, *text++, scale, color); x += 6 * scale; }
}

static void draw_center(int y, const char *text, int scale, uint16_t color)
{
    draw_text((SCREEN_W - text_width(text, scale)) / 2, y, text, scale, color);
}

static void draw_brand(int y, int scale)
{
    int x = (SCREEN_W - text_width("PRISMA-PI", scale)) / 2;
    draw_text(x, y, "PRISMA-", scale, c_text()); x += 7 * 6 * scale;
    draw_text(x, y, "P", scale, c_magenta()); x += 6 * scale;
    draw_text(x, y, "I", scale, c_yellow());
}

static void draw_dots(void)
{
    for (int i = 0; i < 4; ++i) {
        prisma_bsp_display_fill_rect((uint16_t)(92 + i * 16), 226, 8, 8,
                                     i == (int)s_view ? c_cyan() : c_muted());
    }
}

static void draw_bar(int x, int y, int w, int h, uint8_t value)
{
    prisma_bsp_display_fill_rect(x, y, w, h, c_muted());
    int fill = (w * value) / 100;
    if (fill > 0) prisma_bsp_display_fill_rect(x, y, fill, h, c_cyan());
}

static void draw_on_badge(void)
{
    prisma_bsp_display_fill_rect(188, 6, 42, 22, c_green());
    draw_text(198, 10, "ON", 2, c_bg());
}

static void draw_intro(void)
{
    prisma_bsp_display_fill(c_bg());
    draw_brand(42, 3);
    draw_center(92, "LITE", 3, c_text());
    draw_center(132, "VIDEO SYNTHESIZER", 1, c_text());
    draw_center(186, "LOCAL FIRST", 1, c_cyan());
    draw_dots();
}

static void draw_now_playing(const prisma_synth_state_t *st)
{
    char value[8];
    prisma_bsp_display_fill(c_bg());
    draw_brand(8, 2); draw_on_badge();
    draw_text(10, 43, "NOW PLAYING", 1, c_cyan());
    draw_text(10, 61, st->file_name, 2, c_text());
    draw_text(10, 101, "INTENSITY", 2, c_text());
    snprintf(value, sizeof(value), "%u", (unsigned)st->intensity);
    draw_text(194, 101, value, 2, c_text());
    draw_bar(10, 124, 220, 10, st->intensity);
    draw_text(10, 157, "MODE", 2, c_text());
    prisma_bsp_display_fill_rect(86, 148, 144, 42, c_panel());
    draw_center(161, st->bypass ? "BYPASS" : "ACTIVE", 2, st->bypass ? c_cyan() : c_green());
    draw_dots();
}

static void draw_browser(void)
{
    prisma_network_status_t net = prisma_network_get_status();
    prisma_bsp_display_fill(c_bg());
    draw_center(18, "VIEW IN BROWSER", 2, c_text());
    draw_center(58, net.ap_active ? "LOCAL AP" : "NETWORK OFFLINE", 1,
                net.ap_active ? c_green() : c_magenta());
    draw_center(86, net.ssid[0] ? net.ssid : "PRISMA-PI-LITE", 1, c_cyan());
    draw_center(112, net.ip[0] ? net.ip : "192.168.4.1", 2, c_text());
    draw_center(156, "OPEN THIS ADDRESS", 1, c_muted());
    draw_center(172, "IN YOUR BROWSER", 1, c_muted());
    draw_dots();
}

static const char *effect_name(int i)
{
    static const char *names[6] = {"PIXELATE","THRESHOLD","POSTERIZE","MIRROR","RANDOM","MIX"};
    return names[i];
}

static void draw_core(const prisma_synth_state_t *st)
{
    char value[8];
    prisma_bsp_display_fill(c_bg());
    draw_brand(5, 2); draw_on_badge();
    draw_center(29, "CORE EXTENDED", 1, c_cyan());

    for (int i = 0; i < PRISMA_EFFECT_COUNT; ++i) {
        int y = 48 + i * 27;
        bool enabled = st->effects[i].enabled;
        prisma_bsp_display_fill_rect(8, y + 3, 7, 7, enabled ? c_green() : c_muted());
        draw_text(21, y, effect_name(i), 1, enabled ? c_text() : c_muted());
        if (i == PRISMA_EFFECT_MIRROR) {
            prisma_bsp_display_fill_rect(113, y - 4, 52, 20, enabled ? c_cyan() : c_panel());
            prisma_bsp_display_fill_rect(166, y - 4, 52, 20, c_panel());
            draw_text(126, y, "ON", 1, enabled ? c_bg() : c_muted());
            draw_text(179, y, "OFF", 1, enabled ? c_muted() : c_text());
        } else {
            draw_bar(112, y + 2, 94, 8, st->effects[i].value);
            snprintf(value, sizeof(value), "%u", (unsigned)st->effects[i].value);
            draw_text(210, y, value, 1, c_text());
        }
    }
    draw_dots();
}

static void render(void)
{
    prisma_synth_state_t st = prisma_app_get_state();
    s_seen_revision = st.revision;
    switch (s_view) {
        case PRISMA_VIEW_NOW_PLAYING: draw_now_playing(&st); break;
        case PRISMA_VIEW_BROWSER: draw_browser(); break;
        case PRISMA_VIEW_CORE_EXTENDED: draw_core(&st); break;
        case PRISMA_VIEW_INTRO:
        default: draw_intro(); break;
    }
}

static void set_view(prisma_view_t view)
{
    if (view < PRISMA_VIEW_INTRO || view > PRISMA_VIEW_CORE_EXTENDED) return;
    s_view = view;
    (void)prisma_platform_set_last_view(view);
    render();
}

void prisma_ui_init(void)
{
    prisma_settings_t settings = prisma_platform_get_settings();
    prisma_bsp_set_brightness(settings.brightness_percent);
    s_view = settings.last_view;
    if (s_view < PRISMA_VIEW_INTRO || s_view > PRISMA_VIEW_CORE_EXTENDED) s_view = PRISMA_VIEW_INTRO;
    render();
}

void prisma_ui_handle_touch(uint16_t x, uint16_t y)
{
    if (y >= 214) {
        set_view((prisma_view_t)(x / 60));
        return;
    }

    if (s_view == PRISMA_VIEW_INTRO) {
        set_view(PRISMA_VIEW_NOW_PLAYING);
        return;
    }

    if (s_view == PRISMA_VIEW_NOW_PLAYING) {
        if (y >= 40 && y < 92) prisma_app_cycle_file();
        else if (y >= 110 && y < 145) prisma_app_set_intensity((uint8_t)((x * 100U) / 239U));
        else if (y >= 145 && y < 200) prisma_app_toggle_bypass();
        render();
        return;
    }

    if (s_view == PRISMA_VIEW_CORE_EXTENDED && y >= 44 && y < 210) {
        int row = (y - 44) / 27;
        if (row >= 0 && row < PRISMA_EFFECT_COUNT) {
            if (x < 108 || row == PRISMA_EFFECT_MIRROR) {
                prisma_app_toggle_effect((prisma_effect_id_t)row);
            } else {
                int value = ((int)x - 112) * 100 / 94;
                if (value < 0) value = 0;
                if (value > 100) value = 100;
                prisma_app_set_effect_value((prisma_effect_id_t)row, (uint8_t)value);
            }
            render();
        }
    }
}

void prisma_ui_tick(void)
{
    prisma_synth_state_t st = prisma_app_get_state();
    if (st.revision != s_seen_revision &&
        (s_view == PRISMA_VIEW_NOW_PLAYING || s_view == PRISMA_VIEW_CORE_EXTENDED)) {
        render();
    }

    if (s_view == PRISMA_VIEW_BROWSER) {
        prisma_network_status_t net = prisma_network_get_status();
        if (strncmp(s_seen_ip, net.ip, sizeof(s_seen_ip)) != 0) {
            snprintf(s_seen_ip, sizeof(s_seen_ip), "%s", net.ip);
            render();
        }
    }
}
