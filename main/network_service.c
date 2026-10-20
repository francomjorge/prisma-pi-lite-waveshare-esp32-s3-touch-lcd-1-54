#include "network_service.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NET_NS "net_cfg"
#define KEY_STA_SSID "sta_ssid"
#define KEY_STA_PASS "sta_pass"
#define KEY_AP_SSID  "ap_ssid"
#define KEY_AP_PASS  "ap_pass"

static const char *TAG = "NETWORK";
static SemaphoreHandle_t s_lock;
static bool s_initialized;
static bool s_sta_auto_connect;
static uint16_t s_last_disconnect_reason;
static char s_sta_ssid[33];
static char s_sta_pass[65];
static char s_ap_ssid[33];
static char s_ap_pass[65];
static esp_netif_t *s_ap_netif;
static esp_netif_t *s_sta_netif;

static void mac_to_text(const uint8_t mac[6], char out[18])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool nvs_get_text(nvs_handle_t h, const char *key, char *out, size_t out_size)
{
    if (!out || out_size == 0) return false;
    out[0] = 0;
    size_t len = out_size;
    return nvs_get_str(h, key, out, &len) == ESP_OK;
}

static void save_text_pair(const char *key1, const char *value1, const char *key2, const char *value2)
{
    nvs_handle_t h;
    if (nvs_open(NET_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, key1, value1 ? value1 : "");
    nvs_set_str(h, key2, value2 ? value2 : "");
    nvs_commit(h);
    nvs_close(h);
}

static void load_config(void)
{
    nvs_handle_t h;
    if (nvs_open(NET_NS, NVS_READONLY, &h) != ESP_OK) return;
    nvs_get_text(h, KEY_STA_SSID, s_sta_ssid, sizeof(s_sta_ssid));
    nvs_get_text(h, KEY_STA_PASS, s_sta_pass, sizeof(s_sta_pass));
    nvs_get_text(h, KEY_AP_SSID, s_ap_ssid, sizeof(s_ap_ssid));
    nvs_get_text(h, KEY_AP_PASS, s_ap_pass, sizeof(s_ap_pass));
    nvs_close(h);
}

static void configure_sta_from_memory(void)
{
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, s_sta_ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, s_sta_pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
}

static void apply_ap_config(void)
{
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.ap.ssid, s_ap_ssid, sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len = strlen(s_ap_ssid);
    cfg.ap.channel = 1;
    cfg.ap.max_connection = 4;
    cfg.ap.pmf_cfg.required = false;

    if (s_ap_pass[0]) {
        strlcpy((char *)cfg.ap.password, s_ap_pass, sizeof(cfg.ap.password));
        cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        cfg.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            bool connect_now = false;
            xSemaphoreTake(s_lock, portMAX_DELAY);
            connect_now = s_sta_auto_connect && s_sta_ssid[0];
            xSemaphoreGive(s_lock);
            if (connect_now) esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *e = (wifi_event_sta_disconnected_t *)data;
            bool retry = false;
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_last_disconnect_reason = e ? e->reason : 0;
            retry = s_sta_auto_connect && s_sta_ssid[0];
            xSemaphoreGive(s_lock);
            if (retry) esp_wifi_connect();
        }
    }
}

esp_err_t prisma_network_init(void)
{
    if (s_initialized) return ESP_OK;

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else if (r != ESP_OK) {
        return r;
    }

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;

    load_config();

    if (!s_ap_ssid[0]) {
        uint8_t mac[6];
        ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP));
        snprintf(s_ap_ssid, sizeof(s_ap_ssid), "PRISMA-PI-%02X%02X", mac[4], mac[5]);
    }
    s_sta_auto_connect = s_sta_ssid[0] != 0;

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) return loop_err;

    s_ap_netif = esp_netif_create_default_wifi_ap();
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_ap_netif || !s_sta_netif) return ESP_FAIL;

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    apply_ap_config();
    if (s_sta_ssid[0]) configure_sta_from_memory();

    ESP_ERROR_CHECK(esp_wifi_start());
    s_initialized = true;
    ESP_LOGI(TAG, "AP %s + STA %s", s_ap_ssid, s_sta_ssid[0] ? s_sta_ssid : "not configured");
    return ESP_OK;
}

