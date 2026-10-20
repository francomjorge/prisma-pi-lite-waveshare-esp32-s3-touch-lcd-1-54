#include "admin_api.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "network_service.h"
#include "prisma_metrics.h"
#include "security_service.h"
#include "storage.h"

#define FACTORY_MAX_FILENAME 63
#define FACTORY_MAX_UPLOAD_BYTES (4U * 1024U * 1024U)
#define IO_CHUNK 2048
#define FORM_MAX 384
#define COOKIE_NAME "prisma_session"

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

static bool cookie_token(httpd_req_t *r, char out[PRISMA_SECURITY_SESSION_HEX + 1])
{
    out[0] = 0;
    size_t len = httpd_req_get_hdr_value_len(r, "Cookie");
    if (!len || len > 512) return false;
    char buf[513];
    if (httpd_req_get_hdr_value_str(r, "Cookie", buf, sizeof(buf)) != ESP_OK) return false;
    const char *p = buf;
    const size_t keylen = strlen(COOKIE_NAME);
    while (*p) {
        while (*p == ' ' || *p == ';') p++;
        if (strncmp(p, COOKIE_NAME, keylen) == 0 && p[keylen] == '=') {
            p += keylen + 1;
            size_t n = 0;
            while (p[n] && p[n] != ';' && n < PRISMA_SECURITY_SESSION_HEX) n++;
            if (n != PRISMA_SECURITY_SESSION_HEX) return false;
            memcpy(out, p, n);
            out[n] = 0;
            return true;
        }
        const char *semi = strchr(p, ';');
        if (!semi) break;
        p = semi + 1;
    }
    return false;
}

static bool request_authorized(httpd_req_t *r)
{
    prisma_security_status_t st;
    prisma_security_get_status(&st);
    if (!st.password_set) return true;
    char token[PRISMA_SECURITY_SESSION_HEX + 1];
    return cookie_token(r, token) && prisma_security_validate_session(token);
}

static esp_err_t require_auth(httpd_req_t *r)
{
    if (request_authorized(r)) return ESP_OK;
    httpd_resp_set_status(r, "401 Unauthorized");
    return send_json(r, "{\"error\":\"auth_required\"}");
}

static void set_session_cookie(httpd_req_t *r, const char *token)
{
    char cookie[160];
    snprintf(cookie, sizeof(cookie), COOKIE_NAME "=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=43200", token);
    httpd_resp_set_hdr(r, "Set-Cookie", cookie);
}

