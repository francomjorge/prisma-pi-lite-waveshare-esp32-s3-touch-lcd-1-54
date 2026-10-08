#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "prisma_app.h"
#include "prisma_bsp.h"
#include "prisma_network.h"
#include "prisma_platform.h"
#include "prisma_ui.h"

static const char *TAG = "prisma_boot";

void app_main(void)
{
    ESP_LOGI(TAG, "PRISMA-PI Lite ESP32-S3");
    ESP_LOGI(TAG, "Milestone M4.2a: appliance intro + continuous swipe tracking");

    bool platform_ok = prisma_platform_init();
    if (!platform_ok) {
        ESP_LOGW(TAG, "Persistent settings unavailable; using runtime defaults");
    }

    (void)prisma_app_init();

    bool board_ok = prisma_bsp_init();
    prisma_bsp_status_t status = prisma_bsp_get_status();

    bool network_ok = prisma_network_init();
    if (!network_ok) {
        ESP_LOGW(TAG, "Local network unavailable; appliance operation continues");
    }

    if (status.display_ready) {
        prisma_ui_init();
    }

    if (!board_ok) {
        ESP_LOGE(TAG,
                 "Hardware incomplete: display=%d touch=%d",
                 status.display_ready,
                 status.touch_ready);
    } else {
        ESP_LOGI(TAG, "Appliance ready: INTRO / NOW PLAYING / BROWSER / CORE EXTENDED");
    }

    bool was_pressed = false;
    uint32_t touch_count = 0;
    uint8_t last_clients = 255;
    bool last_ready = false;

    while (true) {
        prisma_bsp_touch_sample_t touch = {0};
        bool read_ok = prisma_bsp_touch_read(&touch);

        if (read_ok) {
            if (touch.pressed && !was_pressed) {
                ++touch_count;
                ESP_LOGI(TAG,
                         "Touch #%lu begin x=%u y=%u",
                         (unsigned long)touch_count,
                         (unsigned)touch.x,
                         (unsigned)touch.y);
            }

            if (status.display_ready) {
                prisma_ui_handle_touch_sample(touch.pressed, touch.x, touch.y);
            }

            was_pressed = touch.pressed;
        }

        if (status.display_ready) {
            prisma_ui_tick();
        }

        if (network_ok) {
            prisma_network_status_t net = prisma_network_get_status();
            if (net.local_ready != last_ready || net.clients != last_clients) {
                ESP_LOGI(TAG,
                         "Local network: ready=%d ssid=%s ip=%s clients=%u",
                         net.local_ready,
                         net.ssid,
                         net.ip,
                         (unsigned)net.clients);
                last_ready = net.local_ready;
                last_clients = net.clients;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
