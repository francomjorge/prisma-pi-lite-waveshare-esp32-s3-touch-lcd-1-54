#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "prisma_bsp.h"
#include "prisma_platform.h"
#include "prisma_ui.h"

static const char *TAG = "prisma_boot";

void app_main(void)
{
    ESP_LOGI(TAG, "PRISMA-PI Lite ESP32-S3");
    ESP_LOGI(TAG, "Milestone M3: interactive shell + persistence");

    bool platform_ok = prisma_platform_init();
    if (!platform_ok) {
        ESP_LOGW(TAG, "Persistent settings unavailable; using runtime defaults");
    }

    bool board_ok = prisma_bsp_init();
    prisma_bsp_status_t status = prisma_bsp_get_status();

    if (status.display_ready) {
        prisma_ui_init();
    }

    if (!board_ok) {
        ESP_LOGE(TAG,
                 "Hardware incomplete: display=%d touch=%d",
                 status.display_ready,
                 status.touch_ready);
    } else {
        ESP_LOGI(TAG, "M3 ready: HOME / INPUT / OUTPUT / SYSTEM + diagnostics");
    }

    bool was_pressed = false;
    uint32_t touch_count = 0;

    while (true) {
        prisma_bsp_touch_sample_t touch = {0};
        bool read_ok = prisma_bsp_touch_read(&touch);

        if (read_ok && touch.pressed && !was_pressed) {
            ++touch_count;
            ESP_LOGI(TAG,
                     "Touch #%lu x=%u y=%u",
                     (unsigned long)touch_count,
                     (unsigned)touch.x,
                     (unsigned)touch.y);

            if (status.display_ready) {
                prisma_ui_handle_touch(touch.x, touch.y);
            }
        }

        was_pressed = read_ok && touch.pressed;

        if (status.display_ready) {
            prisma_ui_tick();
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