static void clear_session_cookie(httpd_req_t *r)
{
    httpd_resp_set_hdr(r, "Set-Cookie", COOKIE_NAME "=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
}

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

static void json_escape(const char *src, char *dst, size_t dst_size)
{
    if (!dst || dst_size == 0) return;
    size_t j = 0;
    if (!src) src = "";
    for (size_t i = 0; src[i] && j + 1 < dst_size; i++) {
        unsigned char c = (unsigned char)src[i];
        const char *esc = NULL;
        char u[7];
        if (c == '\\') esc = "\\\\";
        else if (c == '"') esc = "\\\"";
        else if (c == '\n') esc = "\\n";
        else if (c == '\r') esc = "\\r";
        else if (c == '\t') esc = "\\t";
        else if (c < 0x20) { snprintf(u, sizeof(u), "\\u%04x", c); esc = u; }
        if (esc) {
            size_t n = strlen(esc);
            if (j + n >= dst_size) break;
            memcpy(dst + j, esc, n); j += n;
        } else dst[j++] = (char)c;
    }
    dst[j] = 0;
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
    char *rd = s, *wr = s;
    while (*rd) {
        if (*rd == '%' && rd[1] && rd[2]) {
            int hi = hexval(rd[1]), lo = hexval(rd[2]);
            if (hi >= 0 && lo >= 0) { *wr++ = (char)((hi << 4) | lo); rd += 3; continue; }
        }
        *wr++ = (*rd == '+') ? ' ' : *rd;
        rd++;
    }
    *wr = 0;
}

static bool read_form(httpd_req_t *r, char *buf, size_t buf_size)
{
    if (!r || !buf || buf_size < 2 || r->content_len <= 0 || r->content_len >= (int)buf_size) return false;
    int left = r->content_len, pos = 0;
    while (left > 0) {
        int got = httpd_req_recv(r, buf + pos, left);
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (got <= 0) return false;
        pos += got; left -= got;
    }
    buf[pos] = 0;
    return true;
}

static bool form_value(const char *form, const char *key, char *out, size_t out_size)
{
    if (!form || !key || !out || out_size == 0) return false;
    if (httpd_query_key_value(form, key, out, out_size) != ESP_OK) return false;
    url_decode(out);
    return true;
}

static esp_err_t security_status_get(httpd_req_t *r)
{
    prisma_security_status_t st;
    prisma_security_get_status(&st);
    bool authenticated = request_authorized(r);
    char b[320];
    snprintf(b, sizeof(b),
        "{\"user\":\"admin\",\"password_set\":%s,\"authenticated\":%s,\"active_sessions\":%u,"
        "\"recovery_available\":%s,\"recovery\":\"Hold BOOT for 8 seconds while firmware is running\"}",
        st.password_set ? "true" : "false", authenticated ? "true" : "false", (unsigned)st.active_sessions,
        st.recovery_available ? "true" : "false");
    return send_json(r, b);
}

static esp_err_t security_login_post(httpd_req_t *r)
{
    char form[FORM_MAX], user[32], password[80];
    if (!read_form(r, form, sizeof(form))) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "form required");
    user[0] = password[0] = 0;
    form_value(form, "user", user, sizeof(user));
    form_value(form, "password", password, sizeof(password));
    if (strcmp(user, PRISMA_SECURITY_USERNAME) != 0) {
        httpd_resp_set_status(r, "401 Unauthorized");
        return send_json(r, "{\"error\":\"invalid_credentials\"}");
    }
    char token[PRISMA_SECURITY_SESSION_HEX + 1];
    if (!prisma_security_create_session(password, token)) {
        httpd_resp_set_status(r, "401 Unauthorized");
        return send_json(r, "{\"error\":\"invalid_credentials\"}");
    }
    set_session_cookie(r, token);
    return send_json(r, "{\"ok\":true,\"user\":\"admin\"}");
}

static esp_err_t security_password_post(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    char form[FORM_MAX], current[80], next[80];
    if (!read_form(r, form, sizeof(form))) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "form required");
    current[0] = next[0] = 0;
    form_value(form, "current", current, sizeof(current));
    if (!form_value(form, "new", next, sizeof(next))) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "new password required");
    if (!prisma_security_set_password(current, next))
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "current password invalid or new password must be 8-63 chars");
    char token[PRISMA_SECURITY_SESSION_HEX + 1];
    if (prisma_security_create_session(next, token)) set_session_cookie(r, token);
    return send_json(r, "{\"ok\":true,\"password_set\":true}");
}

static esp_err_t security_remove_post(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    char form[FORM_MAX], current[80];
    if (!read_form(r, form, sizeof(form))) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "form required");
    current[0] = 0;
    form_value(form, "current", current, sizeof(current));
    if (!prisma_security_remove_password(current)) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "current password invalid");
    clear_session_cookie(r);
    return send_json(r, "{\"ok\":true,\"password_set\":false}");
}

