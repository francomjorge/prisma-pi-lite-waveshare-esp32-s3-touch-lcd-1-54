#include "prisma_metrics.h"

#include "driver/gpio.h"
#include "driver/temperature_sensor.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_psram.h"

/* Waveshare ESP32-S3-Touch-LCD-1.54 factory pin mapping. */
#define BAT_ADC_UNIT ADC_UNIT_1
#define BAT_ADC_CHANNEL ADC_CHANNEL_0  /* GPIO1 */
#define BAT_ADC_ATTEN ADC_ATTEN_DB_12
#define BAT_DIVIDER_X1000 3000
#define BAT_CHARGING_PIN GPIO_NUM_3

static const char *TAG = "PRISMA_METRICS";
static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static bool adc_ready;
static bool cali_ready;
static bool charging_gpio_ready;
static temperature_sensor_handle_t temp_handle;
static bool temp_ready;

/* Same level thresholds used by Waveshare's ESP-IDF 5.5.1 factory BSP. */
static int battery_percent_from_mv(int mv)
{
    if (mv < 3520) return 1;
    if (mv < 3640) return 20;
    if (mv < 3760) return 40;
    if (mv < 3880) return 60;
    if (mv < 4000) return 80;
    return 100;
}

void prisma_metrics_init(void)
{
    if (!charging_gpio_ready) {
        gpio_config_t charging = {
            .pin_bit_mask = 1ULL << BAT_CHARGING_PIN,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        charging_gpio_ready = gpio_config(&charging) == ESP_OK;
    }

    if (!temp_ready) {
        temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
        if (temperature_sensor_install(&cfg, &temp_handle) == ESP_OK &&
            temperature_sensor_enable(temp_handle) == ESP_OK) {
            temp_ready = true;
        } else {
            ESP_LOGW(TAG, "Chip temperature sensor unavailable");
        }
    }

    if (adc_ready) return;

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BAT_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&unit_cfg, &adc_handle) != ESP_OK) {
        ESP_LOGW(TAG, "Battery ADC unit unavailable");
        return;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_oneshot_config_channel(adc_handle, BAT_ADC_CHANNEL, &chan_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "Battery ADC channel unavailable");
        return;
    }

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BAT_ADC_UNIT,
        .chan = BAT_ADC_CHANNEL,
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    cali_ready = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle) == ESP_OK;
    adc_ready = true;
    ESP_LOGI(TAG, "Waveshare battery ADC ready%s", cali_ready ? " (calibrated)" : " (raw fallback)");
}

bool prisma_metrics_battery(int *millivolts, int *percent)
{
    if (!millivolts || !percent) return false;
    if (!adc_ready) prisma_metrics_init();
    if (!adc_ready) return false;

    int raw_sum = 0;
    int samples = 0;
    for (int i = 0; i < 12; i++) {
        int raw = 0;
        if (adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &raw) == ESP_OK) {
            raw_sum += raw;
            samples++;
        }
    }
    if (!samples) return false;

    int raw = raw_sum / samples;
    int pin_mv = 0;
    if (!cali_ready || adc_cali_raw_to_voltage(cali_handle, raw, &pin_mv) != ESP_OK) {
        pin_mv = raw * 3300 / 4095;
    }

    int batt_mv = pin_mv * BAT_DIVIDER_X1000 / 1000;
    if (batt_mv < 2500 || batt_mv > 5000) return false;

    *millivolts = batt_mv;
    *percent = battery_percent_from_mv(batt_mv);
    return true;
}

bool prisma_metrics_charging(bool *charging)
{
    if (!charging) return false;
    if (!charging_gpio_ready) prisma_metrics_init();
    if (!charging_gpio_ready) return false;
    int mv = 0, pct = 0;
    if (prisma_metrics_battery(&mv, &pct) && pct >= 100) {
        *charging = false;
        return true;
    }
    *charging = gpio_get_level(BAT_CHARGING_PIN) == 0;
    return true;
}

bool prisma_metrics_chip_temperature(float *celsius)
{
    if (!celsius) return false;
    if (!temp_ready) prisma_metrics_init();
    if (!temp_ready) return false;
    return temperature_sensor_get_celsius(temp_handle, celsius) == ESP_OK;
}

uint32_t prisma_metrics_flash_size(void)
{
    uint32_t size = 0;
    return esp_flash_get_size(NULL, &size) == ESP_OK ? size : 0;
}

uint32_t prisma_metrics_psram_size(void)
{
    return (uint32_t)esp_psram_get_size();
}
