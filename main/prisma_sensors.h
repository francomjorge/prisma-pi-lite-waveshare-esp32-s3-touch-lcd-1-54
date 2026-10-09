#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

typedef struct {
    int16_t acc_x;
    int16_t acc_y;
    int16_t acc_z;
    int16_t gyr_x;
    int16_t gyr_y;
    int16_t gyr_z;
    float angle_x;
    float angle_y;
    float angle_z;
} prisma_imu_data_t;

bool prisma_sensors_init(i2c_master_bus_handle_t bus);
bool prisma_sensors_imu_ready(void);
bool prisma_sensors_read_imu(prisma_imu_data_t *data);
