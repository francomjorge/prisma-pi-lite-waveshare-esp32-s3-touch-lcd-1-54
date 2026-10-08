#include "esp_log.h"

static const char *TAG = "prisma_boot";

void app_main(void)
{
    /* M0 composition root only. Real hardware initialization starts in M1. */
    ESP_LOGI(TAG, "PRISMA-PI Lite ESP32-S3");
    ESP_LOGI(TAG, "Milestone M0: architecture and build spine");
}
