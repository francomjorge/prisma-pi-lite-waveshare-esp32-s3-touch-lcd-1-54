#include "prisma_network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"

#define PRISMA_WIFI_NAMESPACE "prisma_wifi"
#define PRISMA_WIFI_SSID_KEY "ssid"
#define PRISMA_WIFI_PASS_KEY "pass"
#define PRISMA_SETUP_AP_SSID "PRISMA-PI-SETUP"
#define PRISMA_STA_RETRY_LIMIT 5

static const char *TAG = "prisma_network";

static bool s_initialized = false;
static bool s_has_credentials = false;
static bool s_setup_ap_active = false;
static uint8_t s_retry_count = 0;
static httpd_handle_t s_http = NULL;
static char s_saved_ssid[33] = {0};
static char s_saved_password[65] = {0};
static prisma_network_status_t s_status = {
    .mode = PRISMA_NETWORK_OFFLINE,
    .connected = false,
    .setup_ap_active = false,
    .ssid = {0},
    .ip = "0.0.0.0",
    .rssi = -127,
    .retry_count = 0,
};
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;

static void status_set(prisma_network_mode_t mode,
                       bool connected,
                       bool ap_active,
                       const char *ssid,
                       const char *ip,
                       int8_t rssi,
                       uint8_t retry_count)
{
    portENTER_CRITICAL(&s_status_lock);
    s_status.mode = mode;
    s_status.connected = connected;
    s_status.setup_ap_active = ap_active;
    s_status.rssi = rssi;
    s_status.retry_count = retry_count;
    snprintf(s_status.ssid, sizeof(s_status.ssid), "%s", ssid ? ssid : "");
    snprintf(s_status.ip, sizeof(s_status.ip), "%s", ip ? ip : "0.0.0.0");
    portEXIT_CRITICAL(&s_status_lock);
}

prisma_network_status_t prisma_network_get_status(void)
{
    prisma_network_status_t snapshot;
    portENTER_CRITICAL(&s_status_lock);
    snapshot = s_status;
    portEXIT_CRITICAL(&s_status_lock);

    if (snapshot.connected) {
        wifi_ap_record_t ap = {0};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            snapshot.rssi = ap.rssi;
        }
    }
    return snapshot;
}

static bool load_credentials(void)
{
    nvs_handle_t nvs;
    if (nvs_open(PRISMA_WIFI_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }

    size_t ssid_len = sizeof(s_saved_ssid);
    size_t pass_len = sizeof(s_saved_password);
    esp_err_t ssid_err = nvs_get_str(nvs, PRISMA_WIFI_SSID_KEY, s_saved_ssid, &ssid_len);
    esp_err_t pass_err = nvs_get_str(nvs, PRISMA_WIFI_PASS_KEY, s_saved_password, &pass_len);
    nvs_close(nvs);

    if (ssid_err != ESP_OK || pass_err != ESP_OK || s_saved_ssid[0] == '\0') {
        s_saved_ssid[0] = '\0';
        s_saved_password[0] = '\0';
        return false;
    }
    return true;
}

static bool save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(PRISMA_WIFI_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return false;

    err = nvs_set_str(nvs, PRISMA_WIFI_SSID_KEY, ssid);
    if (err == ESP_OK) err = nvs_set_str(nvs, PRISMA_WIFI_PASS_KEY, password);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err == ESP_OK;
}

static bool erase_credentials(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(PRISMA_WIFI_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return false;
    err = nvs_erase_all(nvs);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err == ESP_OK;
}

static wifi_config_t make_sta_config(const char *ssid, const char *password)
{
    wifi_config_t config = {0};
    snprintf((char *)config.sta.ssid, sizeof(config.sta.ssid), "%s", ssid);
    snprintf((char *)config.sta.password, sizeof(config.sta.password), "%s", password);
    config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    return config;
}

static wifi_config_t make_ap_config(void)
{
    wifi_config_t config = {0};
    snprintf((char *)config.ap.ssid, sizeof(config.ap.ssid), "%s", PRISMA_SETUP_AP_SSID);
    config.ap.ssid_len = strlen(PRISMA_SETUP_AP_SSID);
    config.ap.channel = 1;
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_OPEN;
    return config;
}

static void start_setup_ap(void)
{
    wifi_config_t ap = make_ap_config();
    wifi_mode_t mode = s_has_credentials ? WIFI_MODE_APSTA : WIFI_MODE_AP;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(mode));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_AP, &ap));
    s_setup_ap_active = true;

    status_set(PRISMA_NETWORK_SETUP_AP,
               false,
               true,
               PRISMA_SETUP_AP_SSID,
               "192.168.4.1",
               -127,
               s_retry_count);

    ESP_LOGW(TAG, "Setup AP active: %s -> http://192.168.4.1/setup", PRISMA_SETUP_AP_SSID);
}

