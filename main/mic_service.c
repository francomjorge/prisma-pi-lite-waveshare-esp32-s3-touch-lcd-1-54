#include "mic_service.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "audio_service.h"
#include "driver/i2s_tdm.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "storage.h"

#define MIC_RATE 16000
#define MIC_BITS 16
#define MIC_MCLK GPIO_NUM_8
#define MIC_BCLK GPIO_NUM_9
#define MIC_WS GPIO_NUM_10
#define MIC_DIN GPIO_NUM_11
#define MIC_GAIN_DB 30
#define MIC_TASK_STACK 6144
#define MIC_CHUNK_SAMPLES 256
#define RECORD_NAME "mic_record.wav"
#define FILES_NS "media_cfg"
#define FILES_SAMPLE "sample"
#define MAX_RECORD_SECONDS 10

static const char *TAG = "PRISMA_MIC";
static SemaphoreHandle_t s_lock;
static i2s_chan_handle_t s_rx;
static esp_codec_dev_handle_t s_input;
static prisma_mic_status_t s_status;
static TaskHandle_t s_task;
static FILE *s_record_file;
static uint32_t s_record_bytes;
static uint32_t s_record_target_bytes;
static volatile bool s_record_stop;

static void lock(void)
{
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void)
{
    if (s_lock) xSemaphoreGive(s_lock);
}

static void set_mic_error(const char *msg)
{
    lock();
    strlcpy(s_status.last_error, msg ? msg : "", sizeof(s_status.last_error));
    unlock();
}

static void put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

static void make_wav_header(uint8_t h[44], uint32_t data_bytes)
{
    memset(h, 0, 44);
    memcpy(h + 0, "RIFF", 4);
    put_le32(h + 4, 36U + data_bytes);
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    put_le32(h + 16, 16);
    put_le16(h + 20, 1);
    put_le16(h + 22, 1);
    put_le32(h + 24, MIC_RATE);
    put_le32(h + 28, MIC_RATE * 2U);
    put_le16(h + 32, 2);
    put_le16(h + 34, MIC_BITS);
    memcpy(h + 36, "data", 4);
    put_le32(h + 40, data_bytes);
}

static esp_err_t save_selected(const char *name)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(FILES_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, FILES_SAMPLE, name);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void finish_recording(bool keep_file, const char *error)
{
    FILE *f = NULL;
    uint32_t bytes = 0;

    lock();
    f = s_record_file;
    bytes = s_record_bytes;
    s_record_file = NULL;
    s_status.recording = false;
    s_status.record_ms = (int)(((uint64_t)bytes * 1000ULL) / (MIC_RATE * 2ULL));
    if (error) strlcpy(s_status.last_error, error, sizeof(s_status.last_error));
    unlock();

    if (!f) return;

    bool ok = keep_file && bytes > 0;
    if (ok) {
        uint8_t header[44];
        make_wav_header(header, bytes);
        if (fflush(f) != 0 || fseek(f, 0, SEEK_SET) != 0 || fwrite(header, 1, sizeof(header), f) != sizeof(header)) {
            ok = false;
        }
    }
    fclose(f);

    if (!ok) {
        remove("/storage/" RECORD_NAME);
        if (!error) set_mic_error("Recording finalize failed");
        return;
    }

    if (save_selected(RECORD_NAME) != ESP_OK) {
        set_mic_error("Recorded WAV saved but sample selection failed");
    } else {
        lock();
        strlcpy(s_status.record_name, RECORD_NAME, sizeof(s_status.record_name));
        if (!error) s_status.last_error[0] = 0;
        unlock();
    }

    ESP_LOGI(TAG, "Recording saved: %s bytes=%u", RECORD_NAME, (unsigned)bytes);
}

static void mic_task(void *arg)
{
    (void)arg;
    int16_t samples[MIC_CHUNK_SAMPLES];

    for (;;) {
        if (!s_input) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int rc = esp_codec_dev_read(s_input, samples, sizeof(samples));
        if (rc != ESP_CODEC_DEV_OK) {
            set_mic_error("Microphone read failed");
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }

        uint32_t peak = 0;
        for (size_t i = 0; i < MIC_CHUNK_SAMPLES; i++) {
            int32_t v = samples[i];
            uint32_t a = (uint32_t)(v < 0 ? -v : v);
            if (a > peak) peak = a;
        }
        if (peak > 32767U) peak = 32767U;
        int raw = (int)((peak * 100U) / 32767U);

        FILE *record_file = NULL;
        uint32_t record_bytes = 0;
        uint32_t target_bytes = 0;
        bool recording = false;
        bool stop_now = false;

        lock();
        int previous = s_status.level;
        s_status.peak = raw;
        s_status.level = raw >= previous ? raw : ((previous * 3 + raw) / 4);
        recording = s_status.recording;
        record_file = s_record_file;
        record_bytes = s_record_bytes;
        target_bytes = s_record_target_bytes;
        stop_now = s_record_stop;
        unlock();

        if (!recording || !record_file) continue;

        size_t chunk = sizeof(samples);
        if (record_bytes + chunk > target_bytes) chunk = target_bytes - record_bytes;

        if (chunk && fwrite(samples, 1, chunk, record_file) != chunk) {
            finish_recording(false, "Storage write failed");
            continue;
        }

        record_bytes += (uint32_t)chunk;
        lock();
        s_record_bytes = record_bytes;
        s_status.record_ms = (int)(((uint64_t)record_bytes * 1000ULL) / (MIC_RATE * 2ULL));
        stop_now = s_record_stop;
        unlock();

        if (stop_now || record_bytes >= target_bytes) {
            finish_recording(true, NULL);
        }
    }
}

