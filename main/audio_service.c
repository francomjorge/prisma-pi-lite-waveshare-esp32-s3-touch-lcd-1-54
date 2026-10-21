#include "audio_service.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#define AUDIO_RATE 16000
#define AUDIO_BITS 16
#define AUDIO_PA GPIO_NUM_7
#define AUDIO_MCLK GPIO_NUM_8
#define AUDIO_BCLK GPIO_NUM_9
#define AUDIO_WS GPIO_NUM_10
#define AUDIO_DOUT GPIO_NUM_12

/* esp_codec_dev exposes codec addresses in the 8-bit shifted form used by
 * audio_codec_new_i2c_ctrl(). ESP-IDF i2c_master_probe() expects the 7-bit
 * slave address instead, so probe with the shifted value divided by two. */
#define ES8311_PROBE_ADDR ((uint16_t)(ES8311_CODEC_DEFAULT_ADDR >> 1))
#define ES7210_PROBE_ADDR ((uint16_t)(ES7210_CODEC_DEFAULT_ADDR >> 1))

#define FILES_NS "media_cfg"
#define FILES_SAMPLE "sample"
#define AUDIO_NS "audio_cfg"
#define AUDIO_VOLUME "volume"
#define DEFAULT_VOLUME 70
#define PLAY_STACK 8192
#define WAV_RAW_BYTES 512
#define WAV_OUT_SAMPLES 512

static const char *TAG = "PRISMA_AUDIO";
static SemaphoreHandle_t s_lock;
static i2s_chan_handle_t s_tx;
static esp_codec_dev_handle_t s_output;
static prisma_audio_status_t s_status;
static volatile bool s_stop;
static TaskHandle_t s_play_task;

typedef struct {
    uint16_t format;
    uint16_t channels;
    uint32_t rate;
    uint16_t bits;
    uint32_t data_offset;
    uint32_t data_bytes;
} wav_info_t;

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void lock(void)
{
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void)
{
    if (s_lock) xSemaphoreGive(s_lock);
}

static void set_error(const char *msg)
{
    lock();
    strlcpy(s_status.last_error, msg ? msg : "", sizeof(s_status.last_error));
    unlock();
}

