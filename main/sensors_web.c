#include "sensors_web.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "prisma_sensors.h"
#include "audio_service.h"

extern const uint8_t web_sensors_html_start[] asm("_binary_sensors_html_start");
extern const uint8_t web_sensors_html_end[] asm("_binary_sensors_html_end");

static bool s_beep_enabled;

static esp_err_t page_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(r, "Pragma", "no-cache");
    return httpd_resp_send(r, (const char *)web_sensors_html_start,
                           (ssize_t)(web_sensors_html_end - web_sensors_html_start));
}

static esp_err_t imu_get(httpd_req_t *r)
{
    prisma_imu_data_t d;
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    if (!prisma_sensors_imu_ready()) {
        httpd_resp_set_status(r, "503 Service Unavailable");
        return httpd_resp_sendstr(r, "{\"ready\":false}");
    }
    if (!prisma_sensors_read_imu(&d)) {
        httpd_resp_set_status(r, "503 Service Unavailable");
        return httpd_resp_sendstr(r, "{\"ready\":true,\"sample\":false}");
    }

    float amag = sqrtf((float)d.acc_x*d.acc_x + (float)d.acc_y*d.acc_y + (float)d.acc_z*d.acc_z);
    float gmag = sqrtf((float)d.gyr_x*d.gyr_x + (float)d.gyr_y*d.gyr_y + (float)d.gyr_z*d.gyr_z);
    static float gravity_ref = 0.0f;
    static float prev_ax = 0, prev_ay = 0, prev_az = 0;
    if (gravity_ref < 1.0f) {
        gravity_ref = amag > 1.0f ? amag : 1.0f;
    }
    gravity_ref = gravity_ref * 0.995f + amag * 0.005f;
    float da = sqrtf(((float)d.acc_x-prev_ax)*((float)d.acc_x-prev_ax) +
                     ((float)d.acc_y-prev_ay)*((float)d.acc_y-prev_ay) +
                     ((float)d.acc_z-prev_az)*((float)d.acc_z-prev_az));
    prev_ax=d.acc_x; prev_ay=d.acc_y; prev_az=d.acc_z;
    float linear = fabsf(amag-gravity_ref) / gravity_ref;
    float delta = da / gravity_ref;
    float motion = linear + delta * 0.5f + gmag / 32768.0f;
    if (motion > 1.0f) motion = 1.0f;

    char b[640];
    snprintf(b, sizeof(b),
        "{\"ready\":true,\"sample\":true,"
        "\"acc\":{\"x\":%d,\"y\":%d,\"z\":%d,\"magnitude\":%.1f},"
        "\"gyro\":{\"x\":%d,\"y\":%d,\"z\":%d,\"magnitude\":%.1f},"
        "\"angle\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
        "\"movement\":{\"index\":%.4f,\"linear\":%.4f,\"delta\":%.4f,\"moving\":%s}}",
        d.acc_x,d.acc_y,d.acc_z,amag,d.gyr_x,d.gyr_y,d.gyr_z,gmag,
        d.angle_x,d.angle_y,d.angle_z,motion,linear,delta,motion>0.035f?"true":"false");
    return httpd_resp_sendstr(r,b);
}

static bool query_on(httpd_req_t *r)
{
    char q[64], v[8];
    if (httpd_req_get_url_query_str(r,q,sizeof(q)) != ESP_OK) return false;
    if (httpd_query_key_value(q,"on",v,sizeof(v)) != ESP_OK) return false;
    return atoi(v) != 0;
}

static esp_err_t beep_post(httpd_req_t *r)
{
    bool on = query_on(r);
    esp_err_t e = ESP_OK;
    if (on) {
        e = prisma_audio_sensor_tone_start(440.0f);
        s_beep_enabled = (e == ESP_OK);
    } else {
        prisma_audio_sensor_tone_stop();
        s_beep_enabled = false;
    }
    char b[96];
    snprintf(b,sizeof(b),"{\"ok\":%s,\"enabled\":%s}",
             e==ESP_OK?"true":"false",s_beep_enabled?"true":"false");
    httpd_resp_set_type(r,"application/json");
    return httpd_resp_sendstr(r,b);
}

void prisma_sensors_web_register(httpd_handle_t s)
{
    if (!s) return;
    httpd_uri_t page={.uri="/sensors",.method=HTTP_GET,.handler=page_get};
    httpd_uri_t api={.uri="/api/sensors/imu",.method=HTTP_GET,.handler=imu_get};
    httpd_uri_t beep={.uri="/api/sensors/beep",.method=HTTP_POST,.handler=beep_post};
    httpd_register_uri_handler(s,&page);
    httpd_register_uri_handler(s,&api);
    httpd_register_uri_handler(s,&beep);
}
