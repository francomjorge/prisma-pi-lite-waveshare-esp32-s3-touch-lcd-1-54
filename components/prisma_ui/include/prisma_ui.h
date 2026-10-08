#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Draw the four appliance surfaces from the shared synth state. */
void prisma_ui_init(void);

/**
 * Feed every valid touch sample to the UI.
 * pressed=true tracks the finger continuously; pressed=false completes the gesture.
 */
void prisma_ui_handle_touch_sample(bool pressed, uint16_t x, uint16_t y);

/** Lightweight periodic maintenance and intro logo animation. */
void prisma_ui_tick(void);
