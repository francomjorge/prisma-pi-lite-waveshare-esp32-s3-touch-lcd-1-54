#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PRISMA_NETWORK_OFFLINE = 0,
    PRISMA_NETWORK_LOCAL_AP,
} prisma_network_mode_t;

typedef struct {
    prisma_network_mode_t mode;
    bool local_ready;
    bool ap_active;
    char ssid[33];
    char ip[16];
    uint8_t clients;
} prisma_network_status_t;

/**
 * Start the autonomous local Wi-Fi network used by the appliance.
 * M4.1 does not require a router or Internet connection.
 */
bool prisma_network_init(void);

/** Read a snapshot of the current local-network state. */
prisma_network_status_t prisma_network_get_status(void);

#ifdef __cplusplus
}
#endif
