#include "admin_api.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"

#define ADMIN_MEDIA_MAX 64

typedef struct {
    char name[64];
    uint32_t size;
} admin_media_file_t;

extern const uint8_t web_admin_html_start[] asm("_binary_admin_html_start");
extern const uint8_t web_admin_html_end[] asm("_binary_admin_html_end");
extern const uint8_t web_admin_css_start[] asm("_binary_admin_css_start");
extern const uint8_t web_admin_css_end[] asm("_binary_admin_css_end");
extern const uint8_t web_admin_js_start[] asm("_binary_admin_js_start");
extern const uint8_t web_admin_js_end[] asm("_binary_admin_js_end");

static bool storage_snapshot_done;
static bool storage_snapshot_ok;
static size_t storage_total_cached;
static size_t storage_used_cached;
static size_t storage_free_cached;
static int media_count_cached;
static admin_media_file_t media_files_cached[ADMIN_MEDIA_MAX];

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

static esp_err_t admin_page_get(httpd_req_t *r)
{
    return send_embedded(r, "text/html", web_admin_html_start, web_admin_html_end);
}

static esp_err_t admin_css_get(httpd_req_t *r)
{
    return send_embedded(r, "text/css", web_admin_css_start, web_admin_css_end);
}

static esp_err_t admin_js_get(httpd_req_t *r)
{
    return send_embedded(r, "application/javascript", web_admin_js_start, web_admin_js_end);
}

static bool is_media(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return !strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg") ||
           !strcasecmp(dot, ".png") || !strcasecmp(dot, ".bmp") ||
           !strcasecmp(dot, ".gif") || !strcasecmp(dot, ".mp4") ||
           !strcasecmp(dot, ".mov") || !strcasecmp(dot, ".avi") ||
           !strcasecmp(dot, ".webm") || !strcasecmp(dot, ".mp3") ||
           !strcasecmp(dot, ".wav") || !strcasecmp(dot, ".ogg");
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

/*
 * ADMIN invariant:
 * Storage is measured and enumerated exactly once per appliance boot.
 * Every later request returns this RAM snapshot and never rescans SPIFFS.
 */
static void admin_storage_snapshot_init(void)
{
    if (storage_snapshot_done) return;
    storage_snapshot_done = true;

    size_t total = 0, used = 0;
    if (esp_spiffs_info("storage", &total, &used) != ESP_OK) {
        storage_snapshot_ok = false;
        return;
    }

    storage_total_cached = total;
    storage_used_cached = used;
    storage_free_cached = total > used ? total - used : 0;

    DIR *dir = opendir("/storage");
    if (!dir) {
        storage_snapshot_ok = false;
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && media_count_cached < ADMIN_MEDIA_MAX) {
        if (!entry->d_name[0] || !is_media(entry->d_name)) continue;

        char path[96];
        int plen = snprintf(path, sizeof(path), "/storage/%s", entry->d_name);
        if (plen <= 0 || plen >= (int)sizeof(path)) continue;

        struct stat st;
        if (stat(path, &st) != 0) continue;

        admin_media_file_t *f = &media_files_cached[media_count_cached++];
        strlcpy(f->name, entry->d_name, sizeof(f->name));
        f->size = (uint32_t)st.st_size;
    }
    closedir(dir);
    storage_snapshot_ok = true;
}

static esp_err_t admin_storage_get(httpd_req_t *r)
{
    admin_storage_snapshot_init();
    if (!storage_snapshot_ok)
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "storage snapshot unavailable");

    httpd_resp_set_type(r, "application/json");
    no_cache(r);

    char head[256];
    int n = snprintf(head, sizeof(head),
        "{\"snapshot\":\"boot\",\"total\":%u,\"used\":%u,\"free\":%u,\"media_count\":%d,\"files\":[",
        (unsigned)storage_total_cached,
        (unsigned)storage_used_cached,
        (unsigned)storage_free_cached,
        media_count_cached);
    if (n <= 0 || n >= (int)sizeof(head) || httpd_resp_send_chunk(r, head, n) != ESP_OK)
        return ESP_FAIL;

    for (int i = 0; i < media_count_cached; i++) {
        char clean[64];
        char item[192];
        safe_json_name(clean, sizeof(clean), media_files_cached[i].name);
        int m = snprintf(item, sizeof(item),
            "%s{\"name\":\"%s\",\"size\":%u}",
            i ? "," : "",
            clean,
            (unsigned)media_files_cached[i].size);
        if (m <= 0 || m >= (int)sizeof(item) || httpd_resp_send_chunk(r, item, m) != ESP_OK)
            return ESP_FAIL;
    }

    if (httpd_resp_send_chunk(r, "]}", 2) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(r, NULL, 0);
}

static void get_ap_ip(char *ipbuf, size_t size)
{
    snprintf(ipbuf, size, "192.168.4.1");
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!netif) return;

    esp_netif_ip_info_t ip = {0};
    if (esp_netif_get_ip_info(netif, &ip) == ESP_OK)
        snprintf(ipbuf, size, IPSTR, IP2STR(&ip.ip));
}

