#include "bluetooth_service.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define DEVICE_NAME "PRISMA-PI LITE"
#define DEFAULT_SCAN_SECONDS 6
#define MIN_SCAN_SECONDS 2
#define MAX_SCAN_SECONDS 15

static const char *TAG = "PRISMA_BLE";

void ble_store_config_init(void);

typedef struct {
    prisma_bt_device_t pub;
    ble_addr_t addr;
} bt_entry_t;

static SemaphoreHandle_t s_lock;
static prisma_bt_status_t s_status;
static bt_entry_t s_devices[PRISMA_BT_MAX_DEVICES];
static int s_device_count;
static uint8_t s_own_addr_type;
static bool s_host_started;

static void lock_state(void)
{
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock_state(void)
{
    if (s_lock) xSemaphoreGive(s_lock);
}

static void format_addr(const uint8_t addr[6], char out[PRISMA_BT_ADDR_LEN])
{
    snprintf(out, PRISMA_BT_ADDR_LEN, "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

static void set_errorf(int code, const char *fmt, ...)
{
    lock_state();
    s_status.last_error_code = code;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_status.last_error, sizeof(s_status.last_error), fmt, ap);
    va_end(ap);
    unlock_state();
}

static void clear_error(void)
{
    lock_state();
    s_status.last_error_code = 0;
    strlcpy(s_status.last_error, "NONE", sizeof(s_status.last_error));
    unlock_state();
}

static void update_connection_from_desc(const struct ble_gap_conn_desc *desc)
{
    if (!desc) return;
    char addr[PRISMA_BT_ADDR_LEN];
    format_addr(desc->peer_id_addr.val, addr);

    lock_state();
    s_status.connected = true;
    s_status.connecting = false;
    s_status.scanning = false;
    s_status.advertising = false;
    s_status.conn_handle = desc->conn_handle;
    strlcpy(s_status.peer_address, addr, sizeof(s_status.peer_address));
    s_status.encrypted = desc->sec_state.encrypted;
    s_status.authenticated = desc->sec_state.authenticated;
    s_status.bonded = desc->sec_state.bonded;
    if (!s_status.peer_name[0]) {
        for (int i = 0; i < s_device_count; i++) {
            if (s_devices[i].addr.type == desc->peer_id_addr.type &&
                memcmp(s_devices[i].addr.val, desc->peer_id_addr.val, 6) == 0) {
                strlcpy(s_status.peer_name, s_devices[i].pub.name, sizeof(s_status.peer_name));
                break;
            }
        }
    }
    unlock_state();
}

static bool should_advertise(void)
{
    bool yes;
    lock_state();
    yes = s_status.synced && s_status.advertise_requested && !s_status.scanning &&
          !s_status.connecting && !s_status.connected;
    unlock_state();
    return yes;
}

static int gap_event(struct ble_gap_event *event, void *arg);

static esp_err_t start_advertising_internal(void)
{
    if (!should_advertise()) return ESP_OK;
    if (ble_gap_adv_active()) {
        lock_state();
        s_status.advertising = true;
        unlock_state();
        return ESP_OK;
    }

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        set_errorf(rc, "Advertising data failed (%d)", rc);
        return ESP_FAIL;
    }

    struct ble_gap_adv_params params;
    memset(&params, 0, sizeof(params));
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0) {
        set_errorf(rc, "Advertising start failed (%d)", rc);
        return ESP_FAIL;
    }

    lock_state();
    s_status.advertising = true;
    unlock_state();
    return ESP_OK;
}

