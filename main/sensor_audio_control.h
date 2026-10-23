#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    PRISMA_SCALE_CHROMATIC = 0,
    PRISMA_SCALE_MAJOR,
    PRISMA_SCALE_MINOR,
    PRISMA_SCALE_DIMINISHED,
    PRISMA_SCALE_DORIAN,
    PRISMA_SCALE_WHOLE_TONE,
    PRISMA_SCALE_COUNT
} prisma_scale_t;

typedef enum {
    PRISMA_WAVE_SINE = 0,
    PRISMA_WAVE_SQUARE,
    PRISMA_WAVE_TRIANGLE,
    PRISMA_WAVE_SAW,
    PRISMA_WAVE_PULSE,
    PRISMA_WAVE_NOISE,
    PRISMA_WAVE_COUNT
} prisma_waveform_t;

typedef struct {
    bool enabled;
    bool active;
    bool face_down;
    bool speaker_enabled;
    int volume;
    int voices;
    float pitch_hz;
    float angle_x;
    float angle_y;
    float angle_z;
    int root;
    prisma_scale_t scale;
    prisma_waveform_t waveform;
    int midi_note;
    char note_name[8];
} prisma_sensor_audio_state_t;

esp_err_t prisma_sensor_audio_start(void);
esp_err_t prisma_sensor_audio_set_enabled(bool enabled);
esp_err_t prisma_sensor_audio_set_volume(int volume);
esp_err_t prisma_sensor_audio_set_root(int root);
esp_err_t prisma_sensor_audio_set_scale(prisma_scale_t scale);
esp_err_t prisma_sensor_audio_set_waveform(prisma_waveform_t waveform);
esp_err_t prisma_sensor_audio_set_speaker(bool enabled);
const char *prisma_sensor_audio_root_name(int root);
const char *prisma_sensor_audio_scale_name(prisma_scale_t scale);
const char *prisma_sensor_audio_waveform_name(prisma_waveform_t waveform);
void prisma_sensor_audio_get_state(prisma_sensor_audio_state_t *out);
