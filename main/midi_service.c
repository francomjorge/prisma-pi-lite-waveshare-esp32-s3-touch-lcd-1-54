#include "midi_service.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "nvs.h"
#include "usb/usb_host.h"
#include "usb/usb_helpers.h"

#define MIDI_CFG_NS "midi_cfg"
#define MIDI_CFG_KEY_PLAY "play"
#define MIDI_CFG_KEY_FILE "file"
#define USB_TASK_PRIORITY 5
#define USB_TASK_STACK 4096
#define MIDI_TASK_PRIORITY 6
#define MIDI_TASK_STACK 4096

static const char *TAG = "MIDI_SERVICE";

static SemaphoreHandle_t s_lock;
static prisma_midi_status_t s_status;
static usb_host_client_handle_t s_client;
static usb_device_handle_t s_dev;
static volatile bool s_new_pending;
static volatile bool s_gone_pending;
static volatile uint8_t s_pending_addr;

static bool file_exists(const char *name)
{
    if (!name || !name[0]) return false;
    char path[128];
    if (snprintf(path, sizeof(path), "/storage/%s", name) >= (int)sizeof(path)) return false;
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void bump_revision_locked(void)
{
    s_status.revision++;
    if (s_status.revision == 0) s_status.revision = 1;
}

static void load_config(void)
{
    nvs_handle_t h;
    if (nvs_open(MIDI_CFG_NS, NVS_READONLY, &h) != ESP_OK) return;

    uint8_t play = 0;
    if (nvs_get_u8(h, MIDI_CFG_KEY_PLAY, &play) == ESP_OK) s_status.play_sound = play != 0;

    size_t len = sizeof(s_status.sound_file);
    if (nvs_get_str(h, MIDI_CFG_KEY_FILE, s_status.sound_file, &len) != ESP_OK) s_status.sound_file[0] = 0;
    s_status.selected_file_present = file_exists(s_status.sound_file);
    nvs_close(h);
}

static bool save_config(bool play_sound, const char *sound_file)
{
    nvs_handle_t h;
    if (nvs_open(MIDI_CFG_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_u8(h, MIDI_CFG_KEY_PLAY, play_sound ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_str(h, MIDI_CFG_KEY_FILE, sound_file ? sound_file : "");
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

static bool config_is_midi(const usb_config_desc_t *cfg)
{
    if (!cfg) return false;
    int offset = 0;
    const usb_standard_desc_t *desc = (const usb_standard_desc_t *)cfg;
    const uint16_t total = cfg->wTotalLength;

    while (desc) {
        if (desc->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            const usb_intf_desc_t *intf = (const usb_intf_desc_t *)desc;
            if (intf->bInterfaceClass == 0x01 && intf->bInterfaceSubClass == 0x03) return true;
        }
        desc = usb_parse_next_descriptor(desc, total, &offset);
    }
    return false;
}

static void status_set_device(bool present, bool midi, uint16_t vid, uint16_t pid)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.usb_present = present;
    s_status.midi_connected = midi;
    s_status.vid = present ? vid : 0;
    s_status.pid = present ? pid : 0;
    bump_revision_locked();
    xSemaphoreGive(s_lock);
}

static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    (void)arg;
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            if (s_dev == NULL) {
                s_pending_addr = event_msg->new_dev.address;
                s_new_pending = true;
            }
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            if (s_dev != NULL && event_msg->dev_gone.dev_hdl == s_dev) s_gone_pending = true;
            break;
        default:
            break;
    }
}

static void handle_new_device(void)
{
    s_new_pending = false;
    if (s_dev != NULL) return;

    usb_device_handle_t dev = NULL;
    esp_err_t err = usb_host_device_open(s_client, s_pending_addr, &dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open device failed: %s", esp_err_to_name(err));
        return;
    }

    const usb_device_desc_t *dev_desc = NULL;
    const usb_config_desc_t *cfg_desc = NULL;
    uint16_t vid = 0, pid = 0;
    bool midi = false;

    if (usb_host_get_device_descriptor(dev, &dev_desc) == ESP_OK && dev_desc) {
        vid = dev_desc->idVendor;
        pid = dev_desc->idProduct;
    }
    if (usb_host_get_active_config_descriptor(dev, &cfg_desc) == ESP_OK) midi = config_is_midi(cfg_desc);

    s_dev = dev;
    status_set_device(true, midi, vid, pid);
    ESP_LOGI(TAG, "USB device %04x:%04x %s", vid, pid, midi ? "MIDI CONNECTED" : "NOT MIDI");
}

static void handle_gone_device(void)
{
    s_gone_pending = false;
    usb_device_handle_t dev = s_dev;
    s_dev = NULL;
    if (dev != NULL) {
        esp_err_t err = usb_host_device_close(s_client, dev);
        if (err != ESP_OK) ESP_LOGW(TAG, "close device failed: %s", esp_err_to_name(err));
    }
    status_set_device(false, false, 0, 0);
    ESP_LOGI(TAG, "USB/MIDI device disconnected");
}

static void midi_client_task(void *arg)
{
    (void)arg;
    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = NULL,
        },
    };

    esp_err_t err = usb_host_client_register(&client_config, &s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "client register failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        usb_host_client_handle_events(s_client, pdMS_TO_TICKS(100));
        if (s_gone_pending) handle_gone_device();
        if (s_new_pending) handle_new_device();
    }
}

static void usb_lib_task(void *arg)
{
    (void)arg;
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
    };

    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "USB host install failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.host_ready = true;
    bump_revision_locked();
    xSemaphoreGive(s_lock);

    xTaskCreate(midi_client_task, "midi_client", MIDI_TASK_STACK, NULL, MIDI_TASK_PRIORITY, NULL);

    while (1) {
        uint32_t event_flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}

void prisma_midi_init(void)
{
    if (s_lock) return;
    memset(&s_status, 0, sizeof(s_status));
    s_status.revision = 1;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "mutex allocation failed");
        return;
    }

    load_config();
    xTaskCreate(usb_lib_task, "usb_host", USB_TASK_STACK, NULL, USB_TASK_PRIORITY, NULL);
}

void prisma_midi_get_status(prisma_midi_status_t *out)
{
    if (!out) return;
    if (!s_lock) {
        memset(out, 0, sizeof(*out));
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.selected_file_present = file_exists(s_status.sound_file);
    *out = s_status;
    xSemaphoreGive(s_lock);
}

bool prisma_midi_set_config(bool play_sound, const char *sound_file)
{
    if (!s_lock || !sound_file) return false;
    if (strlen(sound_file) >= sizeof(s_status.sound_file)) return false;
    if (sound_file[0] && !file_exists(sound_file)) return false;
    if (!save_config(play_sound, sound_file)) return false;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.play_sound = play_sound;
    strlcpy(s_status.sound_file, sound_file, sizeof(s_status.sound_file));
    s_status.selected_file_present = file_exists(sound_file);
    bump_revision_locked();
    xSemaphoreGive(s_lock);
    return true;
}

void prisma_midi_on_file_deleted(const char *name)
{
    if (!s_lock || !name || !name[0]) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool selected = strcmp(s_status.sound_file, name) == 0;
    xSemaphoreGive(s_lock);
    if (selected) prisma_midi_set_config(false, "");
}
