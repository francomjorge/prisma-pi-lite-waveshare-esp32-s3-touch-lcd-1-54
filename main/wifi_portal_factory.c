#include "wifi_portal.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "admin_api.h"
#include "admin_shell.h"
#include "audio_api.h"
#include "bluetooth_api.h"
#include "bluetooth_service.h"
#include "files_plus_api.h"
#include "mic_api.h"
#include "network_service.h"
#include "playback_http.h"
#include "security_service.h"

static const char *TAG = "ADMIN_WIFI";

static esp_err_t root_get(httpd_req_t *r)
{
    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", "/admin");
    return httpd_resp_send(r, NULL, 0);
}

static void ble_start_task(void *arg)
{
    (void)arg;

    /* FACTORY AUDIO V1 proved the Wi-Fi/admin path first. Keep that contract:
     * let HTTP settle before allocating the NimBLE host/controller resources. */
    vTaskDelay(pdMS_TO_TICKS(1200));

    esp_err_t ble = prisma_bluetooth_init();
    if (ble != ESP_OK) ESP_LOGW(TAG, "BLE init failed after admin startup: %s", esp_err_to_name(ble));
    else ESP_LOGI(TAG, "BLE init started after admin startup");

    vTaskDelete(NULL);
}

void prisma_wifi_start(void)
{
    esp_err_t net = prisma_network_init();
    if (net != ESP_OK) {
        ESP_LOGE(TAG, "network init failed: %s", esp_err_to_name(net));
        return;
    }

    esp_err_t sec = prisma_security_init();
    if (sec != ESP_OK) {
        ESP_LOGE(TAG, "security init failed: %s", esp_err_to_name(sec));
        return;
    }

    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.uri_match_fn = httpd_uri_match_wildcard;
    hc.lru_purge_enable = true;
    hc.max_uri_handlers = 64;
    hc.stack_size = 12288;

    httpd_handle_t server = NULL;
    esp_err_t http = httpd_start(&server, &hc);
    if (http != ESP_OK) {
        ESP_LOGE(TAG, "HTTP admin start failed: %s", esp_err_to_name(http));
        return;
    }

    httpd_uri_t root = {.uri="/",.method=HTTP_GET,.handler=root_get};
    httpd_register_uri_handler(server, &root);

    /* Preserve the already validated FACTORY AUDIO V1 admin/API surface.
     * Bluetooth is additive: its API and shell extension are registered only
     * after the same core routes are available. */
    prisma_playback_http_register(server);
    prisma_admin_shell_register(server);
    prisma_admin_register(server);
    prisma_files_plus_register(server);
    prisma_audio_api_register(server);
    prisma_mic_api_register(server);
    prisma_bluetooth_api_register(server);

    prisma_network_status_t status;
    prisma_network_get_status(&status);
    ESP_LOGI(TAG, "%s admin ready at http://%s/admin",
             status.ap_ssid, status.ap_ip[0] ? status.ap_ip : "192.168.4.1");

    /* BLE must never be able to prevent the rescue AP/admin page from coming
     * up. Initialize it in a separate, delayed task after HTTP is live. */
    if (xTaskCreate(ble_start_task, "ble_start", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG, "BLE deferred task allocation failed; admin remains available");
    }
}
