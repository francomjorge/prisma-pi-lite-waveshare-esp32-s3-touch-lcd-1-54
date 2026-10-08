#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PRISMA_EFFECT_PIXELATE = 0,
    PRISMA_EFFECT_THRESHOLD,
    PRISMA_EFFECT_POSTERIZE,
    PRISMA_EFFECT_MIRROR,
    PRISMA_EFFECT_RANDOM,
    PRISMA_EFFECT_MIX,
    PRISMA_EFFECT_COUNT,
} prisma_effect_id_t;

typedef struct {
    bool enabled;
    uint8_t value;
} prisma_effect_state_t;

typedef struct {
    uint32_t revision;
    uint8_t file_index;
    char file_name[24];
    uint8_t intensity;
    bool bypass;
    prisma_effect_state_t effects[PRISMA_EFFECT_COUNT];
} prisma_synth_state_t;

bool prisma_app_init(void);
prisma_synth_state_t prisma_app_get_state(void);
void prisma_app_cycle_file(void);
void prisma_app_set_intensity(uint8_t value);
void prisma_app_toggle_bypass(void);
void prisma_app_set_bypass(bool bypass);
void prisma_app_toggle_effect(prisma_effect_id_t effect);
void prisma_app_set_effect_enabled(prisma_effect_id_t effect, bool enabled);
void prisma_app_set_effect_value(prisma_effect_id_t effect, uint8_t value);
