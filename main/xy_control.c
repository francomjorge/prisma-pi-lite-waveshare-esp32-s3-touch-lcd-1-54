#include "xy_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE s_xy_lock_mux = portMUX_INITIALIZER_UNLOCKED;
static prisma_xy_state_t s_xy[PRISMA_XY_CHANNEL_COUNT] = {
    [PRISMA_XY_FRACTAL] = {.x = 0.5f, .y = 0.5f, .rev = 1},
    [PRISMA_XY_AUDIO] = {.x = 0.5f, .y = 0.5f, .rev = 1},
};
static prisma_xy_lock_state_t s_lock = {
    .locked = false,
    .source = PRISMA_XY_FRACTAL,
    .rev = 1,
};

static float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float absf_local(float v) {
    return v < 0.0f ? -v : v;
}

static float filter_axis(float prev, float input) {
    float d = input - prev;
    float ad = absf_local(d);

    /* About 0.7 px on a 240 px touch surface: reject idle jitter. */
    if (ad <= 0.0030f) return prev;

    /* Large gestures stay immediate; small quantized steps are softened. */
    float alpha;
    if (ad >= 0.0800f) alpha = 1.00f;
    else if (ad >= 0.0300f) alpha = 0.84f;
    else if (ad >= 0.0120f) alpha = 0.68f;
    else alpha = 0.48f;

    float out = prev + d * alpha;
    if (absf_local(input - out) <= 0.0015f) out = input;
    return clamp01(out);
}

static bool set_channel_locked(prisma_xy_channel_t channel, float x, float y) {
    float nx = filter_axis(s_xy[channel].x, x);
    float ny = filter_axis(s_xy[channel].y, y);
    float dx = absf_local(s_xy[channel].x - nx);
    float dy = absf_local(s_xy[channel].y - ny);
    if (dx <= 0.0005f && dy <= 0.0005f) return false;
    s_xy[channel].x = nx;
    s_xy[channel].y = ny;
    s_xy[channel].rev++;
    return true;
}

void prisma_xy_init(void) {
    portENTER_CRITICAL(&s_xy_lock_mux);
    s_xy[PRISMA_XY_FRACTAL] = (prisma_xy_state_t){.x = 0.5f, .y = 0.5f, .rev = 1};
    s_xy[PRISMA_XY_AUDIO] = (prisma_xy_state_t){.x = 0.5f, .y = 0.5f, .rev = 1};
    s_lock = (prisma_xy_lock_state_t){.locked = false, .source = PRISMA_XY_FRACTAL, .rev = 1};
    portEXIT_CRITICAL(&s_xy_lock_mux);
}

void prisma_xy_get(prisma_xy_channel_t channel, prisma_xy_state_t *out) {
    if (!out || channel < 0 || channel >= PRISMA_XY_CHANNEL_COUNT) return;
    portENTER_CRITICAL(&s_xy_lock_mux);
    *out = s_xy[channel];
    portEXIT_CRITICAL(&s_xy_lock_mux);
}

void prisma_xy_set(prisma_xy_channel_t channel, float x, float y) {
    if (channel < 0 || channel >= PRISMA_XY_CHANNEL_COUNT) return;
    x = clamp01(x);
    y = clamp01(y);
    portENTER_CRITICAL(&s_xy_lock_mux);
    set_channel_locked(channel, x, y);
    if (s_lock.locked) {
        prisma_xy_channel_t other = channel == PRISMA_XY_FRACTAL ? PRISMA_XY_AUDIO : PRISMA_XY_FRACTAL;
        set_channel_locked(other, x, y);
    }
    portEXIT_CRITICAL(&s_xy_lock_mux);
}

void prisma_xy_lock_get(prisma_xy_lock_state_t *out) {
    if (!out) return;
    portENTER_CRITICAL(&s_xy_lock_mux);
    *out = s_lock;
    portEXIT_CRITICAL(&s_xy_lock_mux);
}

void prisma_xy_lock_set(bool locked, prisma_xy_channel_t source) {
    if (source < 0 || source >= PRISMA_XY_CHANNEL_COUNT) source = PRISMA_XY_FRACTAL;
    portENTER_CRITICAL(&s_xy_lock_mux);
    if (!locked) {
        if (s_lock.locked) {
            s_lock.locked = false;
            s_lock.rev++;
        }
        portEXIT_CRITICAL(&s_xy_lock_mux);
        return;
    }

    if (!s_lock.locked || s_lock.source != source) {
        prisma_xy_channel_t other = source == PRISMA_XY_FRACTAL ? PRISMA_XY_AUDIO : PRISMA_XY_FRACTAL;
        float x = s_xy[source].x;
        float y = s_xy[source].y;
        set_channel_locked(other, x, y);
        s_lock.locked = true;
        s_lock.source = source;
        s_lock.rev++;
    }
    portEXIT_CRITICAL(&s_xy_lock_mux);
}
