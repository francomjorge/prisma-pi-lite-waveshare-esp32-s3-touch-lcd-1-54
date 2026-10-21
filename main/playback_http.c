#include "playback_http.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "security_service.h"

#define COOKIE_NAME "prisma_session"
#define MAX_NAME 63
#define IO_CHUNK 4096

static bool cookie_token(httpd_req_t *r, char out[PRISMA_SECURITY_SESSION_HEX + 1])
{
    out[0] = 0;
    size_t len = httpd_req_get_hdr_value_len(r, "Cookie");
    if (!len || len > 512) return false;
    char buf[513];
    if (httpd_req_get_hdr_value_str(r, "Cookie", buf, sizeof(buf)) != ESP_OK) return false;
    const char *p = buf;
    size_t k = strlen(COOKIE_NAME);
    while (*p) {
        while (*p == ' ' || *p == ';') p++;
        if (!strncmp(p, COOKIE_NAME, k) && p[k] == '=') {
            p += k + 1;
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
            int a = hexval(r[1]), b = hexval(r[2]);
            if (a >= 0 && b >= 0) {
                *w++ = (char)((a << 4) | b);
                r += 3;
                continue;
            }
        }
        *w++ = (*r == '+') ? ' ' : *r++;
    }
    *w = 0;
}

static bool safe_name(char *dst, size_t cap, const char *src)
{
    if (!dst || !cap || !src || !*src) return false;
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '.' || c == '-' || c == '_') dst[j++] = (char)c;
        else if (c == ' ' || c == '+') dst[j++] = '_';
        else return false;
    }
    dst[j] = 0;
    return dst[0] && dst[0] != '.' && strstr(dst, "..") == NULL;
}

static const char *mime_for(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return "application/octet-stream";
    if (!strcasecmp(dot, ".wav")) return "audio/wav";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(dot, ".png")) return "image/png";
    if (!strcasecmp(dot, ".gif")) return "image/gif";
    if (!strcasecmp(dot, ".json")) return "application/json";
    if (!strcasecmp(dot, ".txt") || !strcasecmp(dot, ".log") || !strcasecmp(dot, ".csv")) return "text/plain";
    return "application/octet-stream";
}

static bool parse_u64(const char *s, uint64_t *v)
{
    if (!s || !*s || !v) return false;
    uint64_t out = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return false;
        uint64_t next = out * 10ULL + (uint64_t)(*p - '0');
        if (next < out) return false;
        out = next;
    }
    *v = out;
    return true;
}

/* 0=no Range, 1=valid Range, -1=invalid Range */
static int request_range(httpd_req_t *r, size_t total, size_t *start, size_t *end)
{
    *start = 0;
    *end = total ? total - 1 : 0;
    size_t n = httpd_req_get_hdr_value_len(r, "Range");
    if (!n) return 0;
    if (!total || n >= 96) return -1;

    char hdr[96];
    if (httpd_req_get_hdr_value_str(r, "Range", hdr, sizeof(hdr)) != ESP_OK) return -1;
    if (strncmp(hdr, "bytes=", 6) || strchr(hdr + 6, ',')) return -1;

    char *spec = hdr + 6;
    char *dash = strchr(spec, '-');
    if (!dash) return -1;
    *dash = 0;

    if (*spec) {
        uint64_t a = 0, b = total - 1;
        if (!parse_u64(spec, &a)) return -1;
        if (dash[1] && !parse_u64(dash + 1, &b)) return -1;
        if (a >= total || b < a) return -1;
        if (b >= total) b = total - 1;
        *start = (size_t)a;
        *end = (size_t)b;
        return 1;
    }

    uint64_t suffix = 0;
    if (!parse_u64(dash + 1, &suffix) || !suffix) return -1;
    *start = suffix >= total ? 0 : total - (size_t)suffix;
    *end = total - 1;
    return 1;
}

static esp_err_t raw_send_all(httpd_req_t *r, const char *buf, size_t len)
{
    while (len) {
        int sent = httpd_send(r, buf, len);
        if (sent <= 0) return ESP_FAIL;
        buf += sent;
        len -= (size_t)sent;
    }
    return ESP_OK;
}

