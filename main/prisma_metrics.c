#include "prisma_metrics.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#define BAT_ADC_UNIT ADC_UNIT_1
#define BAT_ADC_CHANNEL ADC_CHANNEL_0
#define BAT_ADC_ATTEN ADC_ATTEN_DB_12
#define BAT_DIVIDER_X1000 3000

static const char *TAG = "PRISMA_METRICS";
static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static bool adc_ready;
static bool cali_ready;

static int battery_percent_from_mv(int mv)
{
    if (mv >= 4200) return 100;
    if (mv <= 3300) return 0;
    if (mv >= 4100) return 90 + (mv - 4100) * 10 / 100;
    if (mv >= 4000) return 80 + (mv - 4000) * 10 / 100;
    if (mv >= 3900) return 65 + (mv - 3900) * 15 / 100;
    if (mv >= 3800) return 50 + (mv - 3800) * 15 / 100;
    if (mv >= 3700) return 30 + (mv - 3700) * 20 / 100;
    if (mv >= 3600) return 15 + (mv - 3600) * 15 / 100;
    if (mv >= 3400) return 5 + (mv - 3400) * 10 / 200;
    return (mv - 3300) * 5 / 100;
}

void prisma_metrics_init(void)
{
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
    ESP_LOGI(TAG, "Battery ADC ready%s", cali_ready ? " (calibrated)" : " (raw fallback)");
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
