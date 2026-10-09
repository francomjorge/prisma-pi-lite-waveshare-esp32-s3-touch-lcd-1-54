#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include "wifi_portal.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"

static const char *TAG = "PRISMA_WIFI";

/* Legacy state is intentionally retained during M4.8 so the temporary
 * Sync Lab page can compare it with the real appliance variables. */
static volatile bool control_on = true;
static volatile int control_mode = 0;
static volatile int control_intensity = 61;
static volatile int control_generation = 1;
static char control_filename[32] = "F1_MOON.JPG";
static volatile bool control_effects[6] = {false,false,false,false,false,true};
static volatile int control_effect_values[6] = {50,50,50,50,50,50};

bool prisma_control_get_on(void){return control_on;}
int prisma_control_get_mode(void){return control_mode;}
int prisma_control_get_intensity(void){return control_intensity;}
int prisma_control_get_generation(void){return control_generation;}
void prisma_control_set_on(bool v){control_on=v;}
void prisma_control_set_mode(int v){control_mode=v?1:0;}
void prisma_control_set_intensity(int v){if(v<0)v=0;if(v>100)v=100;control_intensity=v;}
void prisma_control_set_generation(int v){control_generation=v;}
void prisma_control_set_filename(const char *v){if(v&&*v)strlcpy(control_filename,v,sizeof(control_filename));}
void prisma_control_set_effect(int effect,bool enabled){if(effect<0||effect>=6)return;if(effect==5)enabled=true;control_effects[effect]=enabled;}
bool prisma_control_get_effect(int effect){if(effect<0||effect>=6)return false;if(effect==5)return true;return control_effects[effect];}
void prisma_control_set_effect_value(int effect,int value){if(effect<0||effect>=6)return;if(value<0)value=0;if(value>100)value=100;control_effect_values[effect]=value;}
int prisma_control_get_effect_value(int effect){if(effect<0||effect>=6)return 0;return control_effect_values[effect];}

extern const uint8_t web_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t web_index_html_end[] asm("_binary_index_html_end");
extern const uint8_t web_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t web_style_css_end[] asm("_binary_style_css_end");

static void no_cache(httpd_req_t *r)
{
    httpd_resp_set_hdr(r,"Cache-Control","no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(r,"Pragma","no-cache");
    httpd_resp_set_hdr(r,"Expires","0");
}

static esp_err_t send_embedded(httpd_req_t*r,const char*type,const uint8_t*s,const uint8_t*e)
{
    httpd_resp_set_type(r,type);
    no_cache(r);
    return httpd_resp_send(r,(const char*)s,(ssize_t)(e-s));
}

static esp_err_t page_get(httpd_req_t*r){return send_embedded(r,"text/html",web_index_html_start,web_index_html_end);}
static esp_err_t css_get(httpd_req_t*r){return send_embedded(r,"text/css",web_style_css_start,web_style_css_end);}

/* Existing Page 2 endpoint. Kept untouched for comparison with the new
 * independent Sync Lab endpoint below. */
static esp_err_t state_get(httpd_req_t*r)
{
    static bool have_last = false;
    static bool last_on = false;
    static int last_mode = -1;
    static int last_intensity = -1;
    static char last_filename[32] = "";
    static int generation = 0;

    bool on = prisma_appliance_get_on();
    int mode = prisma_appliance_get_mode() ? 1 : 0;
    int intensity = prisma_appliance_get_intensity();
    const char *filename = prisma_appliance_get_filename();
    if (!filename || !*filename) filename = "F1_MOON.JPG";

    bool changed = !have_last || last_on != on || last_mode != mode ||
                   last_intensity != intensity || strcmp(last_filename, filename) != 0;
    if (changed) {
        generation++;
        if (generation <= 0) generation = 1;
        last_on = on;
        last_mode = mode;
        last_intensity = intensity;
        strlcpy(last_filename, filename, sizeof(last_filename));
        have_last = true;
        ESP_LOGI(TAG,
                 "PAGE2 GET TX gen=%d on=%s source=%s intensity=%d mode=%s",
                 generation,
                 on ? "ON" : "OFF",
                 filename,
                 intensity,
                 mode ? "BYPASS" : "RANDOM");
    }

    char b[256];
    int n = snprintf(b,sizeof(b),
             "{\"on\":%s,\"mode\":%d,\"intensity\":%d,\"generation\":%d,\"filename\":\"%s\"}",
             on ? "true" : "false", mode, intensity, generation, filename);
    if (n < 0 || n >= (int)sizeof(b)) {
        httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"state overflow");
        return ESP_FAIL;
    }

    httpd_resp_set_type(r,"application/json");
    no_cache(r);
    return httpd_resp_send(r,b,n);
}

