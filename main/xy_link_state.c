#include "xy_link_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE s_link_lock = portMUX_INITIALIZER_UNLOCKED;
static prisma_xy_link_state_t s_link = {
    .locked = false,
    .randomized = false,
    .source = PRISMA_XY_FRACTAL,
    .rev = 1,
};

void prisma_xy_link_init(void) {
    portENTER_CRITICAL(&s_link_lock);
    s_link.locked = false;
    s_link.randomized = false;
    s_link.source = PRISMA_XY_FRACTAL;
    s_link.rev = 1;
    portEXIT_CRITICAL(&s_link_lock);
}

void prisma_xy_link_get(prisma_xy_link_state_t *out) {
    if (!out) return;
    portENTER_CRITICAL(&s_link_lock);
    *out = s_link;
    portEXIT_CRITICAL(&s_link_lock);
}

void prisma_xy_link_set_lock(prisma_xy_channel_t source, bool on) {
    if (source < 0 || source >= PRISMA_XY_CHANNEL_COUNT) source = PRISMA_XY_FRACTAL;
    prisma_xy_state_t src = {0};
    if (on) prisma_xy_get(source, &src);

    bool changed = false;
    portENTER_CRITICAL(&s_link_lock);
    if (s_link.locked != on || s_link.source != source || (!on && s_link.randomized)) changed = true;
    s_link.locked = on;
    s_link.source = source;
    if (!on) s_link.randomized = false;
    if (changed) s_link.rev++;
    portEXIT_CRITICAL(&s_link_lock);

    if (on) {
        prisma_xy_channel_t other = source == PRISMA_XY_FRACTAL ? PRISMA_XY_AUDIO : PRISMA_XY_FRACTAL;
        prisma_xy_set(other, src.x, src.y);
    }
}

void prisma_xy_link_set_random(bool on) {
    bool changed = false;
    portENTER_CRITICAL(&s_link_lock);
    bool next = s_link.locked && on;
    if (s_link.randomized != next) changed = true;
    s_link.randomized = next;
    if (changed) s_link.rev++;
    portEXIT_CRITICAL(&s_link_lock);
}

void prisma_xy_link_apply_input(prisma_xy_channel_t source, float x, float y) {
    if (source < 0 || source >= PRISMA_XY_CHANNEL_COUNT) return;
    prisma_xy_set(source, x, y);

    prisma_xy_link_state_t link;
    prisma_xy_link_get(&link);
    if (link.locked && !link.randomized) {
        prisma_xy_channel_t other = source == PRISMA_XY_FRACTAL ? PRISMA_XY_AUDIO : PRISMA_XY_FRACTAL;
        prisma_xy_set(other, x, y);
    }
}
