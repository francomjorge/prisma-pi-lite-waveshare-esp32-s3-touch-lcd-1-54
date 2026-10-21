#include "bluetooth_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluetooth_service.h"
#include "security_service.h"

#define COOKIE_NAME "prisma_session"

extern const uint8_t web_bluetooth_js_start[] asm("_binary_bluetooth_js_start");
extern const uint8_t web_bluetooth_js_end[] asm("_binary_bluetooth_js_end");
extern const uint8_t web_bluetooth_css_start[] asm("_binary_bluetooth_css_start");
extern const uint8_t web_bluetooth_css_end[] asm("_binary_bluetooth_css_end");

static void no_cache(httpd_req_t *r)
{
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(r, "Pragma", "no-cache");
}

static bool cookie_token(httpd_req_t *r, char out[PRISMA_SECURITY_SESSION_HEX + 1])
{
    out[0] = 0;
    size_t len = httpd_req_get_hdr_value_len(r, "Cookie");
    if (!len || len > 512) return false;
    char buf[513];
    if (httpd_req_get_hdr_value_str(r, "Cookie", buf, sizeof(buf)) != ESP_OK) return false;
    const char *p = buf;
    size_t kl = strlen(COOKIE_NAME);
    while (*p) {
        while (*p == ' ' || *p == ';') p++;
        if (strncmp(p, COOKIE_NAME, kl) == 0 && p[kl] == '=') {
            p += kl + 1;
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

static bool authorized(httpd_req_t *r)
{
    prisma_security_status_t st;
    prisma_security_get_status(&st);
    if (!st.password_set) return true;
    char token[PRISMA_SECURITY_SESSION_HEX + 1];
    return cookie_token(r, token) && prisma_security_validate_session(token);
}

static bool require_auth(httpd_req_t *r)
{
    if (authorized(r)) return true;
    httpd_resp_set_status(r, "401 Unauthorized");
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    httpd_resp_sendstr(r, "{\"error\":\"auth_required\"}");
    return false;
}

static void json_escape(const char *src, char *dst, size_t cap)
{
    size_t j = 0;
    if (!src) src = "";
    for (size_t i = 0; src[i] && j + 1 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c == '\\' || c == '"') && j + 2 < cap) {
            dst[j++] = '\\';
            dst[j++] = (char)c;
        } else if (c >= 0x20) {
            dst[j++] = (char)c;
        }
    }
    dst[j] = 0;
}

static bool read_form(httpd_req_t *r, char *buf, size_t cap)
{
    if (!r || !buf || cap < 2 || r->content_len <= 0 || r->content_len >= (int)cap) return false;
    int left = r->content_len;
    int pos = 0;
    while (left > 0) {
        int n = httpd_req_recv(r, buf + pos, left);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) return false;
        pos += n;
        left -= n;
    }
    buf[pos] = 0;
    return true;
}

static bool form_value(const char *form, const char *key, char *out, size_t cap)
{
    if (!form || !key || !out || !cap) return false;
    return httpd_query_key_value(form, key, out, cap) == ESP_OK;
}

static esp_err_t js_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "application/javascript");
    no_cache(r);
    return httpd_resp_send(r, (const char *)web_bluetooth_js_start,
                           (ssize_t)(web_bluetooth_js_end - web_bluetooth_js_start));
}

static esp_err_t css_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/css");
    no_cache(r);
    return httpd_resp_send(r, (const char *)web_bluetooth_css_start,
                           (ssize_t)(web_bluetooth_css_end - web_bluetooth_css_start));
}