static void load_selected(char out[64])
{
    out[0] = 0;
    nvs_handle_t h;
    if (nvs_open(FILES_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t n = 64;
    if (nvs_get_str(h, FILES_SAMPLE, out, &n) != ESP_OK) out[0] = 0;
    nvs_close(h);
}

static int load_volume(void)
{
    int32_t v = DEFAULT_VOLUME;
    nvs_handle_t h;
    if (nvs_open(AUDIO_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, AUDIO_VOLUME, &v);
        nvs_close(h);
    }
    if (v < 0 || v > 100) v = DEFAULT_VOLUME;
    return (int)v;
}

static esp_err_t save_volume(int volume)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(AUDIO_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_i32(h, AUDIO_VOLUME, volume);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static bool parse_wav(FILE *f, wav_info_t *w)
{
    memset(w, 0, sizeof(*w));
    uint8_t hdr[12];
    rewind(f);
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) return false;
    if (memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) return false;

    bool have_fmt = false;
    bool have_data = false;
    while (!have_fmt || !have_data) {
        uint8_t ch[8];
        if (fread(ch, 1, sizeof(ch), f) != sizeof(ch)) break;
        uint32_t len = le32(ch + 4);
        long payload = ftell(f);

        if (!memcmp(ch, "fmt ", 4)) {
            uint8_t fmt[40] = {0};
            size_t take = len < sizeof(fmt) ? len : sizeof(fmt);
            if (fread(fmt, 1, take, f) != take || take < 16) return false;
            w->format = le16(fmt + 0);
            w->channels = le16(fmt + 2);
            w->rate = le32(fmt + 4);
            w->bits = le16(fmt + 14);
            have_fmt = true;
        } else if (!memcmp(ch, "data", 4)) {
            w->data_offset = (uint32_t)payload;
            w->data_bytes = len;
            have_data = true;
        }

        long next = payload + (long)len + (len & 1U);
        if (fseek(f, next, SEEK_SET) != 0) break;
    }

    return have_fmt && have_data && w->format == 1 &&
           (w->channels == 1 || w->channels == 2) &&
           (w->bits == 8 || w->bits == 16) &&
           w->rate >= 8000 && w->rate <= 48000;
}

static int16_t decode_sample(const uint8_t *p, int bits)
{
    if (bits == 8) return (int16_t)(((int)p[0] - 128) << 8);
    return (int16_t)le16(p);
}

static bool write_pcm(const int16_t *samples, size_t count)
{
    if (!s_output || !count) return false;
    return esp_codec_dev_write(s_output, (void *)samples,
                               (int)(count * sizeof(int16_t))) == ESP_CODEC_DEV_OK;
}

static bool play_wav_file(const char *name)
{
    char path[128];
    snprintf(path, sizeof(path), "/storage/%s", name);
    ESP_LOGI(TAG, "WAV open: %s", path);

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "WAV fopen failed: %s", path);
        set_error("Selected WAV not found");
        return false;
    }

    wav_info_t w;
    if (!parse_wav(f, &w)) {
        fclose(f);
        ESP_LOGE(TAG, "WAV parse failed: %s", path);
        set_error("WAV must be PCM, 8/16-bit, mono/stereo, 8-48 kHz");
        return false;
    }
    if (fseek(f, (long)w.data_offset, SEEK_SET) != 0) {
        fclose(f);
        set_error("Cannot seek WAV data");
        return false;
    }

    const int frame_bytes = (w.bits / 8) * w.channels;
    const uint32_t total_frames = frame_bytes ? w.data_bytes / frame_bytes : 0;
    const int duration_ms = w.rate
        ? (int)(((uint64_t)total_frames * 1000ULL) / w.rate)
        : 0;

    ESP_LOGI(TAG, "WAV ready: %u Hz, %u-bit, %u ch, %u bytes, %d ms",
             (unsigned)w.rate, (unsigned)w.bits, (unsigned)w.channels,
             (unsigned)w.data_bytes, duration_ms);

    lock();
    s_status.source_rate = (int)w.rate;
    s_status.source_channels = w.channels;
    s_status.source_bits = w.bits;
    s_status.duration_ms = duration_ms;
    s_status.position_ms = 0;
    unlock();

    /* Keep playback buffers deliberately small. A4.1.2 used about 5 KiB of
     * local WAV buffers inside a 6 KiB FreeRTOS task, leaving too little room
     * for stdio/decoder/codec call frames. That could make browser playback
     * work while the on-device PLAY SELECTED WAV task failed. */
    uint8_t raw[WAV_RAW_BYTES];
    int16_t out[WAV_OUT_SAMPLES];
    size_t out_n = 0;
    uint32_t phase = 0;
    uint32_t frames_done = 0;
    uint32_t bytes_left = w.data_bytes;
    bool ok = true;

    while (bytes_left && !s_stop) {
        size_t want = bytes_left < sizeof(raw) ? bytes_left : sizeof(raw);
        want -= want % frame_bytes;
        if (!want) break;

        size_t got = fread(raw, 1, want, f);
        if (!got) {
            if (bytes_left) {
                ESP_LOGE(TAG, "WAV read ended early with %u bytes remaining",
                         (unsigned)bytes_left);
                set_error("WAV read failed");
                ok = false;
            }
            break;
        }
        got -= got % frame_bytes;
        bytes_left -= (uint32_t)got;
        size_t frames = got / frame_bytes;

        for (size_t i = 0; i < frames && !s_stop; i++) {
            const uint8_t *p = raw + i * frame_bytes;
            int32_t sample = decode_sample(p, w.bits);
            if (w.channels == 2) {
                int16_t right = decode_sample(p + (w.bits / 8), w.bits);
                sample = (sample + right) / 2;
            }

            phase += AUDIO_RATE;
            while (phase >= w.rate) {
                phase -= w.rate;
                out[out_n++] = (int16_t)sample;
                if (out_n == (sizeof(out) / sizeof(out[0]))) {
                    if (!write_pcm(out, out_n)) {
                        ok = false;
                        break;
                    }
                    out_n = 0;
                }
            }
            if (!ok) break;
            frames_done++;
        }

        lock();
        s_status.position_ms = w.rate
            ? (int)(((uint64_t)frames_done * 1000ULL) / w.rate)
            : 0;
        unlock();
        if (!ok) break;
    }

    if (ok && out_n && !s_stop) ok = write_pcm(out, out_n);
    fclose(f);
    if (!ok && !s_status.last_error[0]) set_error("Audio codec write failed");
    return ok;
}

