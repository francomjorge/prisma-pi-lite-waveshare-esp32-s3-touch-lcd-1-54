#include "esp_err.h"
#include "esp_log.h"

#include "prisma_ui.h"
#include "waveshare_bsp.h"

static const char *TAG = "prisma_main";

void app_main(void)
{
    ESP_LOGI(TAG, "PRISMA-PI Lite ESP32-S3 boot");

    /* Power hold is deliberately first. GPIO2 keeps the battery rail enabled. */
    ESP_ERROR_CHECK(waveshare_bsp_power_hold_init());
    ESP_ERROR_CHECK(waveshare_bsp_display_init());

    ESP_LOGI(TAG, "hardware ready: display=%dx%d touch=%s",
             waveshare_bsp_display_width(),
             waveshare_bsp_display_height(),
             waveshare_bsp_touch_available() ? "yes" : "no");

    ESP_ERROR_CHECK(prisma_ui_start());
    ESP_LOGI(TAG, "Build 0.1 local vertical slice running");
}
