#include "sensor_audio_control.h"
#include <math.h>
#include <string.h>
#include "audio_service.h"
#include "prisma_sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static SemaphoreHandle_t s_lock;
static prisma_sensor_audio_state_t s = {
    .enabled=false,.active=false,.face_down=false,.volume=100,.pitch_hz=440.0f
};
static TaskHandle_t s_task;

static float pitch_from_x(float x)
{
    if (x < -90.0f) x = -90.0f;
    if (x > 90.0f) x = 90.0f;
    return 440.0f * powf(2.0f, x / 90.0f);
}

static void ctl_task(void *arg)
{
    (void)arg;
    while (1) {
        prisma_imu_data_t d;
        if (prisma_sensors_imu_ready() && prisma_sensors_read_imu(&d)) {
            bool enabled, face;
            float hz = pitch_from_x(d.angle_x);
            xSemaphoreTake(s_lock, portMAX_DELAY);
            if (!s.face_down && d.angle_z < 20.0f) s.face_down=true;
            else if (s.face_down && d.angle_z > 35.0f) s.face_down=false;
            s.angle_x=d.angle_x; s.angle_y=d.angle_y; s.angle_z=d.angle_z;
            s.pitch_hz=hz; enabled=s.enabled; face=s.face_down;
            xSemaphoreGive(s_lock);

            if (enabled && !face) {
                if (!prisma_audio_sensor_tone_active()) prisma_audio_sensor_tone_start(hz);
                else prisma_audio_sensor_tone_set_frequency(hz);
            } else if (prisma_audio_sensor_tone_active()) {
                prisma_audio_sensor_tone_stop();
            }
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s.active=prisma_audio_sensor_tone_active() && enabled && !face;
            xSemaphoreGive(s_lock);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t prisma_sensor_audio_start(void)
{
    if (s_task) return ESP_OK;
    if (!s_lock) s_lock=xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    /* Sensor instrument starts at the only reliably audible level found so far. */
    prisma_audio_set_volume(100);
    s.volume=100;
    if (xTaskCreate(ctl_task,"sensor_audio",4096,NULL,5,&s_task)!=pdPASS) {
        s_task=NULL; return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t prisma_sensor_audio_set_enabled(bool enabled)
{
    if (!s_lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock,portMAX_DELAY); s.enabled=enabled; xSemaphoreGive(s_lock);
    if (!enabled) prisma_audio_sensor_tone_stop();
    return ESP_OK;
}

esp_err_t prisma_sensor_audio_set_volume(int volume)
{
    if (volume<0 || volume>100) return ESP_ERR_INVALID_ARG;
    esp_err_t e=prisma_audio_set_volume(volume);
    if (e==ESP_OK) { xSemaphoreTake(s_lock,portMAX_DELAY); s.volume=volume; xSemaphoreGive(s_lock); }
    return e;
}

void prisma_sensor_audio_get_state(prisma_sensor_audio_state_t *out)
{
    if (!out) return;
    if (!s_lock) { *out=s; return; }
    xSemaphoreTake(s_lock,portMAX_DELAY); *out=s; xSemaphoreGive(s_lock);
}
