#include "security_service.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "nvs.h"

#define SEC_NS "sec_cfg"
#define KEY_SALT "salt"
#define KEY_HASH "hash"
#define SALT_BYTES 16
#define HASH_BYTES 32
#define HASH_ROUNDS 12000
#define MAX_SESSIONS 4
#define SESSION_BYTES 32
#define SESSION_IDLE_US (12LL * 60LL * 60LL * 1000000LL)
#define RECOVERY_GPIO GPIO_NUM_0
#define RECOVERY_HOLD_MS 8000

static const char *TAG = "SECURITY";

typedef struct {
    bool used;
    char token[PRISMA_SECURITY_SESSION_HEX + 1];
    int64_t last_seen_us;
} session_t;

static SemaphoreHandle_t s_lock;
static bool s_initialized;
static bool s_password_set;
static uint8_t s_salt[SALT_BYTES];
static uint8_t s_hash[HASH_BYTES];
static uint32_t s_generation = 1;
static session_t s_sessions[MAX_SESSIONS];

static void bytes_hex(const uint8_t *src, size_t n, char *dst)
{
    static const char hx[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        dst[i * 2] = hx[src[i] >> 4];
        dst[i * 2 + 1] = hx[src[i] & 0x0f];
    }
    dst[n * 2] = 0;
}