static esp_err_t status_get(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;

    prisma_bt_status_t s;
    prisma_bt_device_t devices[PRISMA_BT_MAX_DEVICES];
    prisma_bluetooth_get_status(&s);
    int count = prisma_bluetooth_get_devices(devices, PRISMA_BT_MAX_DEVICES);

    char name[80], local[40], peer[40], peer_name[80], err[200];
    json_escape(s.device_name, name, sizeof(name));
    json_escape(s.local_address, local, sizeof(local));
    json_escape(s.peer_address, peer, sizeof(peer));
    json_escape(s.peer_name, peer_name, sizeof(peer_name));
    json_escape(s.last_error, err, sizeof(err));

    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    char head[1400];
    snprintf(head, sizeof(head),
             "{\"initialized\":%s,\"synced\":%s,\"scanning\":%s,\"advertising\":%s,"
             "\"advertise_requested\":%s,\"connecting\":%s,\"connected\":%s,"
             "\"device_name\":\"%s\",\"local_address\":\"%s\","
             "\"peer_address\":\"%s\",\"peer_name\":\"%s\","
             "\"conn_handle\":%u,\"mtu\":%u,\"peer_rssi\":%d,"
             "\"encrypted\":%s,\"authenticated\":%s,\"bonded\":%s,"
             "\"last_error_code\":%d,\"last_error\":\"%s\","
             "\"capabilities\":{\"ble\":true,\"classic\":false,\"a2dp\":false,"
             "\"central\":true,\"peripheral\":true,\"observer\":true,\"bonding\":true,"
             "\"gatt_client\":true,\"gatt_server\":true,\"ble_midi_profile\":false},"
             "\"devices\":[",
             s.initialized ? "true" : "false", s.synced ? "true" : "false",
             s.scanning ? "true" : "false", s.advertising ? "true" : "false",
             s.advertise_requested ? "true" : "false", s.connecting ? "true" : "false",
             s.connected ? "true" : "false", name, local, peer, peer_name,
             (unsigned)s.conn_handle, (unsigned)s.mtu, s.peer_rssi,
             s.encrypted ? "true" : "false", s.authenticated ? "true" : "false",
             s.bonded ? "true" : "false", s.last_error_code, err);
    if (httpd_resp_send_chunk(r, head, HTTPD_RESP_USE_STRLEN) != ESP_OK) return ESP_FAIL;

    for (int i = 0; i < count; i++) {
        char dn[80], da[40], item[320];
        json_escape(devices[i].name, dn, sizeof(dn));
        json_escape(devices[i].address, da, sizeof(da));
        snprintf(item, sizeof(item),
                 "%s{\"name\":\"%s\",\"address\":\"%s\",\"addr_type\":%u,"
                 "\"rssi\":%d,\"connectable\":%s}",
                 i ? "," : "", dn, da, (unsigned)devices[i].addr_type,
                 devices[i].rssi, devices[i].connectable ? "true" : "false");
        if (httpd_resp_send_chunk(r, item, HTTPD_RESP_USE_STRLEN) != ESP_OK) return ESP_FAIL;
    }
    if (httpd_resp_send_chunk(r, "]}", 2) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(r, NULL, 0);
}

static esp_err_t scan_post(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;
    int seconds = 6;
    if (r->content_len > 0) {
        char body[128], value[16];
        if (!read_form(r, body, sizeof(body))) return httpd_resp_send_err(r, 400, "invalid form");
        if (form_value(body, "seconds", value, sizeof(value))) {
            char *end = NULL;
            long v = strtol(value, &end, 10);
            if (end == value || *end || v < 2 || v > 15) return httpd_resp_send_err(r, 400, "seconds must be 2-15");
            seconds = (int)v;
        }
    }
    esp_err_t e = prisma_bluetooth_scan_start(seconds);
    if (e == ESP_ERR_INVALID_STATE) return httpd_resp_send_err(r, 409, "BLE not ready or connection in progress");
    if (e != ESP_OK) return httpd_resp_send_err(r, 500, "scan start failed");
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, "{\"ok\":true,\"scanning\":true}");
}

static esp_err_t scan_stop_post(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;
    if (prisma_bluetooth_scan_stop() != ESP_OK) return httpd_resp_send_err(r, 500, "scan stop failed");
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, "{\"ok\":true,\"scanning\":false}");
}