static esp_err_t raw_status_only(httpd_req_t *r, const char *status, size_t total)
{
    char h[320];
    int n = snprintf(h, sizeof(h),
        "HTTP/1.1 %s\r\n"
        "Content-Length: 0\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Range: bytes */%u\r\n"
        "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
        "Pragma: no-cache\r\n\r\n",
        status, (unsigned)total);
    if (n <= 0 || n >= (int)sizeof(h)) return ESP_FAIL;
    return raw_send_all(r, h, (size_t)n);
}

static esp_err_t serve_file(httpd_req_t *r, const char *prefix, bool attachment, bool head_only)
{
    if (!require_auth(r)) return ESP_OK;

    const char *raw = r->uri + strlen(prefix);
    char decoded[96];
    strlcpy(decoded, raw, sizeof(decoded));
    char *q = strchr(decoded, '?');
    if (q) *q = 0;
    url_decode(decoded);

    char name[MAX_NAME + 1];
    if (!safe_name(name, sizeof(name), decoded)) return httpd_resp_send_err(r, 400, "invalid name");

    char path[128];
    snprintf(path, sizeof(path), "/storage/%s", name);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return httpd_resp_send_err(r, 404, "not found");

    size_t total = (size_t)st.st_size;
    size_t start = 0, end = total ? total - 1 : 0;
    int range = request_range(r, total, &start, &end);
    if (range < 0) return raw_status_only(r, "416 Range Not Satisfiable", total);

    size_t send_len = total ? end - start + 1 : 0;
    const char *status = range == 1 ? "206 Partial Content" : "200 OK";
    const char *disposition = attachment ? "attachment" : "inline";
    char range_hdr[96] = "";
    if (range == 1) {
        snprintf(range_hdr, sizeof(range_hdr), "Content-Range: bytes %u-%u/%u\r\n",
                 (unsigned)start, (unsigned)end, (unsigned)total);
    }

    char header[768];
    int hn = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %u\r\n"
        "Accept-Ranges: bytes\r\n"
        "%s"
        "Content-Disposition: %s; filename=\"%s\"\r\n"
        "Content-Transfer-Encoding: binary\r\n"
        "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
        "Pragma: no-cache\r\n\r\n",
        status, mime_for(name), (unsigned)send_len, range_hdr,
        disposition, name);
    if (hn <= 0 || hn >= (int)sizeof(header)) return ESP_FAIL;
    if (raw_send_all(r, header, (size_t)hn) != ESP_OK || head_only || !send_len) return ESP_OK;

    FILE *f = fopen(path, "rb");
    if (!f) return ESP_FAIL;
    if (start && fseek(f, (long)start, SEEK_SET) != 0) {
        fclose(f);
        return ESP_FAIL;
    }

    char buf[IO_CHUNK];
    size_t left = send_len;
    esp_err_t err = ESP_OK;
    while (left) {
        size_t want = left < sizeof(buf) ? left : sizeof(buf);
        size_t got = fread(buf, 1, want, f);
        if (!got) { err = ESP_FAIL; break; }
        if (raw_send_all(r, buf, got) != ESP_OK) { err = ESP_FAIL; break; }
        left -= got;
    }
    fclose(f);
    return err;
}

static esp_err_t media_get(httpd_req_t *r) { return serve_file(r, "/media/", false, false); }
static esp_err_t media_head(httpd_req_t *r) { return serve_file(r, "/media/", false, true); }
static esp_err_t files_get(httpd_req_t *r) { return serve_file(r, "/files/", true, false); }
static esp_err_t files_head(httpd_req_t *r) { return serve_file(r, "/files/", true, true); }

void prisma_playback_http_register(httpd_handle_t server)
{
    if (!server) return;
    httpd_uri_t media_g = {.uri="/media/*", .method=HTTP_GET, .handler=media_get};
    httpd_uri_t media_h = {.uri="/media/*", .method=HTTP_HEAD, .handler=media_head};
    httpd_uri_t files_g = {.uri="/files/*", .method=HTTP_GET, .handler=files_get};
    httpd_uri_t files_h = {.uri="/files/*", .method=HTTP_HEAD, .handler=files_head};
    httpd_register_uri_handler(server, &media_g);
    httpd_register_uri_handler(server, &media_h);
    httpd_register_uri_handler(server, &files_g);
    httpd_register_uri_handler(server, &files_h);
}
