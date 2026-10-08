#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "wifi_portal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "PRISMA_WIFI";

static volatile bool control_on = true;
static volatile int control_mode = 0;
static volatile int control_intensity = 61;
static volatile int control_generation = 1;
static char control_filename[32] = "F1_MOON.JPG";
static volatile bool control_effects[6] = {false, false, false, false, false, true};
static volatile int control_effect_values[6] = {50, 50, 50, 50, 50, 50};

static QueueHandle_t page2_command_queue = NULL;
static uint32_t page2_command_seq = 0;
static int last_logged_tx_generation = -1;

static void bump_generation(void)
{
    control_generation++;
    if (control_generation <= 0) control_generation = 1;
}

bool prisma_control_get_on(void) { return control_on; }
int prisma_control_get_mode(void) { return control_mode; }
int prisma_control_get_intensity(void) { return control_intensity; }
int prisma_control_get_generation(void) { return control_generation; }

void prisma_control_set_on(bool v)
{
    if (control_on != v) {
        control_on = v;
        bump_generation();
    }
}

void prisma_control_set_mode(int v)
{
    v = v ? 1 : 0;
    if (control_mode != v) {
        control_mode = v;
        bump_generation();
    }
}

void prisma_control_set_intensity(int v)
{
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    if (control_intensity != v) {
        control_intensity = v;
        bump_generation();
    }
}

void prisma_control_set_generation(int v)
{
    control_generation = v > 0 ? v : 1;
}

void prisma_control_set_filename(const char *v)
{
    if (v && *v && strcmp(control_filename, v) != 0) {
        strlcpy(control_filename, v, sizeof(control_filename));
        bump_generation();
    }
}

void prisma_control_set_effect(int effect, bool enabled)
{
    if (effect < 0 || effect >= 6) return;
    if (effect == 5) enabled = true;
    if (control_effects[effect] != enabled) {
        control_effects[effect] = enabled;
        bump_generation();
    }
}

bool prisma_control_get_effect(int effect)
{
    if (effect < 0 || effect >= 6) return false;
    if (effect == 5) return true;
    return control_effects[effect];
}

void prisma_control_set_effect_value(int effect, int value)
{
    if (effect < 0 || effect >= 6) return;
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    if (control_effect_values[effect] != value) {
        control_effect_values[effect] = value;
        bump_generation();
    }
}

int prisma_control_get_effect_value(int effect)
{
    if (effect < 0 || effect >= 6) return 0;
    return control_effect_values[effect];
}

bool prisma_control_take_page2_command(prisma_page2_command_t *out)
{
    if (!out || !page2_command_queue) return false;
    return xQueueReceive(page2_command_queue, out, 0) == pdTRUE;
}

extern const uint8_t web_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t web_index_html_end[] asm("_binary_index_html_end");
extern const uint8_t web_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t web_style_css_end[] asm("_binary_style_css_end");
extern const uint8_t web_app_js_start[] asm("_binary_app_js_start");
extern const uint8_t web_app_js_end[] asm("_binary_app_js_end");

static esp_err_t send_embedded(httpd_req_t *r, const char *type, const uint8_t *s, const uint8_t *e)
{
    httpd_resp_set_type(r, type);
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_send(r, (const char *)s, (ssize_t)(e - s));
}

static esp_err_t page_get(httpd_req_t *r) { return send_embedded(r, "text/html", web_index_html_start, web_index_html_end); }
static esp_err_t css_get(httpd_req_t *r) { return send_embedded(r, "text/css", web_style_css_start, web_style_css_end); }
static esp_err_t js_get(httpd_req_t *r) { return send_embedded(r, "application/javascript", web_app_js_start, web_app_js_end); }

static esp_err_t state_get(httpd_req_t *r)
{
    char b[640];
    int generation = control_generation;

    snprintf(
        b, sizeof(b),
        "{\"on\":%s,\"mode\":%d,\"intensity\":%d,\"generation\":%d,\"filename\":\"%s\",\"effects\":[%s,%s,%s,%s,%s,true],\"effect_values\":[%d,%d,%d,%d,%d,%d]}",
        control_on ? "true" : "false",
        control_mode,
        control_intensity,
        generation,
        control_filename,
        control_effects[0] ? "true" : "false",
        control_effects[1] ? "true" : "false",
        control_effects[2] ? "true" : "false",
        control_effects[3] ? "true" : "false",
        control_effects[4] ? "true" : "false",
        control_effect_values[0], control_effect_values[1], control_effect_values[2],
        control_effect_values[3], control_effect_values[4], control_effect_values[5]);

    if (generation != last_logged_tx_generation) {
        ESP_LOGI(
            TAG,
            "WEB TX gen=%d on=%s source=%s intensity=%d mode=%s",
            generation,
            control_on ? "ON" : "OFF",
            control_filename,
            control_intensity,
            control_mode ? "BYPASS" : "RANDOM");
        last_logged_tx_generation = generation;
    }

    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_sendstr(r, b);
}