static bool play_tone_internal(void)
{
    const int total = AUDIO_RATE / 2;
    int16_t buf[256];
    int done = 0;

    while (done < total && !s_stop) {
        int n = total - done;
        if (n > (int)(sizeof(buf) / sizeof(buf[0]))) {
            n = (int)(sizeof(buf) / sizeof(buf[0]));
        }
        for (int i = 0; i < n; i++) {
            float phase = 2.0f * 3.14159265358979323846f * 440.0f *
                          (float)(done + i) / (float)AUDIO_RATE;
            buf[i] = (int16_t)(sinf(phase) * 11000.0f);
        }
        if (!write_pcm(buf, n)) {
            set_error("Audio codec write failed");
            return false;
        }
        done += n;
        lock();
        s_status.position_ms = done * 1000 / AUDIO_RATE;
        unlock();
    }
    return true;
}

static void playback_task(void *arg)
{
    bool tone = (bool)(uintptr_t)arg;
    bool ok = tone ? play_tone_internal() : play_wav_file(s_status.playing_name);

    int16_t silence[64] = {0};
    if (s_output) write_pcm(silence, sizeof(silence) / sizeof(silence[0]));

    lock();
    if (ok && !s_stop) s_status.position_ms = s_status.duration_ms;
    s_status.playing = false;
    s_status.tone = false;
    s_play_task = NULL;
    unlock();
    vTaskDelete(NULL);
}

