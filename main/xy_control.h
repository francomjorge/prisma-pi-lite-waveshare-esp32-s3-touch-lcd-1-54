#pragma once
#include <stdbool.h>
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

typedef struct {
    bool locked;
    prisma_xy_channel_t source;
    uint32_t rev;
} prisma_xy_lock_state_t;

void prisma_xy_init(void);
void prisma_xy_get(prisma_xy_channel_t channel, prisma_xy_state_t *out);
void prisma_xy_set(prisma_xy_channel_t channel, float x, float y);
void prisma_xy_lock_get(prisma_xy_lock_state_t *out);
void prisma_xy_lock_set(bool locked, prisma_xy_channel_t source);
