#include "prisma_platform.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

#define PRISMA_DEFAULT_BRIGHTNESS 80U
#define PRISMA_NVS_NAMESPACE "prisma"
#define PRISMA_KEY_BRIGHTNESS "bright"
#define PRISMA_KEY_LAST_VIEW "view"

static const char *TAG = "prisma_platform";

static prisma_settings_t s_settings = {
    .brightness_percent = PRISMA_DEFAULT_BRIGHTNESS,
    .last_view = PRISMA_VIEW_HOME,
};

static nvs_handle_t s_nvs = 0;
static bool s_nvs_ready = false;

static bool commit_u8(const char *key, uint8_t value)
{
    if (!s_nvs_ready) {
        return false;
    }

    esp_err_t err = nvs_set_u8(s_nvs, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(s_nvs);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS write %s failed: %s", key, esp_err_to_name(err));
        return false;
    }

    return true;
}

bool prisma_platform_init(void)
{
    s_settings.brightness_percent = PRISMA_DEFAULT_BRIGHTNESS;
    s_settings.last_view = PRISMA_VIEW_HOME;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS requires erase/re-init");
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_open(PRISMA_NVS_NAMESPACE, NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return false;
    }

    s_nvs_ready = true;

    uint8_t brightness = PRISMA_DEFAULT_BRIGHTNESS;
    if (nvs_get_u8(s_nvs, PRISMA_KEY_BRIGHTNESS, &brightness) == ESP_OK &&
        brightness <= 100U) {
        s_settings.brightness_percent = brightness;
    } else {
        (void)commit_u8(PRISMA_KEY_BRIGHTNESS, PRISMA_DEFAULT_BRIGHTNESS);
    }

    uint8_t view = (uint8_t)PRISMA_VIEW_HOME;
    if (nvs_get_u8(s_nvs, PRISMA_KEY_LAST_VIEW, &view) == ESP_OK &&
        view <= (uint8_t)PRISMA_VIEW_SYSTEM) {
        s_settings.last_view = (prisma_view_t)view;
    } else {
        (void)commit_u8(PRISMA_KEY_LAST_VIEW, (uint8_t)PRISMA_VIEW_HOME);
    }

    ESP_LOGI(TAG,
             "Settings ready: brightness=%u last_view=%u",
             (unsigned)s_settings.brightness_percent,
             (unsigned)s_settings.last_view);
    return true;
}

prisma_settings_t prisma_platform_get_settings(void)
{
    return s_settings;
}

bool prisma_platform_set_brightness(uint8_t percent)
{
    if (percent > 100U) {
        percent = 100U;
    }

    s_settings.brightness_percent = percent;
    return !s_nvs_ready || commit_u8(PRISMA_KEY_BRIGHTNESS, percent);
}

bool prisma_platform_set_last_view(prisma_view_t view)
{
    if (view < PRISMA_VIEW_HOME || view > PRISMA_VIEW_SYSTEM) {
        return false;
    }

    s_settings.last_view = view;
    return !s_nvs_ready || commit_u8(PRISMA_KEY_LAST_VIEW, (uint8_t)view);
}

bool prisma_platform_reset_settings(void)
{
    s_settings.brightness_percent = PRISMA_DEFAULT_BRIGHTNESS;
    s_settings.last_view = PRISMA_VIEW_HOME;

    if (!s_nvs_ready) {
        return true;
    }

    esp_err_t err = nvs_erase_all(s_nvs);
    if (err == ESP_OK) {
        err = nvs_commit(s_nvs);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS reset failed: %s", esp_err_to_name(err));
        return false;
    }

    bool ok = commit_u8(PRISMA_KEY_BRIGHTNESS, PRISMA_DEFAULT_BRIGHTNESS);
    ok = commit_u8(PRISMA_KEY_LAST_VIEW, (uint8_t)PRISMA_VIEW_HOME) && ok;
    return ok;
}

uint32_t prisma_platform_uptime_seconds(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000LL);
}

uint32_t prisma_platform_free_heap_bytes(void)
{
    return (uint32_t)esp_get_free_heap_size();
}
