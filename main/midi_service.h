#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool host_ready;
    bool usb_present;
    bool midi_connected;
    uint16_t vid;
    uint16_t pid;
    bool play_sound;
    bool selected_file_present;
    char sound_file[64];
    uint32_t revision;
} prisma_midi_status_t;

void prisma_midi_init(void);
void prisma_midi_get_status(prisma_midi_status_t *out);
bool prisma_midi_set_config(bool play_sound, const char *sound_file);
void prisma_midi_on_file_deleted(const char *name);