void prisma_network_get_status(prisma_network_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!s_initialized) return;

    out->ap_running = true;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    strlcpy(out->ap_ssid, s_ap_ssid, sizeof(out->ap_ssid));
    strlcpy(out->sta_ssid, s_sta_ssid, sizeof(out->sta_ssid));
    out->sta_configured = s_sta_ssid[0] != 0;
    out->sta_auto_connect = s_sta_auto_connect;
    out->last_disconnect_reason = s_last_disconnect_reason;
    out->ap_secured = s_ap_pass[0] != 0;
    xSemaphoreGive(s_lock);

    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) == ESP_OK) mac_to_text(mac, out->ap_mac);
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) mac_to_text(mac, out->sta_mac);

    esp_netif_ip_info_t ip = {0};
    if (s_ap_netif && esp_netif_get_ip_info(s_ap_netif, &ip) == ESP_OK)
        snprintf(out->ap_ip, sizeof(out->ap_ip), IPSTR, IP2STR(&ip.ip));

    wifi_sta_list_t stations = {0};
    if (esp_wifi_ap_get_sta_list(&stations) == ESP_OK) out->ap_clients = stations.num;

    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&primary, &second) == ESP_OK) out->ap_channel = primary;

    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        out->sta_connected = true;
        out->sta_rssi = ap.rssi;
        out->sta_channel = ap.primary;
        if (ap.ssid[0]) strlcpy(out->sta_ssid, (const char *)ap.ssid, sizeof(out->sta_ssid));
    }

    memset(&ip, 0, sizeof(ip));
    if (s_sta_netif && esp_netif_get_ip_info(s_sta_netif, &ip) == ESP_OK && ip.ip.addr != 0) {
        out->sta_got_ip = true;
        snprintf(out->sta_ip, sizeof(out->sta_ip), IPSTR, IP2STR(&ip.ip));
        snprintf(out->sta_gateway, sizeof(out->sta_gateway), IPSTR, IP2STR(&ip.gw));
        snprintf(out->sta_netmask, sizeof(out->sta_netmask), IPSTR, IP2STR(&ip.netmask));
    }
}

int prisma_network_scan(prisma_network_ap_t *out, int max_items)
{
    if (!s_initialized || !out || max_items <= 0) return -1;
    if (max_items > PRISMA_NETWORK_MAX_SCAN) max_items = PRISMA_NETWORK_MAX_SCAN;

    wifi_scan_config_t scan = {0};
    scan.show_hidden = true;
    scan.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    esp_err_t err = esp_wifi_scan_start(&scan, true);
    if (err != ESP_OK) return -(int)err;

    uint16_t count = PRISMA_NETWORK_MAX_SCAN;
    wifi_ap_record_t records[PRISMA_NETWORK_MAX_SCAN];
    memset(records, 0, sizeof(records));
    if (esp_wifi_scan_get_ap_records(&count, records) != ESP_OK) return -2;

    int written = 0;
    for (uint16_t i = 0; i < count && written < max_items; i++) {
        if (!records[i].ssid[0]) continue;
        bool duplicate = false;
        for (int j = 0; j < written; j++) {
            if (strcmp(out[j].ssid, (const char *)records[i].ssid) == 0) { duplicate = true; break; }
        }
        if (duplicate) continue;
        strlcpy(out[written].ssid, (const char *)records[i].ssid, sizeof(out[written].ssid));
        out[written].rssi = records[i].rssi;
        out[written].channel = records[i].primary;
        out[written].authmode = (uint8_t)records[i].authmode;
        written++;
    }
    return written;
}

esp_err_t prisma_network_connect(const char *ssid, const char *password)
{
    if (!s_initialized || !ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;
    size_t sl = strlen(ssid), pl = password ? strlen(password) : 0;
    if (sl > 32 || pl > 64) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    strlcpy(s_sta_ssid, ssid, sizeof(s_sta_ssid));
    strlcpy(s_sta_pass, password ? password : "", sizeof(s_sta_pass));
    s_sta_auto_connect = true;
    s_last_disconnect_reason = 0;
    xSemaphoreGive(s_lock);

    save_text_pair(KEY_STA_SSID, s_sta_ssid, KEY_STA_PASS, s_sta_pass);
    configure_sta_from_memory();
    esp_wifi_disconnect();
    return esp_wifi_connect();
}

esp_err_t prisma_network_disconnect(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_sta_auto_connect = false;
    xSemaphoreGive(s_lock);
    esp_err_t err = esp_wifi_disconnect();
    return (err == ESP_ERR_WIFI_NOT_CONNECT) ? ESP_OK : err;
}

esp_err_t prisma_network_reconnect(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool configured = s_sta_ssid[0] != 0;
    s_sta_auto_connect = configured;
    xSemaphoreGive(s_lock);
    if (!configured) return ESP_ERR_INVALID_STATE;
    configure_sta_from_memory();
    return esp_wifi_connect();
}

esp_err_t prisma_network_forget(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_sta_ssid[0] = 0;
    s_sta_pass[0] = 0;
    s_sta_auto_connect = false;
    s_last_disconnect_reason = 0;
    xSemaphoreGive(s_lock);

    save_text_pair(KEY_STA_SSID, "", KEY_STA_PASS, "");
    wifi_config_t empty = {0};
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &empty));
    esp_err_t err = esp_wifi_disconnect();
    return (err == ESP_ERR_WIFI_NOT_CONNECT) ? ESP_OK : err;
}

esp_err_t prisma_network_set_ap(const char *ssid, const char *password)
{
    if (!s_initialized || !ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;
    size_t sl = strlen(ssid), pl = password ? strlen(password) : 0;
    if (sl > 32 || pl > 63 || (pl > 0 && pl < 8)) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    strlcpy(s_ap_ssid, ssid, sizeof(s_ap_ssid));
    strlcpy(s_ap_pass, password ? password : "", sizeof(s_ap_pass));
    xSemaphoreGive(s_lock);

    save_text_pair(KEY_AP_SSID, s_ap_ssid, KEY_AP_PASS, s_ap_pass);
    apply_ap_config();
    ESP_LOGI(TAG, "fallback AP changed to %s", s_ap_ssid);
    return ESP_OK;
}