typedef struct {
    bool on;
    int mode;
    int intensity;
    char filename[32];
    bool effects[6];
    int values[6];
} sync_snapshot_t;

static void read_direct(sync_snapshot_t *s)
{
    memset(s,0,sizeof(*s));
    s->on = prisma_appliance_get_on();
    s->mode = prisma_appliance_get_mode() ? 1 : 0;
    s->intensity = prisma_appliance_get_intensity();
    const char *filename = prisma_appliance_get_filename();
    if (!filename || !*filename) filename = "F1_MOON.JPG";
    strlcpy(s->filename,filename,sizeof(s->filename));
    for (int i=0;i<6;i++) {
        s->effects[i] = prisma_appliance_get_effect(i);
        s->values[i] = prisma_appliance_get_effect_value(i);
    }
    s->effects[5] = true;
}

static void read_legacy(sync_snapshot_t *s)
{
    memset(s,0,sizeof(*s));
    s->on = control_on;
    s->mode = control_mode ? 1 : 0;
    s->intensity = control_intensity;
    strlcpy(s->filename,control_filename,sizeof(s->filename));
    for (int i=0;i<6;i++) {
        s->effects[i] = (i==5) ? true : control_effects[i];
        s->values[i] = control_effect_values[i];
    }
}

static bool snapshots_equal(const sync_snapshot_t *a,const sync_snapshot_t *b)
{
    if (a->on != b->on || a->mode != b->mode || a->intensity != b->intensity ||
        strcmp(a->filename,b->filename) != 0) return false;
    for (int i=0;i<6;i++) {
        if (a->effects[i] != b->effects[i] || a->values[i] != b->values[i]) return false;
    }
    return true;
}

/*
 * Independent diagnostic endpoint.
 * - Reads the actual appliance variables directly on every request.
 * - Returns a monotonically increasing HTTP request id.
 * - Returns state_seq, which only increments when the direct appliance state changes.
 * - Includes the old mirrored/copy state for visual comparison.
 * - Logs EVERY request to serial so we can prove whether the browser is still polling.
 */
static esp_err_t debug_state_get(httpd_req_t *r)
{
    static uint32_t request_id = 0;
    static uint32_t state_seq = 0;
    static bool have_last = false;
    static sync_snapshot_t last_direct;

    sync_snapshot_t direct;
    sync_snapshot_t legacy;
    read_direct(&direct);
    read_legacy(&legacy);

    if (!have_last || !snapshots_equal(&direct,&last_direct)) {
        state_seq++;
        last_direct = direct;
        have_last = true;
    }
    request_id++;

    uint64_t uptime_ms = (uint64_t)(esp_timer_get_time()/1000ULL);
    uint32_t free_heap = esp_get_free_heap_size();
    bool match = snapshots_equal(&direct,&legacy);

    char b[1536];
    int n = snprintf(b,sizeof(b),
        "{"
        "\"request_id\":%lu,\"state_seq\":%lu,\"uptime_ms\":%llu,\"free_heap\":%lu,\"match\":%s,"
        "\"direct\":{\"on\":%s,\"mode\":%d,\"intensity\":%d,\"filename\":\"%s\","
        "\"effects\":[%s,%s,%s,%s,%s,true],\"values\":[%d,%d,%d,%d,%d,%d]},"
        "\"legacy\":{\"on\":%s,\"mode\":%d,\"intensity\":%d,\"filename\":\"%s\","
        "\"effects\":[%s,%s,%s,%s,%s,true],\"values\":[%d,%d,%d,%d,%d,%d]}"
        "}",
        (unsigned long)request_id,(unsigned long)state_seq,(unsigned long long)uptime_ms,
        (unsigned long)free_heap,match?"true":"false",
        direct.on?"true":"false",direct.mode,direct.intensity,direct.filename,
        direct.effects[0]?"true":"false",direct.effects[1]?"true":"false",direct.effects[2]?"true":"false",
        direct.effects[3]?"true":"false",direct.effects[4]?"true":"false",
        direct.values[0],direct.values[1],direct.values[2],direct.values[3],direct.values[4],direct.values[5],
        legacy.on?"true":"false",legacy.mode,legacy.intensity,legacy.filename,
        legacy.effects[0]?"true":"false",legacy.effects[1]?"true":"false",legacy.effects[2]?"true":"false",
        legacy.effects[3]?"true":"false",legacy.effects[4]?"true":"false",
        legacy.values[0],legacy.values[1],legacy.values[2],legacy.values[3],legacy.values[4],legacy.values[5]);

    if (n < 0 || n >= (int)sizeof(b)) {
        ESP_LOGE(TAG,"SYNC_LAB JSON overflow");
        httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"debug state overflow");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "SYNC_LAB GET req=%lu seq=%lu match=%s direct[%s %s %d %s] legacy[%s %s %d %s]",
             (unsigned long)request_id,(unsigned long)state_seq,match?"YES":"NO",
             direct.on?"ON":"OFF",direct.filename,direct.intensity,direct.mode?"BYPASS":"RANDOM",
             legacy.on?"ON":"OFF",legacy.filename,legacy.intensity,legacy.mode?"BYPASS":"RANDOM");

    httpd_resp_set_type(r,"application/json");
    no_cache(r);
    httpd_resp_set_hdr(r,"Connection","close");
    return httpd_resp_send(r,b,n);
}

