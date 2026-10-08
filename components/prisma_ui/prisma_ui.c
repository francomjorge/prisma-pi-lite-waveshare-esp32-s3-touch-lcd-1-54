#include "prisma_ui.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "prisma_app.h"
#include "prisma_bsp.h"
#include "prisma_network.h"
#include "prisma_platform.h"

#define SCREEN_W 240
#define PAGE_COUNT 4
#define PAGER_Y 224
#define SWIPE_MIN_X 36
#define SWIPE_MAX_Y 72
#define INTRO_LOGO_Y 48
#define INTRO_LOGO_SCALE 3
#define LOGO_SWEEP_MS 140

static uint32_t s_seen_revision = 0;
static char s_seen_ip[16] = {0};
static uint8_t s_logo_phase = 0;
static TickType_t s_last_logo_tick = 0;
static uint8_t s_intro_glitch_level = 0;

typedef struct {
    bool active;
    bool control_drag;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t last_x;
    uint16_t last_y;
} touch_gesture_t;

static touch_gesture_t s_touch = {0};

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8U) << 8) | ((g & 0xFCU) << 3) | ((b & 0xF8U) >> 3));
}

/* Exactly eight colors: the BSP owns eight immutable DMA color-cache slots. */
static uint16_t c_bg(void)      { return rgb565(0, 0, 0); }
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
                prisma_bsp_display_fill_rect((uint16_t)(x + col * scale),
                                             (uint16_t)(y + row * scale),
                                             (uint16_t)scale,
                                             (uint16_t)scale,
                                             color);
            }
        }
    }
}

static void draw_text(int x, int y, const char *text, int scale, uint16_t color)
{
    while (*text) {
        draw_char(x, y, *text++, scale, color);
        x += 6 * scale;
    }
}

static void draw_center(int y, const char *text, int scale, uint16_t color)
{
    draw_text((SCREEN_W - text_width(text, scale)) / 2, y, text, scale, color);
}

static void draw_right(int right, int y, const char *text, int scale, uint16_t color)
{
    draw_text(right - text_width(text, scale), y, text, scale, color);
}

static uint16_t sweep_color(uint8_t index)
{
    switch (index & 3U) {
        case 0: return c_cyan();
        case 1: return c_magenta();
        case 2: return c_yellow();
        default: return c_green();
    }
}

static int brand_x(int scale)
{
    return (SCREEN_W - text_width("PRISMA-PI", scale)) / 2;
}

static void draw_brand(int y, int scale, uint8_t phase, bool clear_tail)
{
    int x = brand_x(scale);
    draw_text(x, y, "PRISMA", scale, c_text());
    x += 6 * 6 * scale;

    if (clear_tail) {
        prisma_bsp_display_fill_rect((uint16_t)x,
                                     (uint16_t)y,
                                     (uint16_t)(18 * scale),
                                     (uint16_t)(7 * scale),
                                     c_bg());
    }

    draw_char(x, y, '-', scale, sweep_color(phase));
    x += 6 * scale;
    draw_char(x, y, 'P', scale, sweep_color((uint8_t)(phase + 1U)));
    x += 6 * scale;
    draw_char(x, y, 'I', scale, sweep_color((uint8_t)(phase + 2U)));
}

static uint8_t current_page(void)
{
    prisma_synth_state_t st = prisma_app_get_state();
    return st.active_page < PAGE_COUNT ? st.active_page : 0U;
}

static void draw_dots(uint8_t page)
{
    for (int i = 0; i < PAGE_COUNT; ++i) {
        prisma_bsp_display_fill_rect((uint16_t)(92 + i * 16),
                                     PAGER_Y,
                                     8,
                                     8,
                                     i == (int)page ? c_cyan() : c_muted());
    }
}

static void draw_bar(int x, int y, int w, int h, uint8_t value)
{
    prisma_bsp_display_fill_rect(x, y, w, h, c_muted());
    int fill = (w * value) / 100;
    if (fill > 0) {
        prisma_bsp_display_fill_rect(x, y, fill, h, c_cyan());
    }
}

static void draw_on_badge(void)
{
    prisma_bsp_display_fill_rect(188, 6, 42, 22, c_green());
    draw_text(198, 10, "ON", 2, c_bg());
}

static void draw_intro_glitch_marks(void)
{
    for (uint8_t i = 0; i < s_intro_glitch_level; ++i) {
        int x1 = 8 + ((int)i * 37) % 178;
        int y1 = 30 + ((int)i * 23) % 150;
        int w1 = 12 + ((int)i * 11) % 38;
        uint16_t color1 = sweep_color((uint8_t)(i + s_logo_phase));
        prisma_bsp_display_fill_rect((uint16_t)x1, (uint16_t)y1, (uint16_t)w1, 3, color1);

        int x2 = 18 + ((int)i * 53) % 188;
        int y2 = 48 + ((int)i * 31) % 128;
        int w2 = 5 + ((int)i * 7) % 20;
        prisma_bsp_display_fill_rect((uint16_t)x2,
                                     (uint16_t)y2,
                                     (uint16_t)w2,
                                     5,
                                     sweep_color((uint8_t)(i + 2U)));
    }
}

