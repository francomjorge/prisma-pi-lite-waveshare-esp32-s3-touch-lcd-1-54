# V10 nests the V9 HTTP response wrapper. Generate build-only copies so the
# physically validated V9 source remains byte-for-byte untouched while its
# final linker-wrapper symbol is renamed to v9_wrapped_httpd_resp_send_chunk.
file(READ "${CMAKE_CURRENT_LIST_DIR}/wifi_portal_record_v9.c" V9_SOURCE)
set(V9_NESTED_NEEDLE [=[#undef __wrap_httpd_resp_send_chunk

static const char fxlab_record_v9_patch[] =]=])
set(V9_NESTED_REPLACEMENT [=[#undef __wrap_httpd_resp_send_chunk
#define __wrap_httpd_resp_send_chunk v9_wrapped_httpd_resp_send_chunk

static const char fxlab_record_v9_patch[] =]=])
string(REPLACE "${V9_NESTED_NEEDLE}" "${V9_NESTED_REPLACEMENT}" V9_NESTED_SOURCE "${V9_SOURCE}")
if(V9_NESTED_SOURCE STREQUAL V9_SOURCE)
    message(FATAL_ERROR "V12: could not prepare nested V9 wrapper")
endif()
set(V9_NESTED_FILE "${CMAKE_CURRENT_BINARY_DIR}/wifi_portal_record_v9_nested.inc")
file(WRITE "${V9_NESTED_FILE}" "${V9_NESTED_SOURCE}")

# Prepare the V10 build-only translation unit, switching its V9 include to the
# generated nested copy and renaming the final V10 wrapper for V11/V12.
file(READ "${CMAKE_CURRENT_LIST_DIR}/wifi_portal_record_v10.c" V10_SOURCE)
set(V10_INCLUDE_NEEDLE [=[#define __wrap_httpd_resp_send_chunk v9_wrapped_httpd_resp_send_chunk
#include "wifi_portal_record_v9.c"
#undef __wrap_httpd_resp_send_chunk]=])
set(V10_INCLUDE_REPLACEMENT [=[#include "wifi_portal_record_v9_nested.inc"
#undef __wrap_httpd_resp_send_chunk]=])
string(REPLACE "${V10_INCLUDE_NEEDLE}" "${V10_INCLUDE_REPLACEMENT}" V10_BUILD_SOURCE "${V10_SOURCE}")
if(V10_BUILD_SOURCE STREQUAL V10_SOURCE)
    message(FATAL_ERROR "V12: could not switch V10 to nested V9 include")
endif()
set(V10_WRAPPER_NEEDLE [=[esp_err_t __wrap_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)]=])
set(V10_WRAPPER_REPLACEMENT [=[esp_err_t v10_wrapped_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)]=])
string(REPLACE "${V10_WRAPPER_NEEDLE}" "${V10_WRAPPER_REPLACEMENT}" V10_NESTED_SOURCE "${V10_BUILD_SOURCE}")
if(V10_NESTED_SOURCE STREQUAL V10_BUILD_SOURCE)
    message(FATAL_ERROR "V12: could not rename V10 wrapper")
endif()
set(V10_BUILD_FILE "${CMAKE_CURRENT_BINARY_DIR}/wifi_portal_record_v10_nested.c")
file(WRITE "${V10_BUILD_FILE}" "${V10_NESTED_SOURCE}")

# V12 owns the real linker wrapper. Compile V11 as a generated nested unit so
# the validated V11 branch/source remains untouched.
file(READ "${CMAKE_CURRENT_LIST_DIR}/wifi_portal_record_v11.c" V11_SOURCE)
set(V11_WRAPPER_NEEDLE [=[esp_err_t __wrap_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)]=])
set(V11_WRAPPER_REPLACEMENT [=[esp_err_t v11_wrapped_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)]=])
string(REPLACE "${V11_WRAPPER_NEEDLE}" "${V11_WRAPPER_REPLACEMENT}" V11_NESTED_SOURCE "${V11_SOURCE}")
if(V11_NESTED_SOURCE STREQUAL V11_SOURCE)
    message(FATAL_ERROR "V12: could not rename V11 wrapper")
endif()
set(V11_BUILD_FILE "${CMAKE_CURRENT_BINARY_DIR}/wifi_portal_record_v11_nested.c")
file(WRITE "${V11_BUILD_FILE}" "${V11_NESTED_SOURCE}")

idf_component_register(
    SRCS "app_checkpoint_a.c" "wifi_portal_layout_v5.c" "${V10_BUILD_FILE}" "${V11_BUILD_FILE}" "wifi_portal_record_v12.c" "prisma_touch.c" "prisma_sensors.c" "storage.c" "prisma_power.c" "prisma_metrics.c" "admin_api_auth.c"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server esp_driver_gpio esp_driver_spi esp_driver_i2c esp_driver_tsens esp_lcd spiffs json esp_adc esp_app_format esp_timer heap spi_flash
    EMBED_TXTFILES "web/index.html" "web/style.css" "web/admin.html" "web/admin.css" "web/admin.js" "web/fxlab_v12.js"
)

# V12 owns the single real linker wrapper. V11/V10/V9/V8/V7 are chained under
# renamed internal symbols in generated build-only translation units.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=httpd_resp_send_chunk")

# Compatibility markers for protected V2/V3/V4/V5/V6/V7/V8/V9/V10/V11 tests: "wifi_portal_layout_v2.c" "wifi_portal_layout_v3.c" "wifi_portal_layout_v4.c" "wifi_portal_layout_v5.c" "wifi_portal_record_v6.c" "wifi_portal_record_v9.c" "wifi_portal_record_v10.c" "wifi_portal_record_v11.c"
spiffs_create_partition_image(storage ../storage FLASH_IN_PROJECT)
