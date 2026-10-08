#include "prisma_network.h"

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"

#define PRISMA_LOCAL_AP_SSID "PRISMA-PI-LITE"
#define PRISMA_LOCAL_AP_CHANNEL 1
#define PRISMA_LOCAL_AP_MAX_CLIENTS 4

static const char *TAG = "prisma_network";

static bool s_initialized = false;
static httpd_handle_t s_http = NULL;
static esp_netif_t *s_ap_netif = NULL;
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;

static prisma_network_status_t s_status = {
    .mode = PRISMA_NETWORK_OFFLINE,
    .local_ready = false,
    .ap_active = false,
    .ssid = PRISMA_LOCAL_AP_SSID,
    .ip = "0.0.0.0",
    .clients = 0,
};

static void status_set(bool ready, bool active, const char *ip)
{
    portENTER_CRITICAL(&s_status_lock);
    s_status.mode = active ? PRISMA_NETWORK_LOCAL_AP : PRISMA_NETWORK_OFFLINE;
    s_status.local_ready = ready;
    s_status.ap_active = active;
    snprintf(s_status.ssid, sizeof(s_status.ssid), "%s", PRISMA_LOCAL_AP_SSID);
    snprintf(s_status.ip, sizeof(s_status.ip), "%s", ip ? ip : "0.0.0.0");
    portEXIT_CRITICAL(&s_status_lock);
}

static void status_client_delta(int delta)
{
    portENTER_CRITICAL(&s_status_lock);
    int value = (int)s_status.clients + delta;
    if (value < 0) value = 0;
    if (value > PRISMA_LOCAL_AP_MAX_CLIENTS) value = PRISMA_LOCAL_AP_MAX_CLIENTS;
    s_status.clients = (uint8_t)value;
    portEXIT_CRITICAL(&s_status_lock);
}

prisma_network_status_t prisma_network_get_status(void)
{
    prisma_network_status_t snapshot;
    portENTER_CRITICAL(&s_status_lock);
    snapshot = s_status;
    portEXIT_CRITICAL(&s_status_lock);
    return snapshot;
}

static void refresh_ap_ip(void)
{
    char ip[16] = "192.168.4.1";
    if (s_ap_netif != NULL) {
        esp_netif_ip_info_t info = {0};
        if (esp_netif_get_ip_info(s_ap_netif, &info) == ESP_OK) {
            snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
        }
    }
    status_set(true, true, ip);
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base != WIFI_EVENT) return;

    switch (event_id) {
        case WIFI_EVENT_AP_START:
            refresh_ap_ip();
            ESP_LOGI(TAG, "Local AP ready: SSID=%s IP=%s",
                     PRISMA_LOCAL_AP_SSID,
                     prisma_network_get_status().ip);
            break;
        case WIFI_EVENT_AP_STOP:
            status_set(false, false, "0.0.0.0");
            ESP_LOGW(TAG, "Local AP stopped");
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            status_client_delta(+1);
            ESP_LOGI(TAG, "Client joined local AP; clients=%u",
                     (unsigned)prisma_network_get_status().clients);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            status_client_delta(-1);
            ESP_LOGI(TAG, "Client left local AP; clients=%u",
                     (unsigned)prisma_network_get_status().clients);
            break;
        default:
            break;
    }
}

static esp_err_t root_handler(httpd_req_t *req)
{
    static const char page[] =
        "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>PRISMA-PI Lite</title><style>"
        "body{margin:0;background:#05070a;color:#f2f0f8;font-family:monospace;display:grid;place-items:center;min-height:100vh}"
        ".c{width:min(640px,88vw);border:1px solid #d9b64a;padding:32px;text-align:center;background:#090d12}"
        "h1{font-size:clamp(28px,7vw,58px);margin:0 0 8px}.p{color:#ff32b8}.i{color:#ffd83d}"
        ".m{color:#25dce0}.s{margin-top:26px;color:#9aa3b2;line-height:1.6}"
        "</style></head><body><div class='c'><h1>PRISMA-<span class='p'>P</span><span class='i'>I</span> Lite</h1>"
        "<div>VIDEO SYNTHESIZER</div><div class='m'>LOCAL NETWORK READY</div>"
        "<div class='s'>M4.1 connectivity foundation<br>Root synthesizer UI and /admin are the next web layers.</div>"
        "</div></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    prisma_network_status_t st = prisma_network_get_status();
    char json[256];
    snprintf(json, sizeof(json),
             "{\"mode\":\"%s\",\"local_ready\":%s,\"ap_active\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"clients\":%u}",
             st.mode == PRISMA_NETWORK_LOCAL_AP ? "local_ap" : "offline",
             st.local_ready ? "true" : "false",
             st.ap_active ? "true" : "false",
             st.ssid,
             st.ip,
             (unsigned)st.clients);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t health_handler(httpd_req_t *req)
{
    prisma_network_status_t st = prisma_network_get_status();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req,
                              st.local_ready
                                  ? "{\"status\":\"ok\",\"network\":\"local_ap\"}"
                                  : "{\"status\":\"degraded\",\"network\":\"offline\"}");
}

static bool start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;

    if (httpd_start(&s_http, &config) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed");
        return false;
    }

    const httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
    };
    const httpd_uri_t status = {
        .uri = "/api/v1/network/status",
        .method = HTTP_GET,
        .handler = status_handler,
    };
    const httpd_uri_t health = {
        .uri = "/api/v1/health",
        .method = HTTP_GET,
        .handler = health_handler,
    };

    if (httpd_register_uri_handler(s_http, &root) != ESP_OK ||
        httpd_register_uri_handler(s_http, &status) != ESP_OK ||
        httpd_register_uri_handler(s_http, &health) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP route registration failed");
        httpd_stop(s_http);
        s_http = NULL;
        return false;
    }

    return true;
}

bool prisma_network_init(void)
{
    if (s_initialized) return s_status.local_ready;
    s_initialized = true;

    ESP_LOGI(TAG, "M4.1 local-first network: autonomous AP, no router/Internet required");

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop init failed: %s", esp_err_to_name(err));
        return false;
    }

    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_ap_netif == NULL) {
        ESP_LOGE(TAG, "AP netif creation failed");
        return false;
    }

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_init);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_event_handler_register(WIFI_EVENT,
                                     ESP_EVENT_ANY_ID,
                                     &wifi_event_handler,
                                     NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi event registration failed: %s", esp_err_to_name(err));
        return false;
    }

    wifi_config_t ap = {0};
    snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s", PRISMA_LOCAL_AP_SSID);
    ap.ap.ssid_len = strlen(PRISMA_LOCAL_AP_SSID);
    ap.ap.channel = PRISMA_LOCAL_AP_CHANNEL;
    ap.ap.max_connection = PRISMA_LOCAL_AP_MAX_CLIENTS;
    ap.ap.authmode = WIFI_AUTH_OPEN;

    if ((err = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK ||
        (err = esp_wifi_set_mode(WIFI_MODE_AP)) != ESP_OK ||
        (err = esp_wifi_set_config(WIFI_IF_AP, &ap)) != ESP_OK ||
        (err = esp_wifi_start()) != ESP_OK) {
        ESP_LOGE(TAG, "Local AP start failed: %s", esp_err_to_name(err));
        return false;
    }

    refresh_ap_ip();

    if (!start_http_server()) {
        ESP_LOGW(TAG, "AP is active but HTTP server failed");
        return false;
    }

    prisma_network_status_t st = prisma_network_get_status();
    ESP_LOGI(TAG, "Local control network active: SSID=%s URL=http://%s",
             st.ssid,
             st.ip);
    return true;
}