static esp_err_t connect_post(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;
    char body[192], addr[40], type[16];
    if (!read_form(r, body, sizeof(body)) ||
        !form_value(body, "address", addr, sizeof(addr)) ||
        !form_value(body, "addr_type", type, sizeof(type))) {
        return httpd_resp_send_err(r, 400, "address and addr_type required");
    }
    char *end = NULL;
    long t = strtol(type, &end, 10);
    if (end == type || *end || t < 0 || t > 3) return httpd_resp_send_err(r, 400, "invalid address type");

    esp_err_t e = prisma_bluetooth_connect(addr, (uint8_t)t);
    if (e == ESP_ERR_NOT_FOUND) return httpd_resp_send_err(r, 404, "device not in current scan list");
    if (e == ESP_ERR_NOT_SUPPORTED) return httpd_resp_send_err(r, 409, "device is not connectable");
    if (e == ESP_ERR_INVALID_STATE) return httpd_resp_send_err(r, 409, "BLE busy or already connected");
    if (e != ESP_OK) return httpd_resp_send_err(r, 500, "connect request failed");
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, "{\"ok\":true,\"connecting\":true}");
}

static esp_err_t disconnect_post(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;
    esp_err_t e = prisma_bluetooth_disconnect();
    if (e == ESP_ERR_INVALID_STATE) return httpd_resp_send_err(r, 409, "not connected");
    if (e != ESP_OK) return httpd_resp_send_err(r, 500, "disconnect failed");
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, "{\"ok\":true}");
}

static esp_err_t pair_post(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;
    esp_err_t e = prisma_bluetooth_pair();
    if (e == ESP_ERR_INVALID_STATE) return httpd_resp_send_err(r, 409, "connect a BLE device first");
    if (e != ESP_OK) return httpd_resp_send_err(r, 500, "pairing failed to start");
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, "{\"ok\":true,\"pairing\":true}");
}

static esp_err_t advertise_post(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;
    char body[128], enabled[16];
    if (!read_form(r, body, sizeof(body)) || !form_value(body, "enabled", enabled, sizeof(enabled))) {
        return httpd_resp_send_err(r, 400, "enabled required");
    }
    bool on;
    if (!strcmp(enabled, "1") || !strcmp(enabled, "true")) on = true;
    else if (!strcmp(enabled, "0") || !strcmp(enabled, "false")) on = false;
    else return httpd_resp_send_err(r, 400, "enabled must be 0 or 1");

    if (prisma_bluetooth_set_advertising(on) != ESP_OK) return httpd_resp_send_err(r, 500, "advertising change failed");
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, on ? "{\"ok\":true,\"advertising_requested\":true}" : "{\"ok\":true,\"advertising_requested\":false}");
}

void prisma_bluetooth_api_register(httpd_handle_t server)
{
    if (!server) return;
    httpd_uri_t js = {.uri="/bluetooth.js", .method=HTTP_GET, .handler=js_get};
    httpd_uri_t css = {.uri="/bluetooth.css", .method=HTTP_GET, .handler=css_get};
    httpd_uri_t status = {.uri="/api/bluetooth/status", .method=HTTP_GET, .handler=status_get};
    httpd_uri_t scan = {.uri="/api/bluetooth/scan", .method=HTTP_POST, .handler=scan_post};
    httpd_uri_t scan_stop = {.uri="/api/bluetooth/scan-stop", .method=HTTP_POST, .handler=scan_stop_post};
    httpd_uri_t connect = {.uri="/api/bluetooth/connect", .method=HTTP_POST, .handler=connect_post};
    httpd_uri_t disconnect = {.uri="/api/bluetooth/disconnect", .method=HTTP_POST, .handler=disconnect_post};
    httpd_uri_t pair = {.uri="/api/bluetooth/pair", .method=HTTP_POST, .handler=pair_post};
    httpd_uri_t advertise = {.uri="/api/bluetooth/advertise", .method=HTTP_POST, .handler=advertise_post};
    httpd_register_uri_handler(server, &js);
    httpd_register_uri_handler(server, &css);
    httpd_register_uri_handler(server, &status);
    httpd_register_uri_handler(server, &scan);
    httpd_register_uri_handler(server, &scan_stop);
    httpd_register_uri_handler(server, &connect);
    httpd_register_uri_handler(server, &disconnect);
    httpd_register_uri_handler(server, &pair);
    httpd_register_uri_handler(server, &advertise);
}
