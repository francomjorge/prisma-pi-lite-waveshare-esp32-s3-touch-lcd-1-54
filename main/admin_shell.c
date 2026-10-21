#include "admin_shell.h"

#include <stddef.h>
#include <string.h>

extern const uint8_t web_admin_html_start[] asm("_binary_admin_html_start");
extern const uint8_t web_admin_html_end[] asm("_binary_admin_html_end");

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

    if (!body) return httpd_resp_send(r, start, (ssize_t)len);

    static const char inject[] = "<script src=\"/bluetooth.js\"></script>\n";
    size_t before = (size_t)(body - start);
    if (httpd_resp_send_chunk(r, start, before) != ESP_OK) return ESP_FAIL;
    if (httpd_resp_send_chunk(r, inject, sizeof(inject) - 1) != ESP_OK) return ESP_FAIL;
    if (httpd_resp_send_chunk(r, body, len - before) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(r, NULL, 0);
}

void prisma_admin_shell_register(httpd_handle_t server)
{
    if (!server) return;
    httpd_uri_t admin = {.uri="/admin", .method=HTTP_GET, .handler=admin_shell_get};
    httpd_register_uri_handler(server, &admin);
}
