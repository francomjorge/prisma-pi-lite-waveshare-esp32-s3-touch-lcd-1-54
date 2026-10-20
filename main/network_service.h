#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define PRISMA_NETWORK_MAX_SCAN 20

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    uint8_t authmode;
} prisma_network_ap_t;

typedef struct {
    bool ap_running;
    char ap_ssid[33];
    char ap_ip[16];
    char ap_mac[18];
    uint8_t ap_channel;
    uint8_t ap_clients;
    bool ap_secured;

    bool sta_configured;
    bool sta_connected;
    bool sta_got_ip;
    bool sta_auto_connect;
    char sta_ssid[33];
    char sta_ip[16];
    char sta_gateway[16];
    char sta_netmask[16];
    char sta_mac[18];
    int8_t sta_rssi;
    uint8_t sta_channel;
    uint16_t last_disconnect_reason;
} prisma_network_status_t;

esp_err_t prisma_network_init(void);
void prisma_network_get_status(prisma_network_status_t *out);
int prisma_network_scan(prisma_network_ap_t *out, int max_items);

esp_err_t prisma_network_connect(const char *ssid, const char *password);
esp_err_t prisma_network_disconnect(void);
esp_err_t prisma_network_reconnect(void);
esp_err_t prisma_network_forget(void);
esp_err_t prisma_network_set_ap(const char *ssid, const char *password);
