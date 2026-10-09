#include "admin_api.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "esp_app_desc.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "prisma_metrics.h"
#include "storage.h"

#define ADMIN_MAX_SOURCES 32
#define ADMIN_MAX_FILE_BYTES (512U * 1024U)
#define ADMIN_STORAGE_RESERVE (256U * 1024U)

extern const uint8_t web_admin_html_start[] asm("_binary_admin_html_start");
extern const uint8_t web_admin_html_end[] asm("_binary_admin_html_end");
extern const uint8_t web_admin_css_start[] asm("_binary_admin_css_start");
extern const uint8_t web_admin_css_end[] asm("_binary_admin_css_end");
extern const uint8_t web_admin_js_start[] asm("_binary_admin_js_start");
extern const uint8_t web_admin_js_end[] asm("_binary_admin_js_end");

static void no_cache(httpd_req_t *r)
{
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(r, "Pragma", "no-cache");
    httpd_resp_set_hdr(r, "Expires", "0");
}

static esp_err_t send_embedded(httpd_req_t *r, const char *type, const uint8_t *s, const uint8_t *e)
{
    httpd_resp_set_type(r, type);
    no_cache(r);
    return httpd_resp_send(r, (const char *)s, (ssize_t)(e - s));
}

static esp_err_t admin_page_get(httpd_req_t *r){return send_embedded(r,"text/html",web_admin_html_start,web_admin_html_end);}
static esp_err_t admin_css_get(httpd_req_t *r){return send_embedded(r,"text/css",web_admin_css_start,web_admin_css_end);}
static esp_err_t admin_js_get(httpd_req_t *r){return send_embedded(r,"application/javascript",web_admin_js_start,web_admin_js_end);}

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON: return "POWER ON";
        case ESP_RST_EXT: return "EXTERNAL";
        case ESP_RST_SW: return "SOFTWARE";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT WDT";
        case ESP_RST_TASK_WDT: return "TASK WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEP SLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}

static esp_err_t admin_overview_get(httpd_req_t *r)
{
    int batt_mv = 0, batt_pct = 0;
    bool battery = prisma_metrics_battery(&batt_mv, &batt_pct);
    const esp_app_desc_t *app = esp_app_get_description();
    uint64_t uptime_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
    uint32_t heap_free = esp_get_free_heap_size();
    uint32_t heap_min = esp_get_minimum_free_heap_size();
    char b[768];
    int n = snprintf(b, sizeof(b),
        "{\"version\":\"PRISMA-PI LITE v1.1 ADMIN RO\","
        "\"idf\":\"%s\",\"build_date\":\"%s\",\"build_time\":\"%s\","
        "\"uptime_ms\":%llu,\"heap_free\":%u,\"heap_min\":%u,"
        "\"reset_reason\":\"%s\","
        "\"battery\":{\"present\":%s,\"mv\":%d,\"percent\":%d,\"estimated\":true}}",
        esp_get_idf_version(), app ? app->date : "", app ? app->time : "",
        (unsigned long long)uptime_ms, (unsigned)heap_free, (unsigned)heap_min,
        reset_reason_name(esp_reset_reason()), battery ? "true" : "false",
        battery ? batt_mv : 0, battery ? batt_pct : 0);
    if (n < 0 || n >= (int)sizeof(b)) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "overview overflow");
    httpd_resp_set_type(r, "application/json"); no_cache(r);
    return httpd_resp_send(r, b, n);
}

static bool is_jpeg(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg"));
}

static void safe_json_name(char *dst, size_t dst_size, const char *src)
{
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 1 < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[j++] = (c == '"' || c == '\\' || c < 32) ? '_' : (char)c;
    }
    dst[j] = 0;
}

