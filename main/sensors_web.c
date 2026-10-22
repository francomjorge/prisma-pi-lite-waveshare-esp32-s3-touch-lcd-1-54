#include "sensors_web.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "prisma_sensors.h"
#include "sensor_audio_control.h"

extern const uint8_t web_sensors_html_start[] asm("_binary_sensors_html_start");
extern const uint8_t web_sensors_html_end[] asm("_binary_sensors_html_end");

static esp_err_t page_get(httpd_req_t *r)
{
    httpd_resp_set_type(r,"text/html");
    httpd_resp_set_hdr(r,"Cache-Control","no-store, no-cache,must-revalidate,max-age=0");
    httpd_resp_set_hdr(r,"Pragma","no-cache");
    return httpd_resp_send(r,(const char*)web_sensors_html_start,(ssize_t)(web_sensors_html_end-web_sensors_html_start));
}

static esp_err_t imu_get(httpd_req_t *r)
{
    prisma_imu_data_t d;
    httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    if(!prisma_sensors_imu_ready()){
        httpd_resp_set_status(r,"503 Service Unavailable");
        return httpd_resp_sendstr(r,"{\"ready\":false}");
    }
    if(!prisma_sensors_read_imu(&d)){
        httpd_resp_set_status(r,"503 Service Unavailable");
        return httpd_resp_sendstr(r,"{\"ready\":true,\"sample\":false}");
    }
    float amag=sqrtf((float)d.acc_x*d.acc_x+(float)d.acc_y*d.acc_y+(float)d.acc_z*d.acc_z);
    float gmag=sqrtf((float)d.gyr_x*d.gyr_x+(float)d.gyr_y*d.gyr_y+(float)d.gyr_z*d.gyr_z);
    static float gravity_ref=0,prev_ax=0,prev_ay=0,prev_az=0;
    if(gravity_ref<1){
        gravity_ref=amag>1?amag:1;
    }
    gravity_ref=gravity_ref*.995f+amag*.005f;
    float da=sqrtf(((float)d.acc_x-prev_ax)*((float)d.acc_x-prev_ax)+((float)d.acc_y-prev_ay)*((float)d.acc_y-prev_ay)+((float)d.acc_z-prev_az)*((float)d.acc_z-prev_az));
    prev_ax=d.acc_x;prev_ay=d.acc_y;prev_az=d.acc_z;
    float linear=fabsf(amag-gravity_ref)/gravity_ref,delta=da/gravity_ref;
    float motion=linear+delta*.5f+gmag/32768.0f;
    if(motion>1)motion=1;
    prisma_sensor_audio_state_t a;
    prisma_sensor_audio_get_state(&a);
    char b[800];
    snprintf(b,sizeof(b),"{\"ready\":true,\"sample\":true,\"acc\":{\"x\":%d,\"y\":%d,\"z\":%d,\"magnitude\":%.1f},\"gyro\":{\"x\":%d,\"y\":%d,\"z\":%d,\"magnitude\":%.1f},\"angle\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"movement\":{\"index\":%.4f,\"linear\":%.4f,\"delta\":%.4f,\"moving\":%s},\"beep\":{\"master\":%s,\"active\":%s,\"face_down\":%s,\"hz\":%.1f,\"volume\":%d}}",
      d.acc_x,d.acc_y,d.acc_z,amag,d.gyr_x,d.gyr_y,d.gyr_z,gmag,d.angle_x,d.angle_y,d.angle_z,motion,linear,delta,motion>.035f?"true":"false",a.enabled?"true":"false",a.active?"true":"false",a.face_down?"true":"false",a.pitch_hz,a.volume);
    return httpd_resp_sendstr(r,b);
}

static bool query_int(httpd_req_t*r,const char*key,int*out){char q[64],v[12];if(httpd_req_get_url_query_str(r,q,sizeof(q))!=ESP_OK||httpd_query_key_value(q,key,v,sizeof(v))!=ESP_OK)return false;*out=atoi(v);return true;}
static esp_err_t beep_post(httpd_req_t*r){int on=0;query_int(r,"on",&on);esp_err_t e=prisma_sensor_audio_set_enabled(on!=0);prisma_sensor_audio_state_t a;prisma_sensor_audio_get_state(&a);char b[160];snprintf(b,sizeof(b),"{\"ok\":%s,\"enabled\":%s,\"active\":%s,\"hz\":%.1f,\"volume\":%d}",e==ESP_OK?"true":"false",a.enabled?"true":"false",a.active?"true":"false",a.pitch_hz,a.volume);httpd_resp_set_type(r,"application/json");return httpd_resp_sendstr(r,b);}
static esp_err_t volume_post(httpd_req_t*r){int v=100;if(!query_int(r,"value",&v))v=100;esp_err_t e=prisma_sensor_audio_set_volume(v);prisma_sensor_audio_state_t a;prisma_sensor_audio_get_state(&a);char b[96];snprintf(b,sizeof(b),"{\"ok\":%s,\"volume\":%d}",e==ESP_OK?"true":"false",a.volume);httpd_resp_set_type(r,"application/json");return httpd_resp_sendstr(r,b);}

void prisma_sensors_web_register(httpd_handle_t s)
{
    if(!s)return;
    httpd_uri_t page={.uri="/sensors",.method=HTTP_GET,.handler=page_get};
    httpd_uri_t api={.uri="/api/sensors/imu",.method=HTTP_GET,.handler=imu_get};
    httpd_uri_t beep={.uri="/api/sensors/beep",.method=HTTP_POST,.handler=beep_post};
    httpd_uri_t vol={.uri="/api/sensors/volume",.method=HTTP_POST,.handler=volume_post};
    httpd_register_uri_handler(s,&page);httpd_register_uri_handler(s,&api);httpd_register_uri_handler(s,&beep);httpd_register_uri_handler(s,&vol);
}