static bool source_name_ok(const char *name)
{
    if(!name||!*name||strlen(name)>63)return false;
    for(const unsigned char *p=(const unsigned char*)name;*p;p++)
        if(!(isalnum(*p)||*p=='_'||*p=='-'||*p=='.'))return false;
    return strstr(name,"..") == NULL;
}

static esp_err_t source_get(httpd_req_t*r)
{
    const char *prefix="/source/";
    const char *name=r->uri+strlen(prefix);
    const char *q=strchr(name,'?');
    char clean[64];
    size_t n=q?(size_t)(q-name):strlen(name);
    if(n==0||n>=sizeof(clean)){httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid source");return ESP_FAIL;}
    memcpy(clean,name,n);clean[n]='\0';
    if(!source_name_ok(clean)){httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid source");return ESP_FAIL;}
    char path[80];
    int plen=snprintf(path,sizeof(path),"/storage/%s",clean);
    if(plen<0||plen>=(int)sizeof(path)){httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid source");return ESP_FAIL;}
    FILE *f=fopen(path,"rb");
    if(!f){httpd_resp_send_err(r,HTTPD_404_NOT_FOUND,"Source not found");return ESP_FAIL;}
    httpd_resp_set_type(r,"image/jpeg");
    no_cache(r);
    char buf[1024];size_t got;esp_err_t err=ESP_OK;
    while((got=fread(buf,1,sizeof(buf),f))>0){err=httpd_resp_send_chunk(r,buf,got);if(err!=ESP_OK)break;}
    fclose(f);
    if(err==ESP_OK)err=httpd_resp_send_chunk(r,NULL,0);
    return err;
}

static void start_portal(void)
{
    uint8_t mac[6];
    esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP);
    char ap[32];
    snprintf(ap,sizeof(ap),"PRISMA-PI-%02X%02X",mac[4],mac[5]);

    wifi_config_t cfg={0};
    strlcpy((char*)cfg.ap.ssid,ap,sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len=strlen(ap);
    cfg.ap.max_connection=4;
    cfg.ap.authmode=WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_config_t hc=HTTPD_DEFAULT_CONFIG();
    hc.uri_match_fn=httpd_uri_match_wildcard;
    hc.lru_purge_enable=true;
    httpd_handle_t s=NULL;
    if(httpd_start(&s,&hc)==ESP_OK){
        httpd_uri_t a={.uri="/",.method=HTTP_GET,.handler=page_get};
        httpd_uri_t b={.uri="/api/state",.method=HTTP_GET,.handler=state_get};
        httpd_uri_t c={.uri="/style.css",.method=HTTP_GET,.handler=css_get};
        httpd_uri_t d={.uri="/api/debug/state",.method=HTTP_GET,.handler=debug_state_get};
        httpd_uri_t e={.uri="/source/*",.method=HTTP_GET,.handler=source_get};
        httpd_register_uri_handler(s,&a);
        httpd_register_uri_handler(s,&b);
        httpd_register_uri_handler(s,&c);
        httpd_register_uri_handler(s,&d);
        httpd_register_uri_handler(s,&e);
    }
    ESP_LOGI(TAG,"PRISMA LITE M4.8 Sync Lab: %s - http://192.168.4.1",ap);
}

void prisma_wifi_start(void)
{
    esp_err_t r=nvs_flash_init();
    if(r==ESP_ERR_NVS_NO_FREE_PAGES||r==ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t i=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&i));
    start_portal();
}
