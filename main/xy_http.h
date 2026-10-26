#pragma once
#include "esp_http_server.h"

extern const char prisma_xy_web_injection[];
void prisma_xy_http_register(httpd_handle_t server);