static void scan_report(const struct ble_gap_disc_desc *disc)
{
    if (!disc) return;

    char address[PRISMA_BT_ADDR_LEN];
    format_addr(disc->addr.val, address);
    bool connectable = disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
                       disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND;

    char name[PRISMA_BT_NAME_LEN] = {0};
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) == 0 &&
        fields.name && fields.name_len) {
        size_t n = fields.name_len;
        if (n >= sizeof(name)) n = sizeof(name) - 1;
        memcpy(name, fields.name, n);
        name[n] = 0;
    }

    lock_state();
    int idx = -1;
    for (int i = 0; i < s_device_count; i++) {
        if (s_devices[i].addr.type == disc->addr.type &&
            memcmp(s_devices[i].addr.val, disc->addr.val, 6) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0 && s_device_count < PRISMA_BT_MAX_DEVICES) {
        idx = s_device_count++;
        memset(&s_devices[idx], 0, sizeof(s_devices[idx]));
        s_devices[idx].addr = disc->addr;
        strlcpy(s_devices[idx].pub.address, address, sizeof(s_devices[idx].pub.address));
        s_devices[idx].pub.addr_type = disc->addr.type;
        strlcpy(s_devices[idx].pub.name, "UNNAMED", sizeof(s_devices[idx].pub.name));
    }
    if (idx >= 0) {
        s_devices[idx].pub.rssi = disc->rssi;
        if (connectable) s_devices[idx].pub.connectable = true;
        if (name[0]) strlcpy(s_devices[idx].pub.name, name, sizeof(s_devices[idx].pub.name));
    }
    s_status.scan_count = s_device_count;
    unlock_state();
}

static void on_reset(int reason)
{
    set_errorf(reason, "NimBLE reset (%d)", reason);
    lock_state();
    s_status.synced = false;
    s_status.scanning = false;
    s_status.advertising = false;
    s_status.connecting = false;
    s_status.connected = false;
    unlock_state();
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        set_errorf(rc, "BLE identity address unavailable (%d)", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        set_errorf(rc, "BLE address type failed (%d)", rc);
        return;
    }

    uint8_t addr[6] = {0};
    rc = ble_hs_id_copy_addr(s_own_addr_type, addr, NULL);
    if (rc != 0) {
        set_errorf(rc, "BLE address copy failed (%d)", rc);
        return;
    }

    char formatted[PRISMA_BT_ADDR_LEN];
    format_addr(addr, formatted);
    lock_state();
    s_status.synced = true;
    strlcpy(s_status.local_address, formatted, sizeof(s_status.local_address));
    unlock_state();
    clear_error();
    ESP_LOGI(TAG, "BLE synced as %s / %s", DEVICE_NAME, formatted);
    start_advertising_internal();
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            scan_report(&event->disc);
            return 0;

        case BLE_GAP_EVENT_DISC_COMPLETE:
            lock_state();
            s_status.scanning = false;
            unlock_state();
            start_advertising_internal();
            return 0;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                if (rc == 0) {
                    update_connection_from_desc(&desc);
                    clear_error();
                    ESP_LOGI(TAG, "BLE connected handle=%u peer=%s", desc.conn_handle, s_status.peer_address);
                } else {
                    set_errorf(rc, "Connected but descriptor failed (%d)", rc);
                }
            } else {
                lock_state();
                s_status.connecting = false;
                s_status.connected = false;
                unlock_state();
                set_errorf(event->connect.status, "Connection failed (%d)", event->connect.status);
                start_advertising_internal();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            lock_state();
            s_status.connected = false;
            s_status.connecting = false;
            s_status.encrypted = false;
            s_status.authenticated = false;
            s_status.bonded = false;
            s_status.conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_status.mtu = 23;
            s_status.peer_rssi = 0;
            unlock_state();
            set_errorf(event->disconnect.reason, "Disconnected (reason %d)", event->disconnect.reason);
            start_advertising_internal();
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            lock_state();
            s_status.advertising = false;
            unlock_state();
            start_advertising_internal();
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE:
            if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) == 0) update_connection_from_desc(&desc);
            return 0;

        case BLE_GAP_EVENT_ENC_CHANGE:
            if (ble_gap_conn_find(event->enc_change.conn_handle, &desc) == 0) update_connection_from_desc(&desc);
            if (event->enc_change.status != 0) {
                set_errorf(event->enc_change.status, "Pair/security failed (%d)", event->enc_change.status);
            } else {
                clear_error();
            }
            return 0;

        case BLE_GAP_EVENT_MTU:
            lock_state();
            s_status.mtu = event->mtu.value;
            unlock_state();
            return 0;

        case BLE_GAP_EVENT_REPEAT_PAIRING:
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            if (rc == 0) ble_store_util_delete_peer(&desc.peer_id_addr);
            return BLE_GAP_REPEAT_PAIRING_RETRY;

        default:
            return 0;
    }
}

