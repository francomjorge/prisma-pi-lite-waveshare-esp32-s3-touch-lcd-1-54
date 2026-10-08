#pragma once

#include <stdint.h>

/** Draw the initial PRISMA-Pi Lite screen. */
void prisma_ui_init(void);

/** Route one new touch-down event into the M2 UI. */
void prisma_ui_handle_touch(uint16_t x, uint16_t y);