static esp_err_t control_post(httpd_req_t *r)
{
    if (r->content_len <= 0 || r->content_len >= 256) {
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid control payload");
        return ESP_FAIL;
    }

    char body[256];
    int total = 0;
    while (total < r->content_len) {
        int got = httpd_req_recv(r, body + total, r->content_len - total);
        if (got <= 0) {
            httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Control body read failed");
            return ESP_FAIL;
        }
        total += got;
    }
    body[total] = '\0';

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    prisma_page2_command_t cmd = {0};
    cJSON *on = cJSON_GetObjectItemCaseSensitive(root, "on");
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, "mode");
    cJSON *intensity = cJSON_GetObjectItemCaseSensitive(root, "intensity");
    cJSON *source_next = cJSON_GetObjectItemCaseSensitive(root, "source_next");

    if (cJSON_IsBool(on)) {
        cmd.has_on = true;
        cmd.on = cJSON_IsTrue(on);
    }
    if (cJSON_IsNumber(mode)) {
        cmd.has_mode = true;
        cmd.mode = mode->valueint ? 1 : 0;
    }
    if (cJSON_IsNumber(intensity)) {
        cmd.has_intensity = true;
        cmd.intensity = intensity->valueint;
        if (cmd.intensity < 0) cmd.intensity = 0;
        if (cmd.intensity > 100) cmd.intensity = 100;
    }
    if (cJSON_IsBool(source_next) && cJSON_IsTrue(source_next)) {
        cmd.source_next = true;
    }

    cJSON_Delete(root);

    if (!cmd.has_on && !cmd.has_mode && !cmd.has_intensity && !cmd.source_next) {
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "No supported page 2 control field");
        return ESP_FAIL;
    }

    cmd.seq = ++page2_command_seq;

    if (!page2_command_queue || xQueueSend(page2_command_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "WEB RX #%lu dropped: page2 command queue full", (unsigned long)cmd.seq);
        httpd_resp_send_err(r, HTTPD_503_SERVICE_UNAVAILABLE, "Control queue busy");
        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "WEB RX #%lu on=%s mode=%s intensity=%s source_next=%s",
        (unsigned long)cmd.seq,
        cmd.has_on ? (cmd.on ? "ON" : "OFF") : "-",
        cmd.has_mode ? (cmd.mode ? "BYPASS" : "RANDOM") : "-",
        cmd.has_intensity ? "SET" : "-",
        cmd.source_next ? "YES" : "NO");
    if (cmd.has_intensity) ESP_LOGI(TAG, "WEB RX #%lu intensity_value=%d", (unsigned long)cmd.seq, cmd.intensity);

    char response[64];
    snprintf(response, sizeof(response), "{\"accepted\":true,\"seq\":%lu}", (unsigned long)cmd.seq);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_sendstr(r, response);
}

static bool source_name_ok(const char *name)
{
    if (!name || !*name || strlen(name) > 63) return false;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (!(isalnum(*p) || *p == '_' || *p == '-' || *p == '.')) return false;
    }
    return strstr(name, "..") == NULL;
}

static esp_err_t source_get(httpd_req_t *r)
{
    const char *prefix = "/source/";
    const char *name = r->uri + strlen(prefix);
    const char *q = strchr(name, '?');
    char clean[64];
    size_t n = q ? (size_t)(q - name) : strlen(name);

    if (n == 0 || n >= sizeof(clean)) {
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid source");
        return ESP_FAIL;
    }
    memcpy(clean, name, n);
    clean[n] = '\0';
    if (!source_name_ok(clean)) {
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid source");
        return ESP_FAIL;
    }

    char path[80];
    int plen = snprintf(path, sizeof(path), "/storage/%s", clean);
    if (plen < 0 || plen >= (int)sizeof(path)) {
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid source");
        return ESP_FAIL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Source not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(r, "image/jpeg");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    char buf[1024];
    size_t got;
    esp_err_t err = ESP_OK;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0) {
        err = httpd_resp_send_chunk(r, buf, got);
        if (err != ESP_OK) break;
    }
    fclose(f);
    if (err == ESP_OK) err = httpd_resp_send_chunk(r, NULL, 0);
    return err;
}

static void start_portal(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ap[32];
    snprintf(ap, sizeof(ap), "PRISMA-PI-%02X%02X", mac[4], mac[5]);

    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.ap.ssid, ap, sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len = strlen(ap);
    cfg.ap.max_connection = 4;
    cfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t s = NULL;

    if (httpd_start(&s, &hc) == ESP_OK) {
        httpd_uri_t a = {.uri = "/", .method = HTTP_GET, .handler = page_get};
        httpd_uri_t b = {.uri = "/api/state", .method = HTTP_GET, .handler = state_get};
        httpd_uri_t c = {.uri = "/style.css", .method = HTTP_GET, .handler = css_get};
        httpd_uri_t d = {.uri = "/app.js", .method = HTTP_GET, .handler = js_get};
        httpd_uri_t e = {.uri = "/source/*", .method = HTTP_GET, .handler = source_get};
        httpd_uri_t f = {.uri = "/api/control", .method = HTTP_POST, .handler = control_post};
        httpd_register_uri_handler(s, &a);
        httpd_register_uri_handler(s, &b);
        httpd_register_uri_handler(s, &c);
        httpd_register_uri_handler(s, &d);
        httpd_register_uri_handler(s, &e);
        httpd_register_uri_handler(s, &f);
    }

    ESP_LOGI(TAG, "PRISMA LITE controller: %s - http://192.168.4.1", ap);
}

void prisma_wifi_start(void)
{
    if (!page2_command_queue) {
        page2_command_queue = xQueueCreate(8, sizeof(prisma_page2_command_t));
        if (!page2_command_queue) {
            ESP_LOGE(TAG, "Cannot create page2 command queue");
        }
    }

    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t i = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&i));
    start_portal();
}
