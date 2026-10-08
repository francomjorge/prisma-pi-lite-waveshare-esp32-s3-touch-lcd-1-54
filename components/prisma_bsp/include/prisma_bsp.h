#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool display_ready;
    bool touch_ready;
    uint8_t brightness_percent;
} prisma_bsp_status_t;

typedef struct {
    bool pressed;
    uint16_t x;
    uint16_t y;
} prisma_bsp_touch_sample_t;

/**
 * Initialize the Waveshare ESP32-S3-Touch-LCD-1.54 hardware required by M1.
 *
 * Hardware/controller details remain private to prisma_bsp.
 */
bool prisma_bsp_init(void);

/** Return the current raw board bring-up status. */
prisma_bsp_status_t prisma_bsp_get_status(void);

/** Set LCD backlight brightness in the range 0..100. */
void prisma_bsp_set_brightness(uint8_t percent);

/** Fill the complete 240x240 LCD with one RGB565 color. */
bool prisma_bsp_display_fill(uint16_t rgb565);

/** Poll the capacitive touch controller. */
bool prisma_bsp_touch_read(prisma_bsp_touch_sample_t *sample);
