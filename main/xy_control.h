#pragma once
#include <stdint.h>

typedef enum {
    PRISMA_XY_FRACTAL = 0,
    PRISMA_XY_AUDIO = 1,
    PRISMA_XY_CHANNEL_COUNT
} prisma_xy_channel_t;

typedef struct {
    float x;
    float y;
    uint32_t rev;
} prisma_xy_state_t;

void prisma_xy_init(void);
void prisma_xy_get(prisma_xy_channel_t channel, prisma_xy_state_t *out);
void prisma_xy_set(prisma_xy_channel_t channel, float x, float y);
