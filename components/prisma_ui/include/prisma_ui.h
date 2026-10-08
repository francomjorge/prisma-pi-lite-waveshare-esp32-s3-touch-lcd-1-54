#pragma once

#include <stdint.h>

/** Draw the four final appliance surfaces. */
void prisma_ui_init(void);

/** Route one new touch-down event into the appliance UI. */
void prisma_ui_handle_touch(uint16_t x, uint16_t y);

/** Lightweight periodic maintenance. */
void prisma_ui_tick(void);
