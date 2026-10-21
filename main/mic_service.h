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
