#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PRISMA_NETWORK_OFFLINE = 0,
    PRISMA_NETWORK_STA_CONNECTING,
    PRISMA_NETWORK_STA_CONNECTED,
    PRISMA_NETWORK_SETUP_AP,
} prisma_network_mode_t;

typedef struct {
    prisma_network_mode_t mode;
    bool connected;
    bool setup_ap_active;
    char ssid[33];
    char ip[16];
    int8_t rssi;
    uint8_t retry_count;
} prisma_network_status_t;

/** Initialize Wi-Fi, fallback setup AP and the minimal M4.1 provisioning server. */
bool prisma_network_init(void);

/** Read a snapshot of current network state. */
prisma_network_status_t prisma_network_get_status(void);

/** Persist credentials and immediately try to connect. */
bool prisma_network_configure_wifi(const char *ssid, const char *password);

/** Forget stored Wi-Fi credentials and return to setup AP mode. */
bool prisma_network_clear_wifi(void);

#ifdef __cplusplus
}
#endif