esp_err_t prisma_mic_init(i2c_master_bus_handle_t i2c_bus)
{
    memset(&s_status, 0, sizeof(s_status));
    s_status.gain_db = MIC_GAIN_DB;
    strlcpy(s_status.record_name, RECORD_NAME, sizeof(s_status.record_name));

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    if (!i2c_bus) {
        set_mic_error("Shared I2C bus unavailable");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t probe = i2c_master_probe(i2c_bus, (uint16_t)(ES7210_CODEC_DEFAULT_ADDR >> 1), 100);
    if (probe != ESP_OK) {
        set_mic_error("ES7210 microphone ADC not detected");
        return ESP_ERR_NOT_FOUND;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_rx);
    if (err != ESP_OK) {
        set_mic_error("I2S RX channel unavailable");
        return err;
    }

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = MIC_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3,
            .ws_width = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
            .skip_mask = false,
            .total_slot = I2S_TDM_AUTO_SLOT_NUM,
        },
        .gpio_cfg = {
            .mclk = MIC_MCLK,
            .bclk = MIC_BCLK,
            .ws = MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = MIC_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_tdm_mode(s_rx, &tdm_cfg);
    if (err != ESP_OK) {
        set_mic_error("I2S microphone TDM init failed");
        return err;
    }
    err = i2s_channel_enable(s_rx);
    if (err != ESP_OK) {
        set_mic_error("I2S microphone enable failed");
        return err;
    }

    audio_codec_i2s_cfg_t data_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = s_rx,
        .tx_handle = NULL,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&data_cfg);
    if (!data_if) {
        set_mic_error("Microphone I2S interface failed");
        return ESP_FAIL;
    }

    audio_codec_i2c_cfg_t i2c_cfg = {
        .addr = ES7210_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (!ctrl_if) {
        set_mic_error("ES7210 control interface failed");
        return ESP_FAIL;
    }

    es7210_codec_cfg_t codec_cfg = {0};
    codec_cfg.ctrl_if = ctrl_if;
    codec_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    const audio_codec_if_t *codec_if = es7210_codec_new(&codec_cfg);
    if (!codec_if) {
        set_mic_error("ES7210 codec creation failed");
        return ESP_FAIL;
    }

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    s_input = esp_codec_dev_new(&dev_cfg);
    if (!s_input) {
        set_mic_error("Microphone device creation failed");
        return ESP_FAIL;
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = MIC_BITS,
        .channel = 4,
        .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        .sample_rate = MIC_RATE,
        .mclk_multiple = 0,
    };
    if (esp_codec_dev_open(s_input, &fs) != ESP_CODEC_DEV_OK) {
        set_mic_error("ES7210 open failed");
        return ESP_FAIL;
    }
    if (esp_codec_dev_set_in_channel_gain(s_input, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), MIC_GAIN_DB) != ESP_CODEC_DEV_OK) {
        set_mic_error("Microphone gain init failed");
        return ESP_FAIL;
    }

    lock();
    s_status.initialized = true;
    s_status.last_error[0] = 0;
    unlock();

    if (xTaskCreate(mic_task, "mic_level", MIC_TASK_STACK, NULL, 4, &s_task) != pdPASS) {
        set_mic_error("Microphone monitor task failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "MIC READY ES7210 rate=%d bits=%d gain=%d DIN=%d",
             MIC_RATE, MIC_BITS, MIC_GAIN_DB, (int)MIC_DIN);
    return ESP_OK;
}

void prisma_mic_get_status(prisma_mic_status_t *out)
{
    if (!out) return;
    lock();
    *out = s_status;
    unlock();
}

esp_err_t prisma_mic_record_start(int seconds)
{
    if (!s_status.initialized || !s_input) return ESP_ERR_INVALID_STATE;
    if (seconds < 1 || seconds > MAX_RECORD_SECONDS) return ESP_ERR_INVALID_ARG;

    prisma_audio_status_t audio;
    prisma_audio_get_status(&audio);
    if (audio.playing) return ESP_ERR_INVALID_STATE;

    lock();
    bool busy = s_status.recording;
    unlock();
    if (busy) return ESP_ERR_INVALID_STATE;

    if (!prisma_storage_ready()) return ESP_ERR_INVALID_STATE;

    struct stat st;
    bool replacing = stat("/storage/" RECORD_NAME, &st) == 0 && S_ISREG(st.st_mode);
    if (!replacing && prisma_storage_file_count() >= prisma_storage_max_files()) {
        set_mic_error("FILES+ is full (4 files)");
        return ESP_ERR_NO_MEM;
    }

    size_t total = 0, used = 0;
    uint32_t target = (uint32_t)seconds * MIC_RATE * 2U;
    if (!prisma_storage_info(&total, &used) || total <= used || (total - used) < (size_t)target + 8192U) {
        set_mic_error("Not enough storage for recording");
        return ESP_ERR_NO_MEM;
    }

    FILE *f = fopen("/storage/" RECORD_NAME, "wb");
    if (!f) {
        set_mic_error("Cannot create recording file");
        return ESP_FAIL;
    }

    uint8_t header[44];
    make_wav_header(header, 0);
    if (fwrite(header, 1, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        remove("/storage/" RECORD_NAME);
        set_mic_error("Cannot write WAV header");
        return ESP_FAIL;
    }

    lock();
    s_record_file = f;
    s_record_bytes = 0;
    s_record_target_bytes = target;
    s_record_stop = false;
    s_status.recording = true;
    s_status.record_ms = 0;
    s_status.record_target_ms = seconds * 1000;
    s_status.last_error[0] = 0;
    unlock();

    ESP_LOGI(TAG, "Recording started: %d sec -> %s", seconds, RECORD_NAME);
    return ESP_OK;
}

void prisma_mic_record_stop(void)
{
    s_record_stop = true;
}
