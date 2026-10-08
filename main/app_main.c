#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "prisma_bsp.h"

static const char *TAG = "prisma_boot";

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8U) << 8) |
                      ((g & 0xFCU) << 3) |
                      ((b & 0xF8U) >> 3));
}

void app_main(void)
{
    ESP_LOGI(TAG, "PRISMA-PI Lite ESP32-S3");
    ESP_LOGI(TAG, "Milestone M1: BSP hardware bring-up");

    bool board_ok = prisma_bsp_init();
    prisma_bsp_status_t status = prisma_bsp_get_status();

    if (status.display_ready) {
        /* Dark PRISMA boot surface: a visible display/backlight proof. */
        prisma_bsp_display_fill(rgb565(10, 6, 20));
    }

    if (!board_ok) {
        ESP_LOGE(TAG,
                 "M1 incomplete: display=%d touch=%d",
                 status.display_ready,
                 status.touch_ready);
    } else {
        ESP_LOGI(TAG, "M1 ready: touch the screen to exercise the input path");
    }

    bool was_pressed = false;
    uint32_t touch_count = 0;

    while (true) {
        prisma_bsp_touch_sample_t touch = {0};
        bool read_ok = prisma_bsp_touch_read(&touch);

        if (read_ok && touch.pressed && !was_pressed) {
            ++touch_count;

            /* Alternate two unmistakable surfaces on each new touch. */
            uint16_t color = (touch_count & 1U)
                                 ? rgb565(14, 52, 62)
                                 : rgb565(48, 12, 62);
            prisma_bsp_display_fill(color);

            ESP_LOGI(TAG,
                     "Touch #%lu x=%u y=%u",
                     (unsigned long)touch_count,
                     (unsigned)touch.x,
                     (unsigned)touch.y);
        }

        was_pressed = read_ok && touch.pressed;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
