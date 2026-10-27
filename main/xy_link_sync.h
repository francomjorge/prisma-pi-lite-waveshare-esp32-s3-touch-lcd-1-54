#pragma once
#include "esp_http_server.h"

extern const char prisma_xy_link_sync_injection[];
void prisma_xy_link_sync_register(httpd_handle_t server);
