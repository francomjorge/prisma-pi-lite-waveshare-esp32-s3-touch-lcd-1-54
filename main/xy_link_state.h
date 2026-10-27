#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "xy_control.h"

typedef struct {
    bool locked;
    bool randomized;
    prisma_xy_channel_t source;
    uint32_t rev;
} prisma_xy_link_state_t;

void prisma_xy_link_init(void);
void prisma_xy_link_get(prisma_xy_link_state_t *out);
void prisma_xy_link_set_lock(prisma_xy_channel_t source, bool on);
void prisma_xy_link_set_random(bool on);
void prisma_xy_link_apply_input(prisma_xy_channel_t source, float x, float y);
