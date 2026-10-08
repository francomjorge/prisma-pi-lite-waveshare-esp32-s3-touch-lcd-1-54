#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PRISMA_UI_GESTURE_NONE = 0,
    PRISMA_UI_GESTURE_SWIPE_LEFT,
    PRISMA_UI_GESTURE_SWIPE_RIGHT,
} prisma_ui_gesture_t;

/** Draw the four appliance surfaces from the shared synth state. */
void prisma_ui_init(void);

/** Feed every valid touch sample to the gesture/control layer. */
void prisma_ui_handle_touch_sample(bool pressed, uint16_t x, uint16_t y);

/** Navigate using the physical T1/T2 buttons. */
void prisma_ui_page_previous(void);
void prisma_ui_page_next(void);

/** Return one pending gesture event; gestures do not navigate pages. */
bool prisma_ui_take_gesture(prisma_ui_gesture_t *gesture);

/** Lightweight periodic maintenance and intro logo animation. */
void prisma_ui_tick(void);