static esp_err_t admin_storage_get(httpd_req_t *r)
{
    size_t total = 0, used = 0;
    if (esp_spiffs_info("storage", &total, &used) != ESP_OK) {
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "storage unavailable");
    }
    size_t free_b = total > used ? total - used : 0;
    size_t safe = free_b > ADMIN_STORAGE_RESERVE ? free_b - ADMIN_STORAGE_RESERVE : 0;
    if (safe > ADMIN_MAX_FILE_BYTES) safe = ADMIN_MAX_FILE_BYTES;

    httpd_resp_set_type(r, "application/json"); no_cache(r);
    char head[384];
    int n = snprintf(head, sizeof(head),
        "{\"total\":%u,\"used\":%u,\"free\":%u,\"source_count\":%d,"
        "\"max_sources\":%d,\"max_file_bytes\":%u,\"reserve_bytes\":%u,"
        "\"safe_upload_now\":%u,\"files\":[",
        (unsigned)total,(unsigned)used,(unsigned)free_b,prisma_storage_count(),ADMIN_MAX_SOURCES,
        (unsigned)ADMIN_MAX_FILE_BYTES,(unsigned)ADMIN_STORAGE_RESERVE,(unsigned)safe);
    if (n < 0 || httpd_resp_send_chunk(r, head, n) != ESP_OK) return ESP_FAIL;

    DIR *dir = opendir("/storage");
    bool first = true;
    if (dir) {
        struct dirent *entry;
        const char *current = prisma_storage_current();
        while ((entry = readdir(dir)) != NULL) {
            char path[96], clean[64];
            if (!entry->d_name[0]) continue;
            int plen = snprintf(path, sizeof(path), "/storage/%s", entry->d_name);
            if (plen <= 0 || plen >= (int)sizeof(path)) continue;
            struct stat st;
            if (stat(path, &st) != 0) continue;
            safe_json_name(clean, sizeof(clean), entry->d_name);
            char item[256];
            int m = snprintf(item, sizeof(item), "%s{\"name\":\"%s\",\"size\":%u,\"source\":%s,\"active\":%s}",
                first ? "" : ",", clean, (unsigned)st.st_size,
                is_jpeg(entry->d_name) ? "true" : "false",
                current && !strcasecmp(current, entry->d_name) ? "true" : "false");
            if (m > 0 && m < (int)sizeof(item)) {
                if (httpd_resp_send_chunk(r, item, m) != ESP_OK) {closedir(dir); return ESP_FAIL;}
                first = false;
            }
        }
        closedir(dir);
    }
    return httpd_resp_send_chunk(r, "]}", 2);
}

static esp_err_t admin_network_get(httpd_req_t *r)
{
    wifi_config_t cfg = {0};
    esp_wifi_get_config(WIFI_IF_AP, &cfg);
    uint8_t channel = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&channel, &second);
    wifi_sta_list_t stations = {0};
    esp_wifi_ap_get_sta_list(&stations);

    char ipbuf[20] = "192.168.4.1";
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif) {
        esp_netif_ip_info_t ip = {0};
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
            snprintf(ipbuf, sizeof(ipbuf), IPSTR, IP2STR(&ip.ip));
        }
    }

    httpd_resp_set_type(r, "application/json"); no_cache(r);
    char head[320];
    int n = snprintf(head, sizeof(head),
        "{\"ssid\":\"%s\",\"ip\":\"%s\",\"channel\":%u,\"auth\":\"%s\","
        "\"max_clients\":%u,\"client_count\":%u,\"clients\":[",
        (char *)cfg.ap.ssid, ipbuf, (unsigned)channel,
        cfg.ap.authmode == WIFI_AUTH_OPEN ? "OPEN" : "PROTECTED",
        (unsigned)cfg.ap.max_connection, (unsigned)stations.num);
    if (n < 0 || httpd_resp_send_chunk(r, head, n) != ESP_OK) return ESP_FAIL;
    for (int i = 0; i < stations.num; i++) {
        char item[160];
        wifi_sta_info_t *s = &stations.sta[i];
        int m = snprintf(item, sizeof(item),
            "%s{\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"rssi\":%d}",
            i ? "," : "", s->mac[0],s->mac[1],s->mac[2],s->mac[3],s->mac[4],s->mac[5],(int)s->rssi);
        if (m > 0 && m < (int)sizeof(item) && httpd_resp_send_chunk(r, item, m) != ESP_OK) return ESP_FAIL;
    }
    return httpd_resp_send_chunk(r, "]}", 2);
}

void prisma_admin_register(httpd_handle_t server)
{
    if (!server) return;
    httpd_uri_t page = {.uri="/admin",.method=HTTP_GET,.handler=admin_page_get};
    httpd_uri_t css = {.uri="/admin.css",.method=HTTP_GET,.handler=admin_css_get};
    httpd_uri_t js = {.uri="/admin.js",.method=HTTP_GET,.handler=admin_js_get};
    httpd_uri_t overview = {.uri="/api/admin/overview",.method=HTTP_GET,.handler=admin_overview_get};
    httpd_uri_t storage = {.uri="/api/admin/storage",.method=HTTP_GET,.handler=admin_storage_get};
    httpd_uri_t network = {.uri="/api/admin/network",.method=HTTP_GET,.handler=admin_network_get};
    httpd_register_uri_handler(server, &page);
    httpd_register_uri_handler(server, &css);
    httpd_register_uri_handler(server, &js);
    httpd_register_uri_handler(server, &overview);
    httpd_register_uri_handler(server, &storage);
    httpd_register_uri_handler(server, &network);
}