static void network_event_handler(void *arg,
                                  esp_event_base_t event_base,
                                  int32_t event_id,
                                  void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_has_credentials) {
            ESP_LOGI(TAG, "Connecting to saved Wi-Fi SSID: %s", s_saved_ssid);
            status_set(PRISMA_NETWORK_STA_CONNECTING, false, s_setup_ap_active,
                       s_saved_ssid, "0.0.0.0", -127, s_retry_count);
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
        }
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (!s_has_credentials) return;

        s_retry_count++;
        status_set(PRISMA_NETWORK_STA_CONNECTING, false, s_setup_ap_active,
                   s_saved_ssid, "0.0.0.0", -127, s_retry_count);

        if (!s_setup_ap_active && s_retry_count < PRISMA_STA_RETRY_LIMIT) {
            ESP_LOGW(TAG, "Wi-Fi disconnected, retry %u/%u",
                     (unsigned)s_retry_count, (unsigned)PRISMA_STA_RETRY_LIMIT);
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
        } else if (!s_setup_ap_active) {
            ESP_LOGW(TAG, "Wi-Fi connection failed; entering setup AP fallback");
            start_setup_ap();
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        char ip[16];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;

        wifi_ap_record_t ap = {0};
        int8_t rssi = -127;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;

        status_set(PRISMA_NETWORK_STA_CONNECTED, true, false,
                   s_saved_ssid, ip, rssi, 0);
        ESP_LOGI(TAG, "Wi-Fi connected: SSID=%s IP=%s RSSI=%d dBm",
                 s_saved_ssid, ip, (int)rssi);

        if (s_setup_ap_active) {
            s_setup_ap_active = false;
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
        }
        return;
    }
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *text)
{
    char *src = text;
    char *dst = text;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' && src[1] && src[2]) {
            int hi = hex_value(src[1]);
            int lo = hex_value(src[2]);
            if (hi >= 0 && lo >= 0) {
                *dst++ = (char)((hi << 4) | lo);
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static bool form_value(const char *body, const char *key, char *out, size_t out_size)
{
    size_t key_len = strlen(key);
    const char *p = body;
    while (*p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            p += key_len + 1;
            const char *end = strchr(p, '&');
            size_t len = end ? (size_t)(end - p) : strlen(p);
            if (len >= out_size) len = out_size - 1;
            memcpy(out, p, len);
            out[len] = '\0';
            url_decode(out);
            return true;
        }
        p = strchr(p, '&');
        if (!p) break;
        p++;
    }
    return false;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/setup");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t setup_handler(httpd_req_t *req)
{
    static const char page[] =
        "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>PRISMA-PI Wi-Fi Setup</title><style>"
        "body{margin:0;background:#070b12;color:#eee;font-family:monospace;display:grid;place-items:center;min-height:100vh}"
        ".c{width:min(440px,88vw);border:1px solid #18dce0;padding:24px;background:#0c1320}"
        "h1{margin-top:0}.pi{color:#ff32b8}.ok{color:#36ec88}.muted{color:#8a94a8}"
        "input,button{box-sizing:border-box;width:100%;padding:12px;margin:7px 0;background:#111c2d;color:#fff;border:1px solid #40506b;font:inherit}"
        "button{background:#0b72e7;border-color:#18dce0;font-weight:bold}#s{white-space:pre-line;margin:14px 0}"
        "</style></head><body><div class='c'><h1>PRISMA-<span class='pi'>PI</span> Lite</h1>"
        "<div class='muted'>M4.1 Wi-Fi provisioning</div><div id='s'>Loading...</div>"
        "<form id='f'><label>SSID</label><input id='ssid' name='ssid' maxlength='32' required>"
        "<label>Password</label><input id='password' name='password' type='password' maxlength='64'>"
        "<button>Save & Connect</button></form>"
        "<button id='forget' type='button'>Forget Wi-Fi / Setup AP</button>"
        "<script>async function st(){let r=await fetch('/api/v1/network/status');let j=await r.json();"
        "document.getElementById('s').textContent='Mode: '+j.mode+'\\nSSID: '+j.ssid+'\\nIP: '+j.ip+'\\nRSSI: '+j.rssi+' dBm';}"
        "document.getElementById('f').onsubmit=async e=>{e.preventDefault();let b=new URLSearchParams(new FormData(e.target));"
        "let r=await fetch('/api/v1/network/configure',{method:'POST',body:b});alert(await r.text());setTimeout(st,1000)};"
        "document.getElementById('forget').onclick=async()=>{let r=await fetch('/api/v1/network/reset',{method:'POST'});alert(await r.text());setTimeout(st,500)};"
        "st();setInterval(st,3000)</script></div></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static const char *mode_name(prisma_network_mode_t mode)
{
    switch (mode) {
        case PRISMA_NETWORK_STA_CONNECTING: return "connecting";
        case PRISMA_NETWORK_STA_CONNECTED: return "connected";
        case PRISMA_NETWORK_SETUP_AP: return "setup_ap";
        default: return "offline";
    }
}

static void json_escape(const char *src, char *dst, size_t size)
{
    size_t w = 0;
    for (size_t i = 0; src[i] && w + 2 < size; ++i) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            dst[w++] = '\\';
            dst[w++] = c;
        } else if ((unsigned char)c >= 0x20) {
            dst[w++] = c;
        }
    }
    dst[w] = '\0';
}

static esp_err_t status_handler(httpd_req_t *req)
{
    prisma_network_status_t st = prisma_network_get_status();
    char ssid[70];
    json_escape(st.ssid, ssid, sizeof(ssid));
    char json[256];
    snprintf(json, sizeof(json),
             "{\"mode\":\"%s\",\"connected\":%s,\"setup_ap\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"retries\":%u}",
             mode_name(st.mode), st.connected ? "true" : "false",
             st.setup_ap_active ? "true" : "false", ssid, st.ip,
             (int)st.rssi, (unsigned)st.retry_count);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t configure_handler(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len >= 256) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request");
        return ESP_OK;
    }

    char body[256];
    int received = httpd_req_recv(req, body, req->content_len);
    if (received <= 0) return ESP_FAIL;
    body[received] = '\0';

    char ssid[33] = {0};
    char password[65] = {0};
    if (!form_value(body, "ssid", ssid, sizeof(ssid)) || ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_OK;
    }
    (void)form_value(body, "password", password, sizeof(password));

    if (!prisma_network_configure_wifi(ssid, password)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save Wi-Fi settings");
        return ESP_OK;
    }

    return httpd_resp_sendstr(req, "Saved. PRISMA-PI is connecting to Wi-Fi.");
}

