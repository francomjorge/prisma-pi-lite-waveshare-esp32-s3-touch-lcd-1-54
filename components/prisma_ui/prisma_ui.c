#include "prisma_ui.h"

#include <stdint.h>
#include <stdlib.h>

#include "lvgl.h"
#include "waveshare_bsp.h"

#define GLITCH_POOL_SIZE 48
#define TAP_MAX_TRAVEL   22
#define SWIPE_MIN_TRAVEL 40

LV_IMAGE_DECLARE(moon_240);

static lv_obj_t *s_root;
static lv_obj_t *s_glitch_layer;
static lv_obj_t *s_blocks[GLITCH_POOL_SIZE];
static uint32_t s_block_cursor;
static lv_point_t s_press_point;
static uint32_t s_rng = 0x50524953u; /* "PRIS" */

static uint32_t rng_next(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static lv_color_t glitch_color(uint32_t value)
{
    switch (value % 6u) {
        case 0: return lv_color_hex(0x23DCDC);
        case 1: return lv_color_hex(0xAF37FF);
        case 2: return lv_color_hex(0xFF2E88);
        case 3: return lv_color_hex(0xF9E547);
        case 4: return lv_color_hex(0x58FF77);
        default:return lv_color_hex(0xFFFFFF);
    }
}

static lv_obj_t *next_block(void)
{
    lv_obj_t *block = s_blocks[s_block_cursor];
    if (block == NULL) {
        block = lv_obj_create(s_glitch_layer);
        lv_obj_remove_style_all(block);
        s_blocks[s_block_cursor] = block;
    }

    s_block_cursor = (s_block_cursor + 1u) % GLITCH_POOL_SIZE;
    return block;
}

static void glitch_burst(lv_coord_t x, lv_coord_t y)
{
    const unsigned count = 4u + (rng_next() % 4u);

    for (unsigned i = 0; i < count; ++i) {
        lv_obj_t *block = next_block();
        const uint32_t r = rng_next();
        const lv_coord_t w = 8 + (lv_coord_t)(r % 42u);
        const lv_coord_t h = 4 + (lv_coord_t)((r >> 8) % 22u);
        const lv_coord_t dx = (lv_coord_t)((int)((r >> 16) % 55u) - 27);
        const lv_coord_t dy = (lv_coord_t)((int)((r >> 24) % 43u) - 21);

        lv_obj_set_size(block, w, h);
        lv_obj_set_pos(block, x + dx - (w / 2), y + dy - (h / 2));
        lv_obj_set_style_bg_color(block, glitch_color(r >> 3), 0);
        lv_obj_set_style_bg_opa(block, LV_OPA_70 + (r % 50u), 0);
        lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_move_foreground(block);
    }
}

static void interaction_event(lv_event_t *event)
{
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(event);
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        s_press_point = p;
        return;
    }

    if (code != LV_EVENT_RELEASED) {
        return;
    }

    const int dx = (int)p.x - (int)s_press_point.x;
    const int dy = (int)p.y - (int)s_press_point.y;
    const int adx = abs(dx);
    const int ady = abs(dy);

    if (adx >= SWIPE_MIN_TRAVEL && adx > ady) {
        /* Reserved for Core view navigation. Build 0.1 intentionally has one local view. */
        return;
    }

    if (adx <= TAP_MAX_TRAVEL && ady <= TAP_MAX_TRAVEL) {
        glitch_burst(p.x, p.y);
    }
}

static void build_home(void)
{
    s_root = lv_screen_active();
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x02050A), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *image = lv_image_create(s_root);
    lv_image_set_src(image, &moon_240);
    lv_obj_center(image);

    lv_obj_t *shade = lv_obj_create(s_root);
    lv_obj_remove_style_all(shade);
    lv_obj_set_size(shade, 240, 240);
    lv_obj_center(shade);
    lv_obj_set_style_bg_color(shade, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(shade, LV_OPA_20, 0);

    lv_obj_t *brand = lv_label_create(s_root);
    lv_label_set_text(brand, "PRISMA-PI");
    lv_obj_set_style_text_color(brand, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_letter_space(brand, 2, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *lite = lv_label_create(s_root);
    lv_label_set_text(lite, "LITE  /  TOUCH GLITCH");
    lv_obj_set_style_text_color(lite, lv_color_hex(0x23DCDC), 0);
    lv_obj_set_style_text_font(lite, &lv_font_montserrat_14, 0);
    lv_obj_align(lite, LV_ALIGN_BOTTOM_MID, 0, -12);

    s_glitch_layer = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_glitch_layer);
    lv_obj_set_size(s_glitch_layer, 240, 240);
    lv_obj_center(s_glitch_layer);
    lv_obj_add_flag(s_glitch_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_glitch_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_glitch_layer, interaction_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_glitch_layer, interaction_event, LV_EVENT_RELEASED, NULL);
}

esp_err_t prisma_ui_start(void)
{
    waveshare_bsp_lock();
    build_home();
    waveshare_bsp_unlock();
    return ESP_OK;
}