static bool constant_equal(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

static void sha256_parts(const uint8_t *a, size_t an, const uint8_t *b, size_t bn,
                         const uint8_t *c, size_t cn, uint8_t out[HASH_BYTES])
{
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    if (a && an) mbedtls_sha256_update(&ctx, a, an);
    if (b && bn) mbedtls_sha256_update(&ctx, b, bn);
    if (c && cn) mbedtls_sha256_update(&ctx, c, cn);
    mbedtls_sha256_finish(&ctx, out);
    mbedtls_sha256_free(&ctx);
}

static void derive_hash(const uint8_t salt[SALT_BYTES], const char *password, uint8_t out[HASH_BYTES])
{
    uint8_t state[HASH_BYTES];
    const uint8_t *pw = (const uint8_t *)(password ? password : "");
    size_t pwlen = strlen((const char *)pw);
    sha256_parts(salt, SALT_BYTES, pw, pwlen, NULL, 0, state);
    for (int i = 1; i < HASH_ROUNDS; i++) {
        uint8_t round[4] = {(uint8_t)(i >> 24), (uint8_t)(i >> 16), (uint8_t)(i >> 8), (uint8_t)i};
        sha256_parts(state, sizeof(state), salt, SALT_BYTES, round, sizeof(round), state);
    }
    memcpy(out, state, HASH_BYTES);
    memset(state, 0, sizeof(state));
}

static void invalidate_locked(void)
{
    memset(s_sessions, 0, sizeof(s_sessions));
    s_generation++;
    if (!s_generation) s_generation = 1;
}

static void load_password(void)
{
    nvs_handle_t h;
    if (nvs_open(SEC_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t sl = sizeof(s_salt), hl = sizeof(s_hash);
    bool ok = nvs_get_blob(h, KEY_SALT, s_salt, &sl) == ESP_OK && sl == sizeof(s_salt) &&
              nvs_get_blob(h, KEY_HASH, s_hash, &hl) == ESP_OK && hl == sizeof(s_hash);
    s_password_set = ok;
    nvs_close(h);
}

static bool save_password_blob(const uint8_t salt[SALT_BYTES], const uint8_t hash[HASH_BYTES])
{
    nvs_handle_t h;
    if (nvs_open(SEC_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t e = nvs_set_blob(h, KEY_SALT, salt, SALT_BYTES);
    if (e == ESP_OK) e = nvs_set_blob(h, KEY_HASH, hash, HASH_BYTES);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    return e == ESP_OK;
}

static bool erase_password_blob(void)
{
    nvs_handle_t h;
    if (nvs_open(SEC_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_erase_key(h, KEY_SALT);
    nvs_erase_key(h, KEY_HASH);
    esp_err_t e = nvs_commit(h);
    nvs_close(h);
    return e == ESP_OK;
}

static void recovery_task(void *arg)
{
    (void)arg;
    bool down = false;
    TickType_t started = 0;
    while (1) {
        bool pressed = gpio_get_level(RECOVERY_GPIO) == 0;
        if (pressed && !down) {
            down = true;
            started = xTaskGetTickCount();
        } else if (!pressed) {
            down = false;
        } else if (down) {
            uint32_t ms = (uint32_t)((xTaskGetTickCount() - started) * portTICK_PERIOD_MS);
            if (ms >= RECOVERY_HOLD_MS) {
                ESP_LOGW(TAG, "BOOT held 8 s -> clear admin password and sessions");
                prisma_security_force_clear_password();
                while (gpio_get_level(RECOVERY_GPIO) == 0) vTaskDelay(pdMS_TO_TICKS(50));
                down = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t prisma_security_init(void)
{
    if (s_initialized) return ESP_OK;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    load_password();

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << RECOVERY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    if (xTaskCreate(recovery_task, "sec_recovery", 2560, NULL, 3, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    s_initialized = true;
    ESP_LOGI(TAG, "admin security ready; password=%s; BOOT 8 s clears credentials", s_password_set ? "SET" : "NOT SET");
    return ESP_OK;
}

void prisma_security_get_status(prisma_security_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out->password_set = s_password_set;
    out->recovery_available = true;
    out->session_generation = s_generation;
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (s_sessions[i].used && now - s_sessions[i].last_seen_us <= SESSION_IDLE_US) out->active_sessions++;
    }
    xSemaphoreGive(s_lock);
}

bool prisma_security_verify_password(const char *password)
{
    if (!s_lock) return false;
    uint8_t salt[SALT_BYTES], expected[HASH_BYTES];
    bool set;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    set = s_password_set;
    memcpy(salt, s_salt, sizeof(salt));
    memcpy(expected, s_hash, sizeof(expected));
    xSemaphoreGive(s_lock);
    if (!set) return true;

    uint8_t got[HASH_BYTES];
    derive_hash(salt, password ? password : "", got);
    bool ok = constant_equal(got, expected, sizeof(got));
    memset(got, 0, sizeof(got));
    memset(expected, 0, sizeof(expected));
    return ok;
}

bool prisma_security_set_password(const char *current_password, const char *new_password)
{
    if (!s_lock || !new_password) return false;
    size_t n = strlen(new_password);
    if (n < 8 || n > 63) return false;

    prisma_security_status_t st;
    prisma_security_get_status(&st);
    if (st.password_set && !prisma_security_verify_password(current_password ? current_password : "")) return false;

    uint8_t salt[SALT_BYTES], hash[HASH_BYTES];
    for (size_t i = 0; i < sizeof(salt); i += 4) {
        uint32_t r = esp_random();
        size_t take = sizeof(salt) - i < 4 ? sizeof(salt) - i : 4;
        memcpy(salt + i, &r, take);
    }
    derive_hash(salt, new_password, hash);
    if (!save_password_blob(salt, hash)) return false;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(s_salt, salt, sizeof(s_salt));
    memcpy(s_hash, hash, sizeof(s_hash));
    s_password_set = true;
    invalidate_locked();
    xSemaphoreGive(s_lock);
    memset(hash, 0, sizeof(hash));
    return true;
}

bool prisma_security_remove_password(const char *current_password)
{
    if (!s_lock) return false;
    prisma_security_status_t st;
    prisma_security_get_status(&st);
    if (!st.password_set) return true;
    if (!prisma_security_verify_password(current_password ? current_password : "")) return false;
    if (!erase_password_blob()) return false;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_password_set = false;
    memset(s_salt, 0, sizeof(s_salt));
    memset(s_hash, 0, sizeof(s_hash));
    invalidate_locked();
    xSemaphoreGive(s_lock);
    return true;
}

void prisma_security_force_clear_password(void)
{
    if (!s_lock) return;
    erase_password_blob();
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_password_set = false;
    memset(s_salt, 0, sizeof(s_salt));
    memset(s_hash, 0, sizeof(s_hash));
    invalidate_locked();
    xSemaphoreGive(s_lock);
}

void prisma_security_invalidate_sessions(void)
{
    if (!s_lock) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    invalidate_locked();
    xSemaphoreGive(s_lock);
}

bool prisma_security_create_session(const char *password, char out_token[PRISMA_SECURITY_SESSION_HEX + 1])
{
    if (!s_lock || !out_token || !prisma_security_verify_password(password ? password : "")) return false;
    uint8_t raw[SESSION_BYTES];
    for (size_t i = 0; i < sizeof(raw); i += 4) {
        uint32_t r = esp_random();
        size_t take = sizeof(raw) - i < 4 ? sizeof(raw) - i : 4;
        memcpy(raw + i, &r, take);
    }
    bytes_hex(raw, sizeof(raw), out_token);
    memset(raw, 0, sizeof(raw));

    xSemaphoreTake(s_lock, portMAX_DELAY);
    int slot = -1;
    int64_t oldest = INT64_MAX;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!s_sessions[i].used) { slot = i; break; }
        if (s_sessions[i].last_seen_us < oldest) { oldest = s_sessions[i].last_seen_us; slot = i; }
    }
    s_sessions[slot].used = true;
    strlcpy(s_sessions[slot].token, out_token, sizeof(s_sessions[slot].token));
    s_sessions[slot].last_seen_us = esp_timer_get_time();
    xSemaphoreGive(s_lock);
    return true;
}

bool prisma_security_validate_session(const char *token)
{
    if (!s_lock) return false;
    prisma_security_status_t st;
    prisma_security_get_status(&st);
    if (!st.password_set) return true;
    if (!token || strlen(token) != PRISMA_SECURITY_SESSION_HEX) return false;

    bool ok = false;
    int64_t now = esp_timer_get_time();
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!s_sessions[i].used) continue;
        if (now - s_sessions[i].last_seen_us > SESSION_IDLE_US) {
            memset(&s_sessions[i], 0, sizeof(s_sessions[i]));
            continue;
        }
        if (strcmp(s_sessions[i].token, token) == 0) {
            s_sessions[i].last_seen_us = now;
            ok = true;
            break;
        }
    }
    xSemaphoreGive(s_lock);
    return ok;
}

void prisma_security_logout_token(const char *token)
{
    if (!s_lock || !token) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (s_sessions[i].used && strcmp(s_sessions[i].token, token) == 0) {
            memset(&s_sessions[i], 0, sizeof(s_sessions[i]));
            break;
        }
    }
    xSemaphoreGive(s_lock);
}