static esp_err_t reset_handler(httpd_req_t *req)
{
    if (!prisma_network_clear_wifi()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not clear Wi-Fi settings");
        return ESP_OK;
    }
    return httpd_resp_sendstr(req, "Wi-Fi forgotten. Setup AP is active at 192.168.4.1.");
}

static bool start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;

    if (httpd_start(&s_http, &config) != ESP_OK) return false;

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_handler};
    const httpd_uri_t setup = {.uri = "/setup", .method = HTTP_GET, .handler = setup_handler};
    const httpd_uri_t status = {.uri = "/api/v1/network/status", .method = HTTP_GET, .handler = status_handler};
    const httpd_uri_t configure = {.uri = "/api/v1/network/configure", .method = HTTP_POST, .handler = configure_handler};
    const httpd_uri_t reset = {.uri = "/api/v1/network/reset", .method = HTTP_POST, .handler = reset_handler};

    if (httpd_register_uri_handler(s_http, &root) != ESP_OK ||
        httpd_register_uri_handler(s_http, &setup) != ESP_OK ||
        httpd_register_uri_handler(s_http, &status) != ESP_OK ||
        httpd_register_uri_handler(s_http, &configure) != ESP_OK ||
        httpd_register_uri_handler(s_http, &reset) != ESP_OK) {
        httpd_stop(s_http);
        s_http = NULL;
        return false;
    }
    return true;
}

