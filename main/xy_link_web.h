#pragma once
#include "esp_http_server.h"

extern const char prisma_xy_link_web_injection[];
void prisma_xy_link_http_register(httpd_handle_t server);
