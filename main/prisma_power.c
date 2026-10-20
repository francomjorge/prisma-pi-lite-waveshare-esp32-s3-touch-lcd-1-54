#include "prisma_power.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"

#define POWER_KEY_PIN GPIO_NUM_5
#define BAT_EN_PIN GPIO_NUM_2
#define LCD_BL_PIN GPIO_NUM_46

#define POWER_POLL_MS 20
#define RESTART_HOLD_MS 2000
#define POWER_ON_HOLD_MS 3000
#define POWER_OFF_HOLD_MS 5000

static const char *TAG = "PRISMA_POWER";

static bool power_key_pressed(void)
{
    return gpio_get_level(POWER_KEY_PIN) == 0;
}

static uint32_t held_ms(TickType_t start)
{
    return (uint32_t)((xTaskGetTickCount() - start) * portTICK_PERIOD_MS);
}

static void wait_for_release(void)
{
    while (power_key_pressed()) {
        vTaskDelay(pdMS_TO_TICKS(POWER_POLL_MS));
    }
}

static void wait_for_power_on_over_usb(void)
{
    bool down = false;
    TickType_t started = 0;

    wait_for_release();
    ESP_LOGI(TAG, "OFF state active; hold PWR for 3 s to power on");

    while (1) {
        bool pressed = power_key_pressed();
        if (pressed && !down) {
            down = true;
            started = xTaskGetTickCount();
        } else if (pressed && down && held_ms(started) >= POWER_ON_HOLD_MS) {
            ESP_LOGI(TAG, "PWR 3 s -> POWER ON");
            gpio_set_level(BAT_EN_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            esp_restart();
        } else if (!pressed) {
            down = false;
        }
        vTaskDelay(pdMS_TO_TICKS(POWER_POLL_MS));
    }
}

static void enter_power_off_state(bool runtime)
{
    ESP_LOGI(TAG, "PWR 5 s -> POWER OFF");

    if (runtime) {
        gpio_set_level(LCD_BL_PIN, 0);
        esp_err_t err = esp_wifi_stop();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
            ESP_LOGW(TAG, "esp_wifi_stop: %s", esp_err_to_name(err));
        }
    }

    /*
     * On battery power this drops the hardware latch and the board turns off
     * when the PWR key is released. With USB still attached the MCU remains
     * powered, so we emulate the same OFF state and wait for a new 3 s hold.
     */
    gpio_set_level(BAT_EN_PIN, 0);
    wait_for_power_on_over_usb();
}

static void require_long_hold_on_battery_start(void)
{
    if (!power_key_pressed()) {
        return;
    }

    TickType_t started = xTaskGetTickCount();
    ESP_LOGI(TAG, "PWR held at boot; validating 3 s power-on hold");

    while (power_key_pressed()) {
        if (held_ms(started) >= POWER_ON_HOLD_MS) {
            ESP_LOGI(TAG, "PWR 3 s -> startup accepted");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(POWER_POLL_MS));
    }

    ESP_LOGI(TAG, "PWR released before 3 s -> remain OFF");
    gpio_set_level(BAT_EN_PIN, 0);
    wait_for_power_on_over_usb();
}

static void power_button_task(void *arg)
{
    (void)arg;
    bool down = false;
    TickType_t started = 0;

    /* Ignore the tail of the press that powered the appliance on. */
    wait_for_release();

    while (1) {
        bool pressed = power_key_pressed();

        if (pressed && !down) {
            down = true;
            started = xTaskGetTickCount();
        } else if (pressed && down && held_ms(started) >= POWER_OFF_HOLD_MS) {
            enter_power_off_state(true);
        } else if (!pressed && down) {
            uint32_t duration = held_ms(started);
            down = false;

            /*
             * Restart is intentionally decided on release. A continuous 5 s
             * hold therefore reaches POWER OFF without restarting at 2 s.
             */
            if (duration >= RESTART_HOLD_MS && duration < POWER_OFF_HOLD_MS) {
                ESP_LOGI(TAG, "PWR %lu ms -> RESTART", (unsigned long)duration);
                vTaskDelay(pdMS_TO_TICKS(80));
                esp_restart();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POWER_POLL_MS));
    }
}

void prisma_power_init(void)
{
    gpio_config_t bat = {
        .pin_bit_mask = 1ULL << BAT_EN_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bat));
    ESP_ERROR_CHECK(gpio_set_level(BAT_EN_PIN, 1));

    gpio_config_t key = {
        .pin_bit_mask = 1ULL << POWER_KEY_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&key));

    require_long_hold_on_battery_start();

    BaseType_t ok = xTaskCreate(power_button_task, "prisma_power", 3072, NULL, 6, NULL);
    ESP_ERROR_CHECK(ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "Power control ready: 2 s + release = restart, 5 s hold = off, 3 s hold = on");
}
