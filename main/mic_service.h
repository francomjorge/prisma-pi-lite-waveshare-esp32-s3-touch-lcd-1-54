#pragma once

#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    bool initialized;
    bool recording;
    int level;
    int peak;
    int gain_db;
    int record_ms;
    int record_target_ms;
    char record_name[64];
    char last_error[96];
} prisma_mic_status_t;

esp_err_t prisma_mic_init(i2c_master_bus_handle_t i2c_bus);
void prisma_mic_get_status(prisma_mic_status_t *out);
esp_err_t prisma_mic_record_start(int seconds);
void prisma_mic_record_stop(void);
/* Factory Audio V1.2 shared-I2S arbitration. Playback temporarily disables
 * ES7210 RX; after speaker output completes the live microphone is resumed. */
esp_err_t prisma_mic_pause_for_playback(void);
esp_err_t prisma_mic_resume_after_playback(void);
