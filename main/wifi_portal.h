#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool has_on;
    bool on;
    bool has_mode;
    int mode;
    bool has_intensity;
    int intensity;
    bool source_next;
    uint32_t seq;
} prisma_page2_command_t;

void prisma_wifi_start(void);
bool prisma_control_get_on(void);
int prisma_control_get_mode(void);
int prisma_control_get_intensity(void);
int prisma_control_get_generation(void);
void prisma_control_set_on(bool on);
void prisma_control_set_mode(int mode);
void prisma_control_set_intensity(int intensity);
void prisma_control_set_generation(int generation);
void prisma_control_set_filename(const char *filename);
void prisma_control_set_effect(int effect, bool enabled);
bool prisma_control_get_effect(int effect);
void prisma_control_set_effect_value(int effect, int value);
int prisma_control_get_effect_value(int effect);
bool prisma_control_take_page2_command(prisma_page2_command_t *out);
