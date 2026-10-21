#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRISMA_BT_MAX_DEVICES 20
#define PRISMA_BT_NAME_LEN 32
#define PRISMA_BT_ADDR_LEN 18

typedef struct {
    char name[PRISMA_BT_NAME_LEN];
    char address[PRISMA_BT_ADDR_LEN];
    uint8_t addr_type;
    int rssi;
    bool connectable;
} prisma_bt_device_t;

typedef struct {
    bool initialized;
    bool synced;
    bool scanning;
    bool advertising;
    bool advertise_requested;
    bool connecting;
    bool connected;
    bool encrypted;
    bool authenticated;
    bool bonded;
    char device_name[PRISMA_BT_NAME_LEN];
    char local_address[PRISMA_BT_ADDR_LEN];
    char peer_address[PRISMA_BT_ADDR_LEN];
    char peer_name[PRISMA_BT_NAME_LEN];
    uint16_t conn_handle;
    uint16_t mtu;
    int peer_rssi;
    int scan_count;
    int last_error_code;
    char last_error[96];
} prisma_bt_status_t;

esp_err_t prisma_bluetooth_init(void);
void prisma_bluetooth_get_status(prisma_bt_status_t *out);
int prisma_bluetooth_get_devices(prisma_bt_device_t *out, int max_devices);
esp_err_t prisma_bluetooth_scan_start(int seconds);
esp_err_t prisma_bluetooth_scan_stop(void);
esp_err_t prisma_bluetooth_connect(const char *address, uint8_t addr_type);
esp_err_t prisma_bluetooth_disconnect(void);
esp_err_t prisma_bluetooth_pair(void);
esp_err_t prisma_bluetooth_set_advertising(bool enabled);

#ifdef __cplusplus
}
#endif
