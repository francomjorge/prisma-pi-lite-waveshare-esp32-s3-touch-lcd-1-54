#include "mic_api.h"

#include <stdlib.h>
#include <string.h>

#include "mic_service.h"
#include "security_service.h"

#define COOKIE_NAME "prisma_session"

extern const uint8_t web_mic_html_start[] asm("_binary_mic_html_start");
extern const uint8_t web_mic_html_end[] asm("_binary_mic_html_end");
extern const uint8_t web_mic_css_start[] asm("_binary_mic_css_start");
extern const uint8_t web_mic_css_end[] asm("_binary_mic_css_end");

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
    httpd_resp_sendstr(r, "{\"error\":\"auth_required\"}");
    return false;
}

static esp_err_t page_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html");
    no_cache(r);
    return httpd_resp_send(r, (const char *)web_mic_html_start,
                           (ssize_t)(web_mic_html_end - web_mic_html_start));
}

static esp_err_t css_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/css");
    no_cache(r);
    return httpd_resp_send(r, (const char *)web_mic_css_start,
                           (ssize_t)(web_mic_css_end - web_mic_css_start));
}

static esp_err_t status_get(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;
    prisma_mic_status_t s;
    prisma_mic_get_status(&s);

    char b[640];
    snprintf(b, sizeof(b),
             "{\"initialized\":%s,\"recording\":%s,\"level\":%d,\"peak\":%d,"
             "\"gain_db\":%d,\"record_ms\":%d,\"record_target_ms\":%d,"
             "\"record_name\":\"%s\",\"last_error\":\"%s\","
             "\"rate\":16000,\"bits\":16,\"channels\":1}",
             s.initialized ? "true" : "false",
             s.recording ? "true" : "false",
             s.level, s.peak, s.gain_db, s.record_ms, s.record_target_ms,
             s.record_name, s.last_error);
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, b);
}

static bool form_seconds(httpd_req_t *r, int *seconds)
{
    *seconds = 5;
    if (r->content_len <= 0) return true;
    if (r->content_len > 127) return false;

    char body[128];
    int left = r->content_len, pos = 0;
    while (left > 0) {
        int n = httpd_req_recv(r, body + pos, left);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) return false;
        pos += n;
        left -= n;
    }
    body[pos] = 0;

    char value[16];
    if (httpd_query_key_value(body, "seconds", value, sizeof(value)) != ESP_OK) return true;
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end || parsed < 1 || parsed > 10) return false;
    *seconds = (int)parsed;
    return true;
}

static esp_err_t record_post(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;
    int seconds = 5;
    if (!form_seconds(r, &seconds)) return httpd_resp_send_err(r, 400, "seconds must be 1-10");

    esp_err_t err = prisma_mic_record_start(seconds);
    if (err == ESP_ERR_NO_MEM) return httpd_resp_send_err(r, 507, "storage full or not enough free space");
    if (err == ESP_ERR_INVALID_STATE) return httpd_resp_send_err(r, 409, "microphone unavailable, busy, or speaker playback active");
    if (err != ESP_OK) return httpd_resp_send_err(r, 500, "cannot start recording");

    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, "{\"ok\":true,\"recording\":true}");
}

static esp_err_t stop_post(httpd_req_t *r)
{
    if (!require_auth(r)) return ESP_OK;
    prisma_mic_record_stop();
    httpd_resp_set_type(r, "application/json");
    no_cache(r);
    return httpd_resp_sendstr(r, "{\"ok\":true}");
}

void prisma_mic_api_register(httpd_handle_t server)
{
    if (!server) return;
    httpd_uri_t page = {.uri="/mic", .method=HTTP_GET, .handler=page_get};
    httpd_uri_t css = {.uri="/mic.css", .method=HTTP_GET, .handler=css_get};
    httpd_uri_t status = {.uri="/api/mic/status", .method=HTTP_GET, .handler=status_get};
    httpd_uri_t record = {.uri="/api/mic/record", .method=HTTP_POST, .handler=record_post};
    httpd_uri_t stop = {.uri="/api/mic/stop", .method=HTTP_POST, .handler=stop_post};
    httpd_register_uri_handler(server, &page);
    httpd_register_uri_handler(server, &css);
    httpd_register_uri_handler(server, &status);
    httpd_register_uri_handler(server, &record);
    httpd_register_uri_handler(server, &stop);
}
