#pragma once

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

void prisma_playback_http_register(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