static void draw_intro(void)
{
    prisma_bsp_display_fill(c_bg());
    draw_brand(INTRO_LOGO_Y, INTRO_LOGO_SCALE, s_logo_phase, false);
    draw_center(94, "LITE", 2, c_text());
    draw_right(222, 137, "VIDEO SYNTHESIZER", 1, c_text());
    draw_intro_glitch_marks();
    draw_dots(PRISMA_VIEW_INTRO);
    s_last_logo_tick = xTaskGetTickCount();
}

static void draw_now_playing(const prisma_synth_state_t *st)
{
    char value[8];
    prisma_bsp_display_fill(c_bg());
    draw_brand(8, 2, 0, false);
    draw_on_badge();
    draw_text(10, 43, "NOW PLAYING", 1, c_cyan());
    draw_text(10, 61, st->file_name, 2, c_text());
    draw_text(10, 101, "INTENSITY", 2, c_text());
    snprintf(value, sizeof(value), "%u", (unsigned)st->intensity);
    draw_text(194, 101, value, 2, c_text());
    draw_bar(10, 124, 220, 10, st->intensity);
    draw_text(10, 157, "MODE", 2, c_text());
    prisma_bsp_display_fill_rect(86, 148, 144, 42, c_panel());
    draw_center(161,
                st->bypass ? "BYPASS" : "ACTIVE",
                2,
                st->bypass ? c_cyan() : c_green());
    draw_dots(PRISMA_VIEW_NOW_PLAYING);
}

static void draw_browser(void)
{
    prisma_network_status_t net = prisma_network_get_status();
    prisma_bsp_display_fill(c_bg());
    draw_center(18, "VIEW IN BROWSER", 2, c_text());
    draw_center(58,
                net.ap_active ? "LOCAL AP" : "NETWORK OFFLINE",
                1,
                net.ap_active ? c_green() : c_magenta());
    draw_center(86, net.ssid[0] ? net.ssid : "PRISMA-PI-LITE", 1, c_cyan());
    draw_center(112, net.ip[0] ? net.ip : "192.168.4.1", 2, c_text());
    draw_center(156, "OPEN THIS ADDRESS", 1, c_muted());
    draw_center(172, "IN YOUR BROWSER", 1, c_muted());
    draw_dots(PRISMA_VIEW_BROWSER);
}

static const char *effect_name(int i)
{
    static const char *names[6] = {
        "PIXELATE", "THRESHOLD", "POSTERIZE", "MIRROR", "RANDOM", "MIX"
    };
    return names[i];
}

static void draw_core(const prisma_synth_state_t *st)
{
    char value[8];
    prisma_bsp_display_fill(c_bg());
    draw_brand(5, 2, 0, false);
    draw_on_badge();
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
    draw_dots(PRISMA_VIEW_CORE_EXTENDED);
}

static void render(void)
{
    prisma_synth_state_t st = prisma_app_get_state();
    if (st.active_page >= PAGE_COUNT) {
        st.active_page = PRISMA_VIEW_INTRO;
    }
    s_seen_revision = st.revision;

    switch ((prisma_view_t)st.active_page) {
        case PRISMA_VIEW_NOW_PLAYING:
            draw_now_playing(&st);
            break;
        case PRISMA_VIEW_BROWSER:
            draw_browser();
            break;
        case PRISMA_VIEW_CORE_EXTENDED:
            draw_core(&st);
            break;
        case PRISMA_VIEW_INTRO:
        default:
            draw_intro();
            break;
    }
}

static void set_page(uint8_t page)
{
    if (page >= PAGE_COUNT) return;
    prisma_app_set_page(page);
    (void)prisma_platform_set_last_view((prisma_view_t)page);
    render();
}

static void page_delta(int delta)
{
    int page = (int)current_page() + delta;
    if (page < 0) page = 0;
    if (page >= PAGE_COUNT) page = PAGE_COUNT - 1;
    if (page != (int)current_page()) {
        set_page((uint8_t)page);
    }
}

static int abs_i(int value)
{
    return value < 0 ? -value : value;
}

static bool is_horizontal_swipe(int dx, int dy)
{
    int ax = abs_i(dx);
    int ay = abs_i(dy);
    return ax >= SWIPE_MIN_X &&
           ay <= SWIPE_MAX_Y &&
           ax >= ay + 12;
}

static bool slider_zone(uint8_t page, uint16_t x, uint16_t y)
{
    if (page == PRISMA_VIEW_NOW_PLAYING) {
        return y >= 108 && y < 145;
    }

    if (page == PRISMA_VIEW_CORE_EXTENDED && y >= 44 && y < 210 && x >= 108) {
        int row = ((int)y - 44) / 27;
        return row >= 0 && row < PRISMA_EFFECT_COUNT && row != PRISMA_EFFECT_MIRROR;
    }

    return false;
}

