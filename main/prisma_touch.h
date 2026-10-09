#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

bool prisma_touch_init(void);
bool prisma_touch_read(uint16_t *x, uint16_t *y);
i2c_master_bus_handle_t prisma_touch_i2c_bus(void);
