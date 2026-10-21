#include "admin_shell.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

extern const uint8_t web_admin_html_start[] asm("_binary_admin_html_start");
extern const uint8_t web_admin_html_end[] asm("_binary_admin_html_end");

static const char *TAG = "ADMIN_SHELL";

static void no_cache(httpd_req_t *r)
{
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(r, "Pragma", "no-cache");
}

static const char *find_bytes(const char *data, size_t len, const char *needle)
{
    size_t n = strlen(needle);
    if (!data || !needle || !n || len < n) return NULL;
    for (size_t i = 0; i + n <= len; i++) {
        if (memcmp(data + i, needle, n) == 0) return data + i;
    }
    return NULL;
}

static esp_err_t admin_shell_get(httpd_req_t *r)
{
    const char *start = (const char *)web_admin_html_start;
    size_t len = (size_t)(web_admin_html_end - web_admin_html_start);
    const char *body = find_bytes(start, len, "</body>");

    httpd_resp_set_type(r, "text/html");
    no_cache(r);

    /* The complete FACTORY AUDIO V1 page is always the fallback. Bluetooth is
     * an optional extension and must never prevent /admin from rendering. */
    if (!body) return httpd_resp_send(r, start, (ssize_t)len);

    static const char inject[] = "<script src=\"/bluetooth.js\"></script>\n";
    const size_t inject_len = sizeof(inject) - 1;
    const size_t before = (size_t)(body - start);
    const size_t total = len + inject_len;

    /* Do not use a large chunked HTML response here. Build one fixed-length
     * response, preferring PSRAM so enabling BLE does not consume the HTTP
     * server's internal-DRAM headroom. */
    char *page = (char *)heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!page) page = (char *)malloc(total);
    if (!page) {
        ESP_LOGW(TAG, "Bluetooth shell allocation failed; serving base admin unchanged");
        return httpd_resp_send(r, start, (ssize_t)len);
    }

    memcpy(page, start, before);
    memcpy(page + before, inject, inject_len);
    memcpy(page + before + inject_len, body, len - before);

    esp_err_t err = httpd_resp_send(r, page, (ssize_t)total);
    free(page);
    return err;
}

void prisma_admin_shell_register(httpd_handle_t server)
{
    if (!server) return;
    httpd_uri_t admin = {.uri="/admin", .method=HTTP_GET, .handler=admin_shell_get};
    esp_err_t err = httpd_register_uri_handler(server, &admin);
    if (err != ESP_OK) ESP_LOGE(TAG, "cannot register /admin shell: %s", esp_err_to_name(err));
}