static void apply_slider(uint8_t page, uint16_t x, uint16_t y)
{
    if (page == PRISMA_VIEW_NOW_PLAYING) {
        uint32_t value = ((uint32_t)x * 100U) / 239U;
        prisma_app_set_intensity((uint8_t)value);
        return;
    }

    if (page == PRISMA_VIEW_CORE_EXTENDED && y >= 44 && y < 210) {
        int row = ((int)y - 44) / 27;
        if (row < 0 || row >= PRISMA_EFFECT_COUNT || row == PRISMA_EFFECT_MIRROR) return;
        int value = ((int)x - 112) * 100 / 94;
        if (value < 0) value = 0;
        if (value > 100) value = 100;
        prisma_app_set_effect_value((prisma_effect_id_t)row, (uint8_t)value);
    }
}

static void intro_tap(void)
{
    if (s_intro_glitch_level < 10U) {
        ++s_intro_glitch_level;
    } else {
        s_intro_glitch_level = 1U;
    }
    draw_intro();
}

static void handle_tap(uint8_t page, uint16_t x, uint16_t y)
{
    if (y >= 214) {
        uint8_t target = (uint8_t)(x / 60U);
        if (target >= PAGE_COUNT) target = PAGE_COUNT - 1;
        set_page(target);
        return;
    }

    if (page == PRISMA_VIEW_INTRO) {
        intro_tap();
        return;
    }

    if (page == PRISMA_VIEW_NOW_PLAYING) {
        if (y >= 40 && y < 92) {
            prisma_app_cycle_file();
            render();
        } else if (y >= 145 && y < 200) {
            prisma_app_toggle_bypass();
            render();
        }
        return;
    }

    if (page == PRISMA_VIEW_CORE_EXTENDED && y >= 44 && y < 210) {
        int row = ((int)y - 44) / 27;
        if (row >= 0 && row < PRISMA_EFFECT_COUNT &&
            (x < 108 || row == PRISMA_EFFECT_MIRROR)) {
            prisma_app_toggle_effect((prisma_effect_id_t)row);
            render();
        }
    }
}

static void finish_gesture(void)
{
    uint8_t page = current_page();

    if (s_touch.control_drag) {
        return;
    }

    int dx = (int)s_touch.last_x - (int)s_touch.start_x;
    int dy = (int)s_touch.last_y - (int)s_touch.start_y;

    if (is_horizontal_swipe(dx, dy)) {
        if (dx < 0) {
            page_delta(+1);
        } else {
            page_delta(-1);
        }
        return;
    }

    handle_tap(page, s_touch.start_x, s_touch.start_y);
}

void prisma_ui_init(void)
{
    prisma_settings_t settings = prisma_platform_get_settings();
    prisma_bsp_set_brightness(settings.brightness_percent);

    uint8_t page = (uint8_t)settings.last_view;
    if (page >= PAGE_COUNT) page = PRISMA_VIEW_INTRO;
    prisma_app_set_page(page);
    render();
}

void prisma_ui_handle_touch_sample(bool pressed, uint16_t x, uint16_t y)
{
    if (pressed) {
        if (!s_touch.active) {
            s_touch.active = true;
            s_touch.start_x = x;
            s_touch.start_y = y;
            s_touch.last_x = x;
            s_touch.last_y = y;
            s_touch.control_drag = slider_zone(current_page(), x, y);
            if (s_touch.control_drag) {
                apply_slider(current_page(), x, y);
            }
            return;
        }

        s_touch.last_x = x;
        s_touch.last_y = y;
        if (s_touch.control_drag) {
            apply_slider(current_page(), x, y);
        }
        return;
    }

    if (s_touch.active) {
        finish_gesture();
        memset(&s_touch, 0, sizeof(s_touch));
    }
}

void prisma_ui_tick(void)
{
    prisma_synth_state_t st = prisma_app_get_state();
    if (st.revision != s_seen_revision) {
        render();
        st = prisma_app_get_state();
    }

    if (st.active_page == PRISMA_VIEW_BROWSER) {
        prisma_network_status_t net = prisma_network_get_status();
        if (strncmp(s_seen_ip, net.ip, sizeof(s_seen_ip)) != 0) {
            snprintf(s_seen_ip, sizeof(s_seen_ip), "%s", net.ip);
            render();
        }
    }

    if (st.active_page == PRISMA_VIEW_INTRO) {
        TickType_t now = xTaskGetTickCount();
        TickType_t interval = pdMS_TO_TICKS(LOGO_SWEEP_MS);
        if ((now - s_last_logo_tick) >= interval) {
            s_last_logo_tick = now;
            s_logo_phase = (uint8_t)((s_logo_phase + 1U) & 3U);
            draw_brand(INTRO_LOGO_Y, INTRO_LOGO_SCALE, s_logo_phase, true);
        }
    }
}
