#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PRISMA_FW_VERSION "0.3.0"

typedef enum {
    PRISMA_VIEW_HOME = 0,
    PRISMA_VIEW_INPUT,
    PRISMA_VIEW_OUTPUT,
    PRISMA_VIEW_SYSTEM,
    PRISMA_VIEW_DIAGNOSTICS,
} prisma_view_t;

typedef struct {
    uint8_t brightness_percent;
    prisma_view_t last_view;
} prisma_settings_t;

bool prisma_platform_init(void);
prisma_settings_t prisma_platform_get_settings(void);
bool prisma_platform_set_brightness(uint8_t percent);
bool prisma_platform_set_last_view(prisma_view_t view);
bool prisma_platform_reset_settings(void);
uint32_t prisma_platform_uptime_seconds(void);
uint32_t prisma_platform_free_heap_bytes(void);
