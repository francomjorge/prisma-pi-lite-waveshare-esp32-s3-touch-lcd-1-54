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

#define BUTTON_DEBOUNCE_SAMPLES 2U

static const char *TAG = "prisma_boot";

typedef struct {
    bool candidate;
    bool stable;
    uint8_t samples;
} button_debounce_t;

static bool button_pressed_edge(button_debounce_t *state, bool raw_pressed)
{
    if (raw_pressed == state->candidate) {
        if (state->samples < 255U) {
            ++state->samples;
        }
    } else {
        state->candidate = raw_pressed;
        state->samples = 1U;
    }

    if (state->samples >= BUTTON_DEBOUNCE_SAMPLES && state->stable != state->candidate) {
        state->stable = state->candidate;
        return state->stable;
    }

    return false;
}

void app_main(void)
{
    ESP_LOGI(TAG, "PRISMA-PI Lite ESP32-S3");
    ESP_LOGI(TAG, "Milestone M4.2b: T1/T2 navigation + independent touch gestures");

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
                 "Hardware incomplete: display=%d touch=%d buttons=%d",
                 status.display_ready,
                 status.touch_ready,
                 status.buttons_ready);
    } else {
        ESP_LOGI(TAG, "Input framework ready: T1=previous T2=next swipe=gesture-only");
    }

    bool touch_was_pressed = false;
    uint32_t touch_count = 0;
    uint8_t last_clients = 255;
    bool last_ready = false;
    button_debounce_t t1 = {0};
    button_debounce_t t2 = {0};

    while (true) {
        prisma_bsp_button_state_t buttons = {0};
        if (prisma_bsp_buttons_read(&buttons)) {
            bool t1_edge = button_pressed_edge(&t1, buttons.t1_pressed);
            bool t2_edge = button_pressed_edge(&t2, buttons.t2_pressed);

            if (t1_edge && !t2.stable) {
                ESP_LOGI(TAG, "T1: previous page");
                prisma_ui_page_previous();
            } else if (t2_edge && !t1.stable) {
                ESP_LOGI(TAG, "T2: next page");
                prisma_ui_page_next();
            }
        }

        prisma_bsp_touch_sample_t touch = {0};
        bool read_ok = prisma_bsp_touch_read(&touch);

        if (read_ok) {
            if (touch.pressed && !touch_was_pressed) {
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
            touch_was_pressed = touch.pressed;
        } else if (touch_was_pressed) {
            if (status.display_ready) {
                prisma_ui_handle_touch_sample(false, 0, 0);
            }
            touch_was_pressed = false;
        }

        if (status.display_ready) {
            prisma_ui_tick();

            prisma_ui_gesture_t gesture = PRISMA_UI_GESTURE_NONE;
            if (prisma_ui_take_gesture(&gesture)) {
                ESP_LOGI(TAG,
                         "Swipe detected: %s (reserved; page navigation stays on T1/T2)",
                         gesture == PRISMA_UI_GESTURE_SWIPE_LEFT ? "LEFT" : "RIGHT");
            }
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