static esp_err_t security_logout_post(httpd_req_t *r)
{
    char token[PRISMA_SECURITY_SESSION_HEX + 1];
    if (cookie_token(r, token)) prisma_security_logout_token(token);
    clear_session_cookie(r);
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t security_logout_all_post(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    prisma_security_invalidate_sessions();
    clear_session_cookie(r);
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t overview_get(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    float temp = 0.0f;
    bool temp_ok = prisma_metrics_chip_temperature(&temp);
    const esp_app_desc_t *app = esp_app_get_description();
    char b[768];
    snprintf(b, sizeof(b),
        "{\"version\":\"PRISMA-PI LITE ADMIN V2 A2\","
        "\"idf\":\"%s\",\"build_date\":\"%s\",\"build_time\":\"%s\","
        "\"uptime_ms\":%llu,\"heap_free\":%u,\"heap_min\":%u,"
        "\"reset_reason\":\"%s\",\"flash_bytes\":%u,\"psram_bytes\":%u,"
        "\"chip_temp\":{\"present\":%s,\"c\":%.1f}}",
        esp_get_idf_version(), app ? app->date : "", app ? app->time : "",
        (unsigned long long)(esp_timer_get_time() / 1000ULL),
        (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
        reset_reason_name(esp_reset_reason()), (unsigned)prisma_metrics_flash_size(), (unsigned)prisma_metrics_psram_size(),
        temp_ok ? "true" : "false", temp_ok ? temp : 0.0f);
    return send_json(r, b);
}

static esp_err_t network_status_get(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    prisma_network_status_t s;
    prisma_network_get_status(&s);
    char ap_ssid[80], sta_ssid[80];
    json_escape(s.ap_ssid, ap_ssid, sizeof(ap_ssid));
    json_escape(s.sta_ssid, sta_ssid, sizeof(sta_ssid));
    char b[1400];
    snprintf(b, sizeof(b),
        "{\"mode\":\"AP+STA\","
        "\"ap\":{\"running\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"mac\":\"%s\",\"channel\":%u,\"clients\":%u,\"secured\":%s},"
        "\"sta\":{\"configured\":%s,\"connected\":%s,\"got_ip\":%s,\"auto_connect\":%s,"
        "\"ssid\":\"%s\",\"ip\":\"%s\",\"gateway\":\"%s\",\"netmask\":\"%s\",\"mac\":\"%s\","
        "\"rssi\":%d,\"channel\":%u,\"last_disconnect_reason\":%u}}",
        s.ap_running ? "true" : "false", ap_ssid, s.ap_ip, s.ap_mac, (unsigned)s.ap_channel, (unsigned)s.ap_clients, s.ap_secured ? "true" : "false",
        s.sta_configured ? "true" : "false", s.sta_connected ? "true" : "false", s.sta_got_ip ? "true" : "false", s.sta_auto_connect ? "true" : "false",
        sta_ssid, s.sta_ip, s.sta_gateway, s.sta_netmask, s.sta_mac, (int)s.sta_rssi, (unsigned)s.sta_channel, (unsigned)s.last_disconnect_reason);
    return send_json(r, b);
}

static const char *auth_name(uint8_t auth)
{
    switch (auth) {
        case 0: return "OPEN"; case 1: return "WEP"; case 2: return "WPA-PSK"; case 3: return "WPA2-PSK";
        case 4: return "WPA/WPA2"; case 5: return "WPA2-ENTERPRISE"; case 6: return "WPA3-PSK"; case 7: return "WPA2/WPA3";
        default: return "SECURED";
    }
}

static esp_err_t network_scan_get(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    prisma_network_ap_t aps[PRISMA_NETWORK_MAX_SCAN];
    memset(aps, 0, sizeof(aps));
    int count = prisma_network_scan(aps, PRISMA_NETWORK_MAX_SCAN);
    if (count < 0) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
    httpd_resp_set_type(r, "application/json"); no_cache(r);
    if (httpd_resp_send_chunk(r, "{\"networks\":[", HTTPD_RESP_USE_STRLEN) != ESP_OK) return ESP_FAIL;
    for (int i = 0; i < count; i++) {
        char esc[80], item[240];
        json_escape(aps[i].ssid, esc, sizeof(esc));
        snprintf(item, sizeof(item), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%u,\"auth\":\"%s\",\"open\":%s}",
                 i ? "," : "", esc, (int)aps[i].rssi, (unsigned)aps[i].channel, auth_name(aps[i].authmode), aps[i].authmode == 0 ? "true" : "false");
        if (httpd_resp_send_chunk(r, item, HTTPD_RESP_USE_STRLEN) != ESP_OK) return ESP_FAIL;
    }
    if (httpd_resp_send_chunk(r, "]}", 2) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(r, NULL, 0);
}

static esp_err_t network_connect_post(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    char form[FORM_MAX], ssid[33], password[65];
    if (!read_form(r, form, sizeof(form)) || !form_value(form, "ssid", ssid, sizeof(ssid))) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "ssid required");
    password[0] = 0; form_value(form, "password", password, sizeof(password));
    if (prisma_network_connect(ssid, password) != ESP_OK) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "connect rejected");
    return send_json(r, "{\"ok\":true,\"state\":\"connecting\"}");
}

static esp_err_t network_disconnect_post(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    if (prisma_network_disconnect() != ESP_OK) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "disconnect failed");
    return send_json(r, "{\"ok\":true}");
}
static esp_err_t network_reconnect_post(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    if (prisma_network_reconnect() != ESP_OK) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "no saved network");
    return send_json(r, "{\"ok\":true,\"state\":\"connecting\"}");
}
static esp_err_t network_forget_post(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    if (prisma_network_forget() != ESP_OK) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "forget failed");
    return send_json(r, "{\"ok\":true}");
}
static esp_err_t network_ap_post(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    char form[FORM_MAX], ssid[33], password[65];
    if (!read_form(r, form, sizeof(form)) || !form_value(form, "ssid", ssid, sizeof(ssid))) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "ssid required");
    password[0] = 0; form_value(form, "password", password, sizeof(password));
    if (prisma_network_set_ap(ssid, password) != ESP_OK) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "AP password must be empty or 8-63 chars");
    return send_json(r, "{\"ok\":true,\"warning\":\"AP settings applied; reconnect if your session drops\"}");
}

