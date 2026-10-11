#include "esp_http_server.h"
#include <string.h>

/* V12 is additive over V11 and remains browser-side only. */
extern esp_err_t v11_wrapped_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len);
extern esp_err_t __real_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len);

/* ESP-IDF v5.5.1 data_file_embed_asm.cmake derives symbols from the
 * embedded file basename, so web/fxlab_v12.js exports fxlab_v12_js_*.
 */
extern const unsigned char fxlab_v12_js_start[] asm("_binary_fxlab_v12_js_start");
extern const unsigned char fxlab_v12_js_end[] asm("_binary_fxlab_v12_js_end");

esp_err_t __wrap_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)
{
    if (buf && (len == HTTPD_RESP_USE_STRLEN || len < 0)) {
        const char marker[] = "v2RenderList();v2Paint();\n})();";
        if (strstr(buf, marker) != NULL) {
            const ssize_t patch_len = (ssize_t)(fxlab_v12_js_end - fxlab_v12_js_start);
            esp_err_t err = __real_httpd_resp_send_chunk(
                r,
                (const char *)fxlab_v12_js_start,
                patch_len
            );
            if (err != ESP_OK) return err;
        }
    }
    return v11_wrapped_httpd_resp_send_chunk(r, buf, len);
}
