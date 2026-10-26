#include "xy_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE s_xy_lock = portMUX_INITIALIZER_UNLOCKED;
static prisma_xy_state_t s_xy[PRISMA_XY_CHANNEL_COUNT] = {
    [PRISMA_XY_FRACTAL] = {.x = 0.5f, .y = 0.5f, .rev = 1},
    [PRISMA_XY_AUDIO] = {.x = 0.5f, .y = 0.5f, .rev = 1},
};

static float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void prisma_xy_init(void) {
    portENTER_CRITICAL(&s_xy_lock);
    s_xy[PRISMA_XY_FRACTAL] = (prisma_xy_state_t){.x = 0.5f, .y = 0.5f, .rev = 1};
    s_xy[PRISMA_XY_AUDIO] = (prisma_xy_state_t){.x = 0.5f, .y = 0.5f, .rev = 1};
    portEXIT_CRITICAL(&s_xy_lock);
}

void prisma_xy_get(prisma_xy_channel_t channel, prisma_xy_state_t *out) {
    if (!out || channel < 0 || channel >= PRISMA_XY_CHANNEL_COUNT) return;
    portENTER_CRITICAL(&s_xy_lock);
    *out = s_xy[channel];
    portEXIT_CRITICAL(&s_xy_lock);
}

void prisma_xy_set(prisma_xy_channel_t channel, float x, float y) {
    if (channel < 0 || channel >= PRISMA_XY_CHANNEL_COUNT) return;
    x = clamp01(x);
    y = clamp01(y);
    portENTER_CRITICAL(&s_xy_lock);
    float dx = s_xy[channel].x - x;
    float dy = s_xy[channel].y - y;
    if (dx < 0.0f) dx = -dx;
    if (dy < 0.0f) dy = -dy;
    if (dx > 0.0005f || dy > 0.0005f) {
        s_xy[channel].x = x;
        s_xy[channel].y = y;
        s_xy[channel].rev++;
    }
    portEXIT_CRITICAL(&s_xy_lock);
}