bool prisma_network_configure_wifi(const char *ssid, const char *password)
{
    if (!s_initialized || !ssid || !password) return false;
    size_t ssid_len = strlen(ssid);
    size_t pass_len = strlen(password);
    if (ssid_len == 0 || ssid_len > 32 || pass_len > 64) return false;

    if (!save_credentials(ssid, password)) return false;

    snprintf(s_saved_ssid, sizeof(s_saved_ssid), "%s", ssid);
    snprintf(s_saved_password, sizeof(s_saved_password), "%s", password);
    s_has_credentials = true;
    s_retry_count = 0;

    wifi_config_t sta = make_sta_config(s_saved_ssid, s_saved_password);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(s_setup_ap_active ? WIFI_MODE_APSTA : WIFI_MODE_STA));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &sta));
    status_set(PRISMA_NETWORK_STA_CONNECTING, false, s_setup_ap_active,
               s_saved_ssid, s_setup_ap_active ? "192.168.4.1" : "0.0.0.0", -127, 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
    return true;
}

bool prisma_network_clear_wifi(void)
{
    if (!s_initialized) return false;
    if (!erase_credentials()) return false;

    s_has_credentials = false;
    s_saved_ssid[0] = '\0';
    s_saved_password[0] = '\0';
    s_retry_count = 0;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_disconnect());
    start_setup_ap();
    return true;
}

bool prisma_network_init(void)
{
    if (s_initialized) return true;

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

    if (!esp_netif_create_default_wifi_sta() || !esp_netif_create_default_wifi_ap()) {
        ESP_LOGE(TAG, "Could not create Wi-Fi network interfaces");
        return false;
    }

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event_handler, NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, network_event_handler, NULL));

    s_has_credentials = load_credentials();
    s_initialized = true;

    if (s_has_credentials) {
        wifi_config_t sta = make_sta_config(s_saved_ssid, s_saved_password);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &sta));
        status_set(PRISMA_NETWORK_STA_CONNECTING, false, false,
                   s_saved_ssid, "0.0.0.0", -127, 0);
    } else {
        wifi_config_t ap = make_ap_config();
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_AP, &ap));
        s_setup_ap_active = true;
        status_set(PRISMA_NETWORK_SETUP_AP, false, true,
                   PRISMA_SETUP_AP_SSID, "192.168.4.1", -127, 0);
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(err));
        return false;
    }

    if (!start_http_server()) {
        ESP_LOGE(TAG, "Provisioning HTTP server failed to start");
        return false;
    }

    if (s_has_credentials) {
        ESP_LOGI(TAG, "M4.1 network ready: trying saved Wi-Fi credentials");
    } else {
        ESP_LOGI(TAG, "M4.1 network ready: setup AP %s at 192.168.4.1", PRISMA_SETUP_AP_SSID);
    }
    return true;
}
