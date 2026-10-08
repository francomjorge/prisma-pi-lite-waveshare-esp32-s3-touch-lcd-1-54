#pragma once

#include <stdint.h>

/** Draw the M3 splash and restore the persisted UI state after first touch. */
void prisma_ui_init(void);

/** Route one new touch-down event into the M3 UI. */
void prisma_ui_handle_touch(uint16_t x, uint16_t y);

/** Lightweight periodic UI maintenance (diagnostics refresh). */
void prisma_ui_tick(void);
