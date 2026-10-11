#include "esp_http_server.h"
#include <string.h>

/* V12 REDO is additive over the physically validated V11 baseline. */
extern esp_err_t v11_wrapped_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len);
extern esp_err_t __real_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len);

/* ESP-IDF EMBED_TXTFILES uses the basename, then MAKE_C_IDENTIFIER(). */
extern const unsigned char fxlab_v12_redo_js_start[] asm("_binary_fxlab_v12_redo_js_start");
extern const unsigned char fxlab_v12_redo_js_end[] asm("_binary_fxlab_v12_redo_js_end");

static int v12_redo_contains(const char *buf, size_t len, const char *needle)
{
    size_t nlen;
    if (!buf || !needle) return 0;
    nlen = strlen(needle);
    if (!nlen || len < nlen) return 0;
    for (size_t i = 0; i + nlen <= len; ++i) {
        if (buf[i] == needle[0] && memcmp(buf + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

esp_err_t __wrap_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)
{
    if (buf) {
        const char marker[] = "v2RenderList();v2Paint();\n})();";
        size_t blen = (len == HTTPD_RESP_USE_STRLEN || len < 0) ? strlen(buf) : (size_t)len;
        if (v12_redo_contains(buf, blen, marker)) {
            const ssize_t patch_len = (ssize_t)(fxlab_v12_redo_js_end - fxlab_v12_redo_js_start);
            esp_err_t err = __real_httpd_resp_send_chunk(r, (const char *)fxlab_v12_redo_js_start, patch_len);
            if (err != ESP_OK) return err;
        }
    }
    return v11_wrapped_httpd_resp_send_chunk(r, buf, len);
}