static void host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t prisma_bluetooth_init(void)
{
    if (s_host_started) return ESP_OK;

    if (!s_lock) {
        s_lock = xSemaphoreCreateMutex();
        if (!s_lock) return ESP_ERR_NO_MEM;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_status.mtu = 23;
    s_status.peer_rssi = 0;
    s_status.advertise_requested = true;
    strlcpy(s_status.device_name, DEVICE_NAME, sizeof(s_status.device_name));
    strlcpy(s_status.last_error, "NONE", sizeof(s_status.last_error));

    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Never erase user NVS automatically in this appliance. */
        set_errorf(nvs, "NVS requires recovery; BLE not started");
        return nvs;
    }
    if (nvs != ESP_OK) {
        set_errorf(nvs, "NVS init failed (%d)", nvs);
        return nvs;
    }

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        set_errorf(ret, "NimBLE init failed (%d)", ret);
        return ret;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();
#if CONFIG_BT_NIMBLE_GAP_SERVICE
    int rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        set_errorf(rc, "BLE device name failed (%d)", rc);
        nimble_port_deinit();
        return ESP_FAIL;
    }
#endif

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();

    s_host_started = true;
    lock_state();
    s_status.initialized = true;
    unlock_state();
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

void prisma_bluetooth_get_status(prisma_bt_status_t *out)
{
    if (!out) return;
    lock_state();
    *out = s_status;
    unlock_state();

    if (out->connected && out->conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        int8_t rssi = 0;
        if (ble_gap_conn_rssi(out->conn_handle, &rssi) == 0) {
            out->peer_rssi = rssi;
            lock_state();
            s_status.peer_rssi = rssi;
            unlock_state();
        }
    }
}

int prisma_bluetooth_get_devices(prisma_bt_device_t *out, int max_devices)
{
    if (!out || max_devices <= 0) return 0;
    lock_state();
    int n = s_device_count < max_devices ? s_device_count : max_devices;
    for (int i = 0; i < n; i++) out[i] = s_devices[i].pub;
    unlock_state();
    return n;
}

esp_err_t prisma_bluetooth_scan_start(int seconds)
{
    if (seconds == 0) seconds = DEFAULT_SCAN_SECONDS;
    if (seconds < MIN_SCAN_SECONDS || seconds > MAX_SCAN_SECONDS) return ESP_ERR_INVALID_ARG;

    lock_state();
    if (!s_status.synced || s_status.connecting) {
        unlock_state();
        return ESP_ERR_INVALID_STATE;
    }
    memset(s_devices, 0, sizeof(s_devices));
    s_device_count = 0;
    s_status.scan_count = 0;
    s_status.scanning = true;
    unlock_state();

    if (ble_gap_adv_active()) ble_gap_adv_stop();
    if (ble_gap_disc_active()) ble_gap_disc_cancel();

    struct ble_gap_disc_params params;
    memset(&params, 0, sizeof(params));
    params.filter_duplicates = 0;
    params.passive = 0;
    params.itvl = 0;
    params.window = 0;
    params.filter_policy = 0;
    params.limited = 0;

    int rc = ble_gap_disc(s_own_addr_type, seconds * 1000, &params, gap_event, NULL);
    if (rc != 0) {
        lock_state();
        s_status.scanning = false;
        unlock_state();
        set_errorf(rc, "Scan start failed (%d)", rc);
        start_advertising_internal();
        return ESP_FAIL;
    }
    clear_error();
    return ESP_OK;
}

