#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    bool enabled;
    bool active;
    bool face_down;
    int volume;
    float pitch_hz;
    float angle_x;
    float angle_y;
    float angle_z;
} prisma_sensor_audio_state_t;

esp_err_t prisma_sensor_audio_start(void);
esp_err_t prisma_sensor_audio_set_enabled(bool enabled);
esp_err_t prisma_sensor_audio_set_volume(int volume);
void prisma_sensor_audio_get_state(prisma_sensor_audio_state_t *out);