esp_err_t prisma_audio_init(i2c_master_bus_handle_t i2c_bus)
{
    memset(&s_status, 0, sizeof(s_status));
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;

    s_status.volume = load_volume();
    s_status.saved_volume = s_status.volume;
    load_selected(s_status.selected_sample);
    if (!i2c_bus) {
        set_error("Shared I2C bus unavailable");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t es8311_probe = i2c_master_probe(i2c_bus, ES8311_PROBE_ADDR, 100);
    esp_err_t es7210_probe = i2c_master_probe(i2c_bus, ES7210_PROBE_ADDR, 100);
    s_status.es8311_present = es8311_probe == ESP_OK;
    s_status.es7210_present = es7210_probe == ESP_OK;

    ESP_LOGI(TAG,
             "I2C codec probe: ES8311 7-bit=0x%02X %s, ES7210 7-bit=0x%02X %s",
             (unsigned)ES8311_PROBE_ADDR,
             s_status.es8311_present ? "DETECTED" : "NOT DETECTED",
             (unsigned)ES7210_PROBE_ADDR,
             s_status.es7210_present ? "DETECTED" : "NOT DETECTED");

    if (!s_status.es8311_present) {
        set_error("ES8311 speaker codec not detected");
        return ESP_ERR_NOT_FOUND;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx, NULL);
    if (err != ESP_OK) {
        set_error("I2S channel init failed");
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = AUDIO_MCLK,
            .bclk = AUDIO_BCLK,
            .ws = AUDIO_WS,
            .dout = AUDIO_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    err = i2s_channel_init_std_mode(s_tx, &std_cfg);
    if (err != ESP_OK) {
        set_error("I2S std mode failed");
        return err;
    }
    err = i2s_channel_enable(s_tx);
    if (err != ESP_OK) {
        set_error("I2S enable failed");
        return err;
    }

    audio_codec_i2s_cfg_t data_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = NULL,
        .tx_handle = s_tx,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&data_cfg);
    if (!data_if) {
        set_error("Codec I2S interface failed");
        return ESP_FAIL;
    }

    /* Keep the codec driver's native 8-bit shifted address here. The codec
     * control adapter performs the conversion expected by the IDF I2C bus. */
    audio_codec_i2c_cfg_t i2c_cfg = {
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    if (!ctrl_if || !gpio_if) {
        set_error("Codec control interface failed");
        return ESP_FAIL;
    }

    es8311_codec_cfg_t codec_cfg = {0};
    codec_cfg.ctrl_if = ctrl_if;
    codec_cfg.gpio_if = gpio_if;
    codec_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    codec_cfg.pa_pin = AUDIO_PA;
    codec_cfg.pa_reverted = false;
    codec_cfg.master_mode = false;
    codec_cfg.use_mclk = true;
    codec_cfg.digital_mic = false;
    codec_cfg.hw_gain.pa_voltage = 5.0;
    codec_cfg.hw_gain.codec_dac_voltage = 3.3;

    const audio_codec_if_t *codec_if = es8311_codec_new(&codec_cfg);
    if (!codec_if) {
        set_error("ES8311 codec creation failed");
        return ESP_FAIL;
    }

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    s_output = esp_codec_dev_new(&dev_cfg);
    if (!s_output) {
        set_error("Speaker device creation failed");
        return ESP_FAIL;
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = AUDIO_BITS,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = AUDIO_RATE,
        .mclk_multiple = 0,
    };
    if (esp_codec_dev_open(s_output, &fs) != ESP_CODEC_DEV_OK) {
        set_error("ES8311 open failed");
        return ESP_FAIL;
    }
    if (esp_codec_dev_set_out_vol(s_output, s_status.volume) != ESP_CODEC_DEV_OK) {
        set_error("Volume init failed");
        return ESP_FAIL;
    }

    lock();
    s_status.initialized = true;
    s_status.last_error[0] = 0;
    unlock();

    ESP_LOGI(TAG, "AUDIO READY ES8311=%d ES7210=%d rate=%d volume=%d",
             s_status.es8311_present, s_status.es7210_present,
             AUDIO_RATE, s_status.volume);
    return ESP_OK;
}

void prisma_audio_get_status(prisma_audio_status_t *out)
{
    if (!out) return;
    lock();
    load_selected(s_status.selected_sample);
    *out = s_status;
    unlock();
}

esp_err_t prisma_audio_preview_volume(int volume)
{
    if (volume < 0 || volume > 100) return ESP_ERR_INVALID_ARG;
    if (!s_output) return ESP_ERR_INVALID_STATE;
    if (esp_codec_dev_set_out_vol(s_output, volume) != ESP_CODEC_DEV_OK) {
        return ESP_FAIL;
    }
    lock();
    s_status.volume = volume;
    unlock();
    return ESP_OK;
}

esp_err_t prisma_audio_set_volume(int volume)
{
    esp_err_t err = prisma_audio_preview_volume(volume);
    if (err != ESP_OK) return err;

    err = save_volume(volume);
    if (err != ESP_OK) return err;

    lock();
    s_status.saved_volume = volume;
    unlock();
    return ESP_OK;
}

static esp_err_t start_play(bool tone)
{
    if (!s_status.initialized || !s_output) return ESP_ERR_INVALID_STATE;

    lock();
    if (s_status.playing) {
        unlock();
        return ESP_ERR_INVALID_STATE;
    }

    if (!tone) {
        load_selected(s_status.selected_sample);
        if (!s_status.selected_sample[0]) {
            unlock();
            return ESP_ERR_NOT_FOUND;
        }
        strlcpy(s_status.playing_name, s_status.selected_sample,
                sizeof(s_status.playing_name));
    } else {
        strlcpy(s_status.playing_name, "440 HZ TEST",
                sizeof(s_status.playing_name));
    }

    s_status.playing = true;
    s_status.tone = tone;
    s_status.position_ms = 0;
    s_status.duration_ms = tone ? 500 : 0;
    s_status.source_rate = tone ? AUDIO_RATE : 0;
    s_status.source_channels = tone ? 1 : 0;
    s_status.source_bits = tone ? 16 : 0;
    s_status.last_error[0] = 0;
    s_stop = false;
    unlock();

    if (xTaskCreate(playback_task, "audio_play", PLAY_STACK,
                    (void *)(uintptr_t)tone, 5, &s_play_task) != pdPASS) {
        lock();
        s_status.playing = false;
        s_status.tone = false;
        s_play_task = NULL;
        unlock();
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t prisma_audio_play_selected(void)
{
    return start_play(false);
}

esp_err_t prisma_audio_play_tone(void)
{
    return start_play(true);
}

void prisma_audio_stop(void)
{
    s_stop = true;
}
