#pragma once

#include <stdbool.h>
#include <stdint.h>

void prisma_metrics_init(void);
bool prisma_metrics_battery(int *millivolts, int *percent);
bool prisma_metrics_charging(bool *charging);
bool prisma_metrics_chip_temperature(float *celsius);
uint32_t prisma_metrics_flash_size(void);
uint32_t prisma_metrics_psram_size(void);