static bool visible_file(const char *name) { return name && name[0] && name[0] != '.' && strcmp(name, "upload.tmp") != 0; }
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
    return dst[0] && dst[0] != '.' && strstr(dst, "..") == NULL;
}
static bool query_name(httpd_req_t *r, char *out, size_t out_size)
{
    char query[160], raw[96];
    if (httpd_req_get_url_query_str(r, query, sizeof(query)) != ESP_OK) return false;
    if (httpd_query_key_value(query, "name", raw, sizeof(raw)) != ESP_OK) return false;
    url_decode(raw); return sanitize_name(out, out_size, raw);
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
    if (require_auth(r) != ESP_OK) return ESP_OK;
    size_t total = 0, used = 0;
    if (!prisma_storage_info(&total, &used)) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "storage unavailable");
    httpd_resp_set_type(r, "application/json"); no_cache(r);
    char head[256];
    snprintf(head, sizeof(head), "{\"total\":%u,\"used\":%u,\"free\":%u,\"max_files\":%d,\"files\":[",
             (unsigned)total, (unsigned)used, (unsigned)(total > used ? total - used : 0), prisma_storage_max_files());
    if (httpd_resp_send_chunk(r, head, HTTPD_RESP_USE_STRLEN) != ESP_OK) return ESP_FAIL;
    DIR *dir = opendir("/storage"); bool first = true;
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!visible_file(entry->d_name)) continue;
            char path[128]; if (snprintf(path, sizeof(path), "/storage/%s", entry->d_name) >= (int)sizeof(path)) continue;
            struct stat st; if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            char esc[160], item[320]; json_escape(entry->d_name, esc, sizeof(esc));
            snprintf(item, sizeof(item), "%s{\"name\":\"%s\",\"size\":%u,\"type\":\"%s\"}", first ? "" : ",", esc, (unsigned)st.st_size, mime_for(entry->d_name));
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
    if (require_auth(r) != ESP_OK) return ESP_OK;
    char name[FACTORY_MAX_FILENAME + 1];
    if (!query_name(r, name, sizeof(name))) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "invalid name");
    if (r->content_len <= 0 || r->content_len > FACTORY_MAX_UPLOAD_BYTES) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "invalid size");
    char target[128]; snprintf(target, sizeof(target), "/storage/%s", name);
    struct stat existing; bool replacing = stat(target, &existing) == 0;
    if (!replacing && prisma_storage_file_count() >= prisma_storage_max_files()) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "four-file limit reached");
    const char *tmp = "/storage/upload.tmp"; remove(tmp);
    FILE *f = fopen(tmp, "wb"); if (!f) return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "cannot create file");
    char buf[IO_CHUNK]; int remaining = r->content_len; bool ok = true;
    while (remaining > 0) {
        int want = remaining > (int)sizeof(buf) ? (int)sizeof(buf) : remaining;
        int got = httpd_req_recv(r, buf, want);
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (got <= 0 || fwrite(buf, 1, got, f) != (size_t)got) { ok = false; break; }
        remaining -= got;
    }
    fclose(f);
    if (!ok || remaining != 0) { remove(tmp); return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "upload failed"); }
    if (replacing) remove(target);
    if (rename(tmp, target) != 0) { remove(tmp); return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "commit failed"); }
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t file_delete(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    char name[FACTORY_MAX_FILENAME + 1];
    if (!query_name(r, name, sizeof(name))) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "invalid name");
    char path[128]; snprintf(path, sizeof(path), "/storage/%s", name);
    if (remove(path) != 0) return httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "not found");
    return send_json(r, "{\"ok\":true}");
}

