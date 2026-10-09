#pragma once

#include <stdbool.h>
#include <stdint.h>

void prisma_metrics_init(void);
bool prisma_metrics_battery(int *millivolts, int *percent);
