#include "wifi_portal.h"

#include "esp_http_server.h"
#include "esp_log.h"

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

    /* A5 BLE is intentionally independent from Wi-Fi success after startup.
     * ESP32-S3 provides BLE only (no BR/EDR / Classic Bluetooth). */
    esp_err_t ble = prisma_bluetooth_init();
    if (ble != ESP_OK) ESP_LOGW(TAG, "BLE init failed: %s", esp_err_to_name(ble));
    else ESP_LOGI(TAG, "BLE init started");

    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.uri_match_fn = httpd_uri_match_wildcard;
    hc.lru_purge_enable = true;
    hc.max_uri_handlers = 64;
    hc.stack_size = 12288;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &hc) == ESP_OK) {
        httpd_uri_t root = {.uri="/",.method=HTTP_GET,.handler=root_get};
        httpd_register_uri_handler(server, &root);

        /* Register authoritative transport and the extensible /admin shell
         * before legacy handlers. Duplicate legacy registrations are rejected. */
        prisma_playback_http_register(server);
        prisma_admin_shell_register(server);
        prisma_admin_register(server);
        prisma_files_plus_register(server);
        prisma_audio_api_register(server);
        prisma_mic_api_register(server);
        prisma_bluetooth_api_register(server);
    }

    prisma_network_status_t status;
    prisma_network_get_status(&status);
    ESP_LOGI(TAG, "%s ready at http://%s/admin", status.ap_ssid, status.ap_ip[0] ? status.ap_ip : "192.168.4.1");
}
