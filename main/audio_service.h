#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    bool initialized;
    bool es8311_present;
    bool es7210_present;
    bool playing;
    bool tone;
    int volume;
    int saved_volume;
    int source_rate;
    int source_channels;
    int source_bits;
    int position_ms;
    int duration_ms;
    char selected_sample[64];
    char playing_name[64];
    char last_error[96];
} prisma_audio_status_t;

esp_err_t prisma_audio_init(i2c_master_bus_handle_t i2c_bus);
void prisma_audio_get_status(prisma_audio_status_t *out);
esp_err_t prisma_audio_preview_volume(int volume);
esp_err_t prisma_audio_set_volume(int volume);
esp_err_t prisma_audio_play_selected(void);
esp_err_t prisma_audio_play_tone(void);
void prisma_audio_stop(void);
