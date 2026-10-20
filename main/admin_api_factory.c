#include "admin_api.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "esp_app_desc.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "prisma_metrics.h"
#include "storage.h"

#define FACTORY_MAX_FILENAME 63
#define FACTORY_MAX_UPLOAD_BYTES (4U * 1024U * 1024U)
#define IO_CHUNK 2048

extern const uint8_t web_admin_html_start[] asm("_binary_admin_html_start");
extern const uint8_t web_admin_html_end[] asm("_binary_admin_html_end");
extern const uint8_t web_admin_css_start[] asm("_binary_admin_css_start");
extern const uint8_t web_admin_css_end[] asm("_binary_admin_css_end");

static void no_cache(httpd_req_t *r)
{
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(r, "Pragma", "no-cache");
}

static esp_err_t send_embedded(httpd_req_t *r, const char *type, const uint8_t *s, const uint8_t *e)
{
    httpd_resp_set_type(r, type);
    no_cache(r);
    return httpd_resp_send(r, (const char *)s, (ssize_t)(e - s));
}

static esp_err_t send_json(httpd_req_t *r, const char *json)
{
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, json);
}

static esp_err_t admin_page_get(httpd_req_t *r) { return send_embedded(r, "text/html", web_admin_html_start, web_admin_html_end); }
static esp_err_t admin_css_get(httpd_req_t *r) { return send_embedded(r, "text/css", web_admin_css_start, web_admin_css_end); }

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
        default: return "UNKNOWN";
    }
}

static void get_ap_ip(char *ipbuf, size_t size)
{
    strlcpy(ipbuf, "192.168.4.1", size);
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif) {
        esp_netif_ip_info_t ip = {0};
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK)
            snprintf(ipbuf, size, IPSTR, IP2STR(&ip.ip));
    }
}

static esp_err_t overview_get(httpd_req_t *r)
{
    float temp = 0.0f;
    bool temp_ok = prisma_metrics_chip_temperature(&temp);
    const esp_app_desc_t *app = esp_app_get_description();
    char b[768];
    snprintf(b, sizeof(b),
        "{\"version\":\"PRISMA-PI LITE FACTORY V1\","
        "\"idf\":\"%s\",\"build_date\":\"%s\",\"build_time\":\"%s\","
        "\"uptime_ms\":%llu,\"heap_free\":%u,\"heap_min\":%u,"
        "\"reset_reason\":\"%s\",\"flash_bytes\":%u,\"psram_bytes\":%u,"
        "\"chip_temp\":{\"present\":%s,\"c\":%.1f}}",
        esp_get_idf_version(), app ? app->date : "", app ? app->time : "",
        (unsigned long long)(esp_timer_get_time() / 1000ULL),
        (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
        reset_reason_name(esp_reset_reason()),
        (unsigned)prisma_metrics_flash_size(), (unsigned)prisma_metrics_psram_size(),
        temp_ok ? "true" : "false", temp_ok ? temp : 0.0f);
    return send_json(r, b);
}

static esp_err_t network_get(httpd_req_t *r)
{
    wifi_config_t cfg = {0};
    if (esp_wifi_get_config(WIFI_IF_AP, &cfg) != ESP_OK)
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "wifi unavailable");

    uint8_t channel = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&channel, &second);
    wifi_sta_list_t stations = {0};
    esp_wifi_ap_get_sta_list(&stations);
    char ip[20];
    get_ap_ip(ip, sizeof(ip));

    char b[384];
    snprintf(b, sizeof(b),
        "{\"ssid\":\"%s\",\"ip\":\"%s\",\"channel\":%u,\"client_count\":%u,\"max_clients\":%u}",
        (char *)cfg.ap.ssid, ip, (unsigned)channel, (unsigned)stations.num, (unsigned)cfg.ap.max_connection);
    return send_json(r, b);
}

static bool visible_file(const char *name)
{
    return name && name[0] && name[0] != '.' && strcmp(name, "upload.tmp") != 0;
}

static bool sanitize_name(char *dst, size_t dst_size, const char *src)
{
    if (!src || !*src || dst_size < 2) return false;
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '.' || c == '-' || c == '_') dst[j++] = (char)c;
        else if (c == ' ' || c == '+') dst[j++] = '_';
        else return false;
    }
    dst[j] = 0;
    if (!dst[0] || dst[0] == '.' || strstr(dst, "..") != NULL) return false;
    return true;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *s)
{
    char *r = s, *w = s;
    while (*r) {
        if (*r == '%' && r[1] && r[2]) {
            int hi = hexval(r[1]), lo = hexval(r[2]);
            if (hi >= 0 && lo >= 0) { *w++ = (char)((hi << 4) | lo); r += 3; continue; }
        }
        *w++ = (*r == '+') ? ' ' : *r;
        r++;
    }
    *w = 0;
}

static bool query_name(httpd_req_t *r, char *out, size_t out_size)
{
    char query[160], raw[96];
    if (httpd_req_get_url_query_str(r, query, sizeof(query)) != ESP_OK) return false;
    if (httpd_query_key_value(query, "name", raw, sizeof(raw)) != ESP_OK) return false;
    url_decode(raw);
    return sanitize_name(out, out_size, raw);
}

static const char *mime_for(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return "application/octet-stream";
    if (!strcasecmp(dot, ".wav")) return "audio/wav";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(dot, ".png")) return "image/png";
    if (!strcasecmp(dot, ".json")) return "application/json";
    if (!strcasecmp(dot, ".txt")) return "text/plain";
    return "application/octet-stream";
}

