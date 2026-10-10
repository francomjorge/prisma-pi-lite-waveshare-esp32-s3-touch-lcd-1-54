#define admin_page_get admin_page_get_unauthed
#define admin_css_get admin_css_get_unauthed
#define admin_js_get admin_js_get_unauthed
#define admin_overview_get admin_overview_get_unauthed
#define admin_status_get admin_status_get_unauthed
#define admin_sensors_get admin_sensors_get_unauthed
#define admin_storage_get admin_storage_get_unauthed
#define admin_network_get admin_network_get_unauthed
#define prisma_admin_register prisma_admin_register_unauthed
#include "admin_api.c"
#undef prisma_admin_register
#undef admin_network_get
#undef admin_storage_get
#undef admin_sensors_get
#undef admin_status_get
#undef admin_overview_get
#undef admin_js_get
#undef admin_css_get
#undef admin_page_get

#define ADMIN_AUTH_HEADER "Basic cHJpc21hLXBpOnByaXNtYS1waQ=="
#define ADMIN_AUTH_REALM "Basic realm=\"PRISMA-PI ADMIN\""

static esp_err_t admin_unauthorized(httpd_req_t *r)
{
    httpd_resp_set_status(r, "401 Unauthorized");
    httpd_resp_set_hdr(r, "WWW-Authenticate", ADMIN_AUTH_REALM);
    httpd_resp_set_type(r, "text/plain");
    no_cache(r);
    return httpd_resp_sendstr(r, "Authentication required");
}

static bool admin_auth_ok(httpd_req_t *r)
{
    char value[96];
    size_t len = httpd_req_get_hdr_value_len(r, "Authorization");
    if (len == 0 || len >= sizeof(value)) return false;
    if (httpd_req_get_hdr_value_str(r, "Authorization", value, sizeof(value)) != ESP_OK)
        return false;
    return strcmp(value, ADMIN_AUTH_HEADER) == 0;
}

static esp_err_t admin_page_get(httpd_req_t *r)
{
    if (!admin_auth_ok(r)) return admin_unauthorized(r);
    return admin_page_get_unauthed(r);
}

static esp_err_t admin_css_get(httpd_req_t *r)
{
    if (!admin_auth_ok(r)) return admin_unauthorized(r);
    return admin_css_get_unauthed(r);
}

static esp_err_t admin_js_get(httpd_req_t *r)
{
    if (!admin_auth_ok(r)) return admin_unauthorized(r);
    return admin_js_get_unauthed(r);
}

static esp_err_t admin_overview_get(httpd_req_t *r)
{
    if (!admin_auth_ok(r)) return admin_unauthorized(r);
    return admin_overview_get_unauthed(r);
}

static esp_err_t admin_status_get(httpd_req_t *r)
{
    if (!admin_auth_ok(r)) return admin_unauthorized(r);
    return admin_status_get_unauthed(r);
}

static esp_err_t admin_sensors_get(httpd_req_t *r)
{
    if (!admin_auth_ok(r)) return admin_unauthorized(r);
    return admin_sensors_get_unauthed(r);
}

static esp_err_t admin_storage_get(httpd_req_t *r)
{
    if (!admin_auth_ok(r)) return admin_unauthorized(r);
    return admin_storage_get_unauthed(r);
}

static esp_err_t admin_network_get(httpd_req_t *r)
{
    if (!admin_auth_ok(r)) return admin_unauthorized(r);
    return admin_network_get_unauthed(r);
}

void prisma_admin_register(httpd_handle_t server)
{
    if (!server) return;

    admin_storage_snapshot_init();

    httpd_uri_t page = {.uri="/admin",.method=HTTP_GET,.handler=admin_page_get};
    httpd_uri_t css = {.uri="/admin.css",.method=HTTP_GET,.handler=admin_css_get};
    httpd_uri_t js = {.uri="/admin.js",.method=HTTP_GET,.handler=admin_js_get};
    httpd_uri_t overview = {.uri="/api/admin/overview",.method=HTTP_GET,.handler=admin_overview_get};
    httpd_uri_t status = {.uri="/api/admin/status",.method=HTTP_GET,.handler=admin_status_get};
    httpd_uri_t sensors = {.uri="/api/admin/sensors",.method=HTTP_GET,.handler=admin_sensors_get};
    httpd_uri_t storage = {.uri="/api/admin/storage",.method=HTTP_GET,.handler=admin_storage_get};
    httpd_uri_t network = {.uri="/api/admin/network",.method=HTTP_GET,.handler=admin_network_get};
    httpd_register_uri_handler(server, &page);
    httpd_register_uri_handler(server, &css);
    httpd_register_uri_handler(server, &js);
    httpd_register_uri_handler(server, &overview);
    httpd_register_uri_handler(server, &status);
    httpd_register_uri_handler(server, &sensors);
    httpd_register_uri_handler(server, &storage);
    httpd_register_uri_handler(server, &network);
}