/* Wi-Fi is read only when the user presses CHECK WI-FI. */
static esp_err_t admin_network_get(httpd_req_t *r)
{
    wifi_config_t cfg = {0};
    if (esp_wifi_get_config(WIFI_IF_AP, &cfg) != ESP_OK)
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "wifi config unavailable");

    uint8_t channel = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    (void)esp_wifi_get_channel(&channel, &second);

    wifi_sta_list_t stations = {0};
    (void)esp_wifi_ap_get_sta_list(&stations);

    char ipbuf[20];
    get_ap_ip(ipbuf, sizeof(ipbuf));

    httpd_resp_set_type(r, "application/json");
    no_cache(r);

    char head[320];
    int n = snprintf(head, sizeof(head),
        "{\"ssid\":\"%s\",\"ip\":\"%s\",\"channel\":%u,\"auth\":\"%s\",\"max_clients\":%u,\"client_count\":%u,\"clients\":[",
        (char *)cfg.ap.ssid,
        ipbuf,
        (unsigned)channel,
        cfg.ap.authmode == WIFI_AUTH_OPEN ? "OPEN" : "PROTECTED",
        (unsigned)cfg.ap.max_connection,
        (unsigned)stations.num);
    if (n <= 0 || n >= (int)sizeof(head) || httpd_resp_send_chunk(r, head, n) != ESP_OK)
        return ESP_FAIL;

    for (int i = 0; i < stations.num; i++) {
        wifi_sta_info_t *s = &stations.sta[i];
        char item[160];
        int m = snprintf(item, sizeof(item),
            "%s{\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"rssi\":%d}",
            i ? "," : "",
            s->mac[0], s->mac[1], s->mac[2],
            s->mac[3], s->mac[4], s->mac[5],
            (int)s->rssi);
        if (m <= 0 || m >= (int)sizeof(item) || httpd_resp_send_chunk(r, item, m) != ESP_OK)
            return ESP_FAIL;
    }

    if (httpd_resp_send_chunk(r, "]}", 2) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(r, NULL, 0);
}

void prisma_admin_register(httpd_handle_t server)
{
    if (!server) return;

    /* Do the only SPIFFS admin scan now, once per boot. */
    admin_storage_snapshot_init();

    httpd_uri_t page = {.uri="/admin", .method=HTTP_GET, .handler=admin_page_get};
    httpd_uri_t css = {.uri="/admin.css", .method=HTTP_GET, .handler=admin_css_get};
    httpd_uri_t js = {.uri="/admin.js", .method=HTTP_GET, .handler=admin_js_get};
    httpd_uri_t storage = {.uri="/api/admin/storage", .method=HTTP_GET, .handler=admin_storage_get};
    httpd_uri_t network = {.uri="/api/admin/network", .method=HTTP_GET, .handler=admin_network_get};

    httpd_register_uri_handler(server, &page);
    httpd_register_uri_handler(server, &css);
    httpd_register_uri_handler(server, &js);
    httpd_register_uri_handler(server, &storage);
    httpd_register_uri_handler(server, &network);
}