static esp_err_t files_list_get(httpd_req_t *r)
{
    size_t total = 0, used = 0;
    if (!prisma_storage_info(&total, &used))
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "storage unavailable");

    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    char head[256];
    snprintf(head, sizeof(head),
        "{\"total\":%u,\"used\":%u,\"free\":%u,\"max_files\":%d,\"files\":[",
        (unsigned)total, (unsigned)used, (unsigned)(total > used ? total - used : 0), prisma_storage_max_files());
    if (httpd_resp_send_chunk(r, head, HTTPD_RESP_USE_STRLEN) != ESP_OK) return ESP_FAIL;

    DIR *dir = opendir("/storage");
    bool first = true;
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!visible_file(entry->d_name)) continue;
            char path[128];
            snprintf(path, sizeof(path), "/storage/%s", entry->d_name);
            struct stat st;
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            char item[256];
            snprintf(item, sizeof(item), "%s{\"name\":\"%s\",\"size\":%u}",
                     first ? "" : ",", entry->d_name, (unsigned)st.st_size);
            first = false;
            if (httpd_resp_send_chunk(r, item, HTTPD_RESP_USE_STRLEN) != ESP_OK) { closedir(dir); return ESP_FAIL; }
        }
        closedir(dir);
    }
    if (httpd_resp_send_chunk(r, "]}", 2) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(r, NULL, 0);
}

static esp_err_t file_upload_post(httpd_req_t *r)
{
    char name[FACTORY_MAX_FILENAME + 1];
    if (!query_name(r, name, sizeof(name)))
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "invalid name");
    if (r->content_len <= 0 || r->content_len > FACTORY_MAX_UPLOAD_BYTES)
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "invalid size");

    char target[128];
    snprintf(target, sizeof(target), "/storage/%s", name);
    struct stat existing;
    bool replacing = stat(target, &existing) == 0;
    if (!replacing && prisma_storage_file_count() >= prisma_storage_max_files())
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "four-file limit reached");

    const char *tmp = "/storage/upload.tmp";
    remove(tmp);
    FILE *f = fopen(tmp, "wb");
    if (!f) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "cannot create file");

    char buf[IO_CHUNK];
    int remaining = r->content_len;
    bool ok = true;
    while (remaining > 0) {
        int want = remaining > (int)sizeof(buf) ? (int)sizeof(buf) : remaining;
        int got = httpd_req_recv(r, buf, want);
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (got <= 0 || fwrite(buf, 1, got, f) != (size_t)got) { ok = false; break; }
        remaining -= got;
    }
    fclose(f);

    if (!ok || remaining != 0) {
        remove(tmp);
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "upload failed");
    }

    if (replacing) remove(target);
    if (rename(tmp, target) != 0) {
        remove(tmp);
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "commit failed");
    }

    return send_json(r, "{\"ok\":true}");
}

static esp_err_t file_delete(httpd_req_t *r)
{
    char name[FACTORY_MAX_FILENAME + 1];
    if (!query_name(r, name, sizeof(name)))
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "invalid name");
    char path[128];
    snprintf(path, sizeof(path), "/storage/%s", name);
    if (remove(path) != 0)
        return httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "not found");
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t file_download_get(httpd_req_t *r)
{
    const char *prefix = "/files/";
    const char *raw = r->uri + strlen(prefix);
    char decoded[96];
    strlcpy(decoded, raw, sizeof(decoded));
    char *q = strchr(decoded, '?');
    if (q) *q = 0;
    url_decode(decoded);

    char name[FACTORY_MAX_FILENAME + 1];
    if (!sanitize_name(name, sizeof(name), decoded))
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "invalid name");

    char path[128];
    snprintf(path, sizeof(path), "/storage/%s", name);
    FILE *f = fopen(path, "rb");
    if (!f) return httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "not found");

    httpd_resp_set_type(r, mime_for(name));
    no_cache(r);
    char disp[128];
    snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", name);
    httpd_resp_set_hdr(r, "Content-Disposition", disp);

    char buf[IO_CHUNK];
    size_t n;
    esp_err_t err = ESP_OK;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(r, buf, n) != ESP_OK) { err = ESP_FAIL; break; }
    }
    fclose(f);
    if (err != ESP_OK) return err;
    return httpd_resp_send_chunk(r, NULL, 0);
}

void prisma_admin_register(httpd_handle_t server)
{
    if (!server) return;

    httpd_uri_t admin = {.uri="/admin",.method=HTTP_GET,.handler=admin_page_get};
    httpd_uri_t css = {.uri="/admin.css",.method=HTTP_GET,.handler=admin_css_get};
    httpd_uri_t overview = {.uri="/api/admin/overview",.method=HTTP_GET,.handler=overview_get};
    httpd_uri_t network = {.uri="/api/admin/network",.method=HTTP_GET,.handler=network_get};
    httpd_uri_t files_get = {.uri="/api/files",.method=HTTP_GET,.handler=files_list_get};
    httpd_uri_t files_post = {.uri="/api/files",.method=HTTP_POST,.handler=file_upload_post};
    httpd_uri_t files_delete = {.uri="/api/files",.method=HTTP_DELETE,.handler=file_delete};
    httpd_uri_t files_download = {.uri="/files/*",.method=HTTP_GET,.handler=file_download_get};

    httpd_register_uri_handler(server, &admin);
    httpd_register_uri_handler(server, &css);
    httpd_register_uri_handler(server, &overview);
    httpd_register_uri_handler(server, &network);
    httpd_register_uri_handler(server, &files_get);
    httpd_register_uri_handler(server, &files_post);
    httpd_register_uri_handler(server, &files_delete);
    httpd_register_uri_handler(server, &files_download);
}