esp_err_t prisma_bluetooth_scan_stop(void)
{
    if (!ble_gap_disc_active()) {
        lock_state();
        s_status.scanning = false;
        unlock_state();
        start_advertising_internal();
        return ESP_OK;
    }
    int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        set_errorf(rc, "Scan stop failed (%d)", rc);
        return ESP_FAIL;
    }
    lock_state();
    s_status.scanning = false;
    unlock_state();
    start_advertising_internal();
    return ESP_OK;
}

esp_err_t prisma_bluetooth_connect(const char *address, uint8_t addr_type)
{
    if (!address || !*address) return ESP_ERR_INVALID_ARG;

    ble_addr_t target;
    char peer_name[PRISMA_BT_NAME_LEN] = {0};
    bool found = false;

    lock_state();
    if (!s_status.synced || s_status.connected || s_status.connecting) {
        unlock_state();
        return ESP_ERR_INVALID_STATE;
    }
    for (int i = 0; i < s_device_count; i++) {
        if (s_devices[i].pub.addr_type == addr_type && strcmp(s_devices[i].pub.address, address) == 0) {
            if (!s_devices[i].pub.connectable) {
                unlock_state();
                return ESP_ERR_NOT_SUPPORTED;
            }
            target = s_devices[i].addr;
            strlcpy(peer_name, s_devices[i].pub.name, sizeof(peer_name));
            found = true;
            break;
        }
    }
    if (!found) {
        unlock_state();
        return ESP_ERR_NOT_FOUND;
    }
    s_status.connecting = true;
    s_status.scanning = false;
    strlcpy(s_status.peer_address, address, sizeof(s_status.peer_address));
    strlcpy(s_status.peer_name, peer_name, sizeof(s_status.peer_name));
    unlock_state();

    if (ble_gap_disc_active()) ble_gap_disc_cancel();
    if (ble_gap_adv_active()) ble_gap_adv_stop();

    int rc = ble_gap_connect(s_own_addr_type, &target, 30000, NULL, gap_event, NULL);
    if (rc != 0) {
        lock_state();
        s_status.connecting = false;
        unlock_state();
        set_errorf(rc, "Connect request failed (%d)", rc);
        start_advertising_internal();
        return ESP_FAIL;
    }
    clear_error();
    return ESP_OK;
}

esp_err_t prisma_bluetooth_disconnect(void)
{
    prisma_bt_status_t st;
    prisma_bluetooth_get_status(&st);
    if (!st.connected || st.conn_handle == BLE_HS_CONN_HANDLE_NONE) return ESP_ERR_INVALID_STATE;
    int rc = ble_gap_terminate(st.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) {
        set_errorf(rc, "Disconnect failed (%d)", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t prisma_bluetooth_pair(void)
{
    prisma_bt_status_t st;
    prisma_bluetooth_get_status(&st);
    if (!st.connected || st.conn_handle == BLE_HS_CONN_HANDLE_NONE) return ESP_ERR_INVALID_STATE;
    int rc = ble_gap_security_initiate(st.conn_handle);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        set_errorf(rc, "Pairing start failed (%d)", rc);
        return ESP_FAIL;
    }
    clear_error();
    return ESP_OK;
}

esp_err_t prisma_bluetooth_set_advertising(bool enabled)
{
    lock_state();
    s_status.advertise_requested = enabled;
    unlock_state();

    if (!enabled) {
        if (ble_gap_adv_active()) {
            int rc = ble_gap_adv_stop();
            if (rc != 0 && rc != BLE_HS_EALREADY) {
                set_errorf(rc, "Advertising stop failed (%d)", rc);
                return ESP_FAIL;
            }
        }
        lock_state();
        s_status.advertising = false;
        unlock_state();
        return ESP_OK;
    }

    return start_advertising_internal();
}