static esp_err_t file_download_get(httpd_req_t *r)
{
    if (require_auth(r) != ESP_OK) return ESP_OK;
    const char *prefix = "/files/"; const char *raw = r->uri + strlen(prefix);
    char decoded[96]; strlcpy(decoded, raw, sizeof(decoded));
    char *q = strchr(decoded, '?'); if (q) *q = 0; url_decode(decoded);
    char name[FACTORY_MAX_FILENAME + 1];
    if (!sanitize_name(name, sizeof(name), decoded)) return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "invalid name");
    char path[128]; snprintf(path, sizeof(path), "/storage/%s", name);
    FILE *f = fopen(path, "rb"); if (!f) return httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "not found");
    httpd_resp_set_type(r, mime_for(name)); no_cache(r);
    char disp[128]; snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", name); httpd_resp_set_hdr(r, "Content-Disposition", disp);
    char buf[IO_CHUNK]; size_t n; esp_err_t err = ESP_OK;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) if (httpd_resp_send_chunk(r, buf, n) != ESP_OK) { err = ESP_FAIL; break; }
    fclose(f); if (err != ESP_OK) return err; return httpd_resp_send_chunk(r, NULL, 0);
}

void prisma_admin_register(httpd_handle_t server)
{
    if (!server) return;
    httpd_uri_t admin = {.uri="/admin",.method=HTTP_GET,.handler=admin_page_get};
    httpd_uri_t css = {.uri="/admin.css",.method=HTTP_GET,.handler=admin_css_get};
    httpd_uri_t sec_status = {.uri="/api/security/status",.method=HTTP_GET,.handler=security_status_get};
    httpd_uri_t sec_login = {.uri="/api/security/login",.method=HTTP_POST,.handler=security_login_post};
    httpd_uri_t sec_password = {.uri="/api/security/password",.method=HTTP_POST,.handler=security_password_post};
    httpd_uri_t sec_remove = {.uri="/api/security/remove",.method=HTTP_POST,.handler=security_remove_post};
    httpd_uri_t sec_logout = {.uri="/api/security/logout",.method=HTTP_POST,.handler=security_logout_post};
    httpd_uri_t sec_logout_all = {.uri="/api/security/logout-all",.method=HTTP_POST,.handler=security_logout_all_post};
    httpd_uri_t overview = {.uri="/api/admin/overview",.method=HTTP_GET,.handler=overview_get};
    httpd_uri_t network_legacy = {.uri="/api/admin/network",.method=HTTP_GET,.handler=network_status_get};
    httpd_uri_t network_status = {.uri="/api/network/status",.method=HTTP_GET,.handler=network_status_get};
    httpd_uri_t network_scan = {.uri="/api/network/scan",.method=HTTP_GET,.handler=network_scan_get};
    httpd_uri_t network_connect = {.uri="/api/network/connect",.method=HTTP_POST,.handler=network_connect_post};
    httpd_uri_t network_disconnect = {.uri="/api/network/disconnect",.method=HTTP_POST,.handler=network_disconnect_post};
    httpd_uri_t network_reconnect = {.uri="/api/network/reconnect",.method=HTTP_POST,.handler=network_reconnect_post};
    httpd_uri_t network_forget = {.uri="/api/network/forget",.method=HTTP_POST,.handler=network_forget_post};
    httpd_uri_t network_ap = {.uri="/api/network/ap",.method=HTTP_POST,.handler=network_ap_post};
    httpd_uri_t files_get = {.uri="/api/files",.method=HTTP_GET,.handler=files_list_get};
    httpd_uri_t files_post = {.uri="/api/files",.method=HTTP_POST,.handler=file_upload_post};
    httpd_uri_t files_delete = {.uri="/api/files",.method=HTTP_DELETE,.handler=file_delete};
    httpd_uri_t files_download = {.uri="/files/*",.method=HTTP_GET,.handler=file_download_get};
    httpd_uri_t handlers[] = {admin,css,sec_status,sec_login,sec_password,sec_remove,sec_logout,sec_logout_all,overview,network_legacy,network_status,network_scan,network_connect,network_disconnect,network_reconnect,network_forget,network_ap,files_get,files_post,files_delete,files_download};
    for (size_t i = 0; i < sizeof(handlers)/sizeof(handlers[0]); i++) httpd_register_uri_handler(server, &handlers[i]);
}
