#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "power_button.h"

#define POWER_KEY_PIN GPIO_NUM_5
#define BAT_EN_PIN GPIO_NUM_2
#define LCD_BL_PIN GPIO_NUM_46

#define POWER_SAMPLE_MS 20
#define POWER_DEBOUNCE_SAMPLES 3
#define POWER_REBOOT_MS 2000
#define POWER_OFF_MS 3000
#define POWER_BOOT_HOLD_MS 3000

static const char *TAG = "POWER";
static bool s_boot_key_consumed = false;

static bool power_key_pressed(void)
{
    return gpio_get_level(POWER_KEY_PIN) == 0;
}

static void power_key_configure(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << POWER_KEY_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

static void battery_latch_set(bool enabled)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BAT_EN_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    ESP_ERROR_CHECK(gpio_set_level(BAT_EN_PIN, enabled ? 1 : 0));
}

static void backlight_force_off(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << LCD_BL_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    ESP_ERROR_CHECK(gpio_set_level(LCD_BL_PIN, 0));
}

static void enter_off_state(void)
{
    ESP_LOGI(TAG, "3s hold -> POWER OFF");
    backlight_force_off();

    /*
     * Arm GPIO5 as the wake source before releasing BAT_EN.
     * On battery power, releasing BAT_EN removes power after the key is
     * released. On USB power, deep sleep provides the same user-visible OFF
     * state and GPIO5 wakes the MCU for the 3-second power-on gate.
     */
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(POWER_KEY_PIN, 0));
    battery_latch_set(false);

    /* Do not immediately wake from deep sleep while the same press is held. */
    while (power_key_pressed()) {
        vTaskDelay(pdMS_TO_TICKS(POWER_SAMPLE_MS));
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    esp_deep_sleep_start();

    /* esp_deep_sleep_start() does not return. */
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

void prisma_power_prepare_boot(void)
{
    power_key_configure();

    /*
     * Normal resets/flashing arrive with the key released and boot exactly as
     * before. A cold/deep-sleep wake started by the physical power key must
     * keep GPIO5 low for 3 seconds before BAT_EN is latched.
     */
    if (!power_key_pressed()) {
        return;
    }

    s_boot_key_consumed = true;
    ESP_LOGI(TAG, "Power key detected at boot; hold 3s to power on");

    TickType_t start = xTaskGetTickCount();
    while (power_key_pressed()) {
        const uint32_t held_ms =
            (uint32_t)((xTaskGetTickCount() - start) * portTICK_PERIOD_MS);

        if (held_ms >= POWER_BOOT_HOLD_MS) {
            battery_latch_set(true);
            ESP_LOGI(TAG, "3s startup hold accepted -> POWER ON");
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(POWER_SAMPLE_MS));
    }

    /*
     * Short press while the appliance was OFF: do not latch power.
     * Battery operation will collapse here; USB operation enters deep sleep.
     */
    ESP_LOGI(TAG, "Startup hold shorter than 3s -> remain OFF");
    backlight_force_off();
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(POWER_KEY_PIN, 0));
    battery_latch_set(false);
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_deep_sleep_start();

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void power_button_task(void *arg)
{
    (void)arg;

    /*
     * If this boot was initiated by the power key, ignore that same press
     * until it is released. Otherwise the successful 3-second ON press would
     * immediately be interpreted as a 3-second OFF press.
     */
    if (s_boot_key_consumed) {
        while (power_key_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(POWER_SAMPLE_MS));
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }

    bool stable_pressed = false;
    bool last_raw = power_key_pressed();
    int stable_samples = 1;
    TickType_t press_start = 0;

    while (true) {
        const bool raw = power_key_pressed();

        if (raw == last_raw) {
            if (stable_samples < POWER_DEBOUNCE_SAMPLES) {
                stable_samples++;
            }
        } else {
            last_raw = raw;
            stable_samples = 1;
        }

        if (stable_samples >= POWER_DEBOUNCE_SAMPLES && raw != stable_pressed) {
            stable_pressed = raw;

            if (stable_pressed) {
                press_start = xTaskGetTickCount();
            } else if (press_start != 0) {
                const uint32_t held_ms =
                    (uint32_t)((xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS);
                press_start = 0;

                if (held_ms >= POWER_OFF_MS) {
                    enter_off_state();
                } else if (held_ms >= POWER_REBOOT_MS) {
                    ESP_LOGI(TAG, "2s hold/release -> RESTART");
                    vTaskDelay(pdMS_TO_TICKS(80));
                    esp_restart();
                }
            }
        }

        /*
         * Three seconds continuous hold powers OFF immediately. Reboot is
         * intentionally decided on release in the 2s..<3s window so both
         * gestures can coexist on one physical button.
         */
        if (stable_pressed && press_start != 0) {
            const uint32_t held_ms =
                (uint32_t)((xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS);
            if (held_ms >= POWER_OFF_MS) {
                enter_off_state();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POWER_SAMPLE_MS));
    }
}

void prisma_power_start_monitor(void)
{
    BaseType_t ok = xTaskCreate(
        power_button_task,
        "prisma_power",
        3072,
        NULL,
        8,
        NULL
    );
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
