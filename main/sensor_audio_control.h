#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    PRISMA_SCALE_CHROMATIC = 0,
    PRISMA_SCALE_MAJOR = 1,
} prisma_scale_t;

typedef struct {
    bool enabled;
    bool active;
    bool face_down;
    int volume;
    int voices;
    float pitch_hz;
    float angle_x;
    float angle_y;
    float angle_z;
    int root;
    prisma_scale_t scale;
    int midi_note;
    char note_name[8];
} prisma_sensor_audio_state_t;

esp_err_t prisma_sensor_audio_start(void);
esp_err_t prisma_sensor_audio_set_enabled(bool enabled);
esp_err_t prisma_sensor_audio_set_volume(int volume);
esp_err_t prisma_sensor_audio_set_root(int root);
esp_err_t prisma_sensor_audio_set_scale(prisma_scale_t scale);
const char *prisma_sensor_audio_root_name(int root);
const char *prisma_sensor_audio_scale_name(prisma_scale_t scale);
void prisma_sensor_audio_get_state(prisma_sensor_audio_state_t *out);
