#include "sensors_web.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prisma_sensors.h"
#include "audio_service.h"

extern const uint8_t web_sensors_html_start[] asm("_binary_sensors_html_start");
extern const uint8_t web_sensors_html_end[] asm("_binary_sensors_html_end");

static float s_motion_min = 0.035f;
static float s_motion_max = 0.350f;
static float s_hz_min = 220.0f;
static float s_hz_max = 1100.0f;
static bool s_beep_enabled;
static float s_last_motion;
static float s_last_mapped;

static esp_err_t page_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(r, "Pragma", "no-cache");
    return httpd_resp_send(r, (const char *)web_sensors_html_start,
                           (ssize_t)(web_sensors_html_end - web_sensors_html_start));
}

static float clamp01(float v){return v<0?0:(v>1?1:v);}
static float mapped_motion(float motion){float span=s_motion_max-s_motion_min;if(span<0.001f)span=0.001f;return clamp01((motion-s_motion_min)/span);}
static float mapped_hz(float m){return s_hz_min+(s_hz_max-s_hz_min)*m;}

static esp_err_t imu_get(httpd_req_t *r)
{
    prisma_imu_data_t d;httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    if(!prisma_sensors_imu_ready()){httpd_resp_set_status(r,"503 Service Unavailable");return httpd_resp_sendstr(r,"{\"ready\":false}");}
    if(!prisma_sensors_read_imu(&d)){httpd_resp_set_status(r,"503 Service Unavailable");return httpd_resp_sendstr(r,"{\"ready\":true,\"sample\":false}");}
    float amag=sqrtf((float)d.acc_x*d.acc_x+(float)d.acc_y*d.acc_y+(float)d.acc_z*d.acc_z);
    float gmag=sqrtf((float)d.gyr_x*d.gyr_x+(float)d.gyr_y*d.gyr_y+(float)d.gyr_z*d.gyr_z);
    static float gravity_ref=0.0f,prev_ax=0,prev_ay=0,prev_az=0;
    if(gravity_ref<1.0f)gravity_ref=amag>1.0f?amag:1.0f;gravity_ref=gravity_ref*0.995f+amag*0.005f;
    float da=sqrtf(((float)d.acc_x-prev_ax)*((float)d.acc_x-prev_ax)+((float)d.acc_y-prev_ay)*((float)d.acc_y-prev_ay)+((float)d.acc_z-prev_az)*((float)d.acc_z-prev_az));
    prev_ax=d.acc_x;prev_ay=d.acc_y;prev_az=d.acc_z;
    float linear=fabsf(amag-gravity_ref)/gravity_ref,delta=da/gravity_ref,motion=linear+delta*0.5f+gmag/32768.0f;if(motion>1.0f)motion=1.0f;
    s_last_motion=motion;s_last_mapped=mapped_motion(motion);float hz=mapped_hz(s_last_mapped);
    if(s_beep_enabled&&prisma_audio_sensor_tone_active())prisma_audio_sensor_tone_set_frequency(hz);
    char b[900];snprintf(b,sizeof(b),"{\"ready\":true,\"sample\":true,\"acc\":{\"x\":%d,\"y\":%d,\"z\":%d,\"magnitude\":%.1f},\"gyro\":{\"x\":%d,\"y\":%d,\"z\":%d,\"magnitude\":%.1f},\"angle\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"movement\":{\"index\":%.4f,\"linear\":%.4f,\"delta\":%.4f,\"moving\":%s,\"mapped\":%.4f},\"synth\":{\"enabled\":%s,\"active\":%s,\"hz\":%.1f,\"motion_min\":%.4f,\"motion_max\":%.4f,\"hz_min\":%.1f,\"hz_max\":%.1f}}",d.acc_x,d.acc_y,d.acc_z,amag,d.gyr_x,d.gyr_y,d.gyr_z,gmag,d.angle_x,d.angle_y,d.angle_z,motion,linear,delta,motion>s_motion_min?"true":"false",s_last_mapped,s_beep_enabled?"true":"false",prisma_audio_sensor_tone_active()?"true":"false",hz,s_motion_min,s_motion_max,s_hz_min,s_hz_max);
    return httpd_resp_sendstr(r,b);
}
static bool query_float(httpd_req_t*r,const char*key,float*out){char q[256],v[32];if(httpd_req_get_url_query_str(r,q,sizeof(q))!=ESP_OK)return false;if(httpd_query_key_value(q,key,v,sizeof(v))!=ESP_OK)return false;char*e=NULL;float f=strtof(v,&e);if(!e||e==v||*e)return false;*out=f;return true;}
static esp_err_t synth_config_post(httpd_req_t*r){float mn,mx,hmin,hmax;if(!query_float(r,"motion_min",&mn)||!query_float(r,"motion_max",&mx)||!query_float(r,"hz_min",&hmin)||!query_float(r,"hz_max",&hmax))return httpd_resp_send_err(r,400,"motion_min motion_max hz_min hz_max required");if(mn<0||mx>1||mx<=mn||hmin<80||hmax>2000||hmax<=hmin)return httpd_resp_send_err(r,400,"invalid ranges");s_motion_min=mn;s_motion_max=mx;s_hz_min=hmin;s_hz_max=hmax;httpd_resp_set_type(r,"application/json");return httpd_resp_sendstr(r,"{\"ok\":true}");}
static esp_err_t synth_enable_post(httpd_req_t*r){float on=0;query_float(r,"on",&on);s_beep_enabled=on>=0.5f;esp_err_t e=ESP_OK;if(s_beep_enabled){float hz=mapped_hz(mapped_motion(s_last_motion));e=prisma_audio_sensor_tone_start(hz);if(e!=ESP_OK)s_beep_enabled=false;}else prisma_audio_sensor_tone_stop();char b[160];snprintf(b,sizeof(b),"{\"ok\":%s,\"enabled\":%s,\"hz\":%.1f}",e==ESP_OK?"true":"false",s_beep_enabled?"true":"false",prisma_audio_sensor_tone_frequency());httpd_resp_set_type(r,"application/json");return httpd_resp_sendstr(r,b);}

void prisma_sensors_web_register(httpd_handle_t s)
{
    if(!s)return;
    httpd_uri_t page={.uri="/sensor",.method=HTTP_GET,.handler=page_get};
    httpd_uri_t page_alias={.uri="/sensors",.method=HTTP_GET,.handler=page_get};
    httpd_uri_t api={.uri="/api/sensors/imu",.method=HTTP_GET,.handler=imu_get};
    httpd_uri_t cfg={.uri="/api/sensors/synth/config",.method=HTTP_POST,.handler=synth_config_post};
    httpd_uri_t enable={.uri="/api/sensors/synth/enable",.method=HTTP_POST,.handler=synth_enable_post};
    httpd_register_uri_handler(s,&page);httpd_register_uri_handler(s,&page_alias);httpd_register_uri_handler(s,&api);httpd_register_uri_handler(s,&cfg);httpd_register_uri_handler(s,&enable);
}
