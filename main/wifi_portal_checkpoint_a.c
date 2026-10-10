#define state_get prisma_legacy_state_get
#define start_portal prisma_legacy_start_portal
#define prisma_wifi_start prisma_legacy_wifi_start
#include "wifi_portal.c"
#undef prisma_wifi_start
#undef start_portal
#undef state_get

bool prisma_appliance_get_motion_effect(int effect);
int prisma_appliance_get_motion_value(int effect);

static esp_err_t checkpoint_state_get(httpd_req_t*r)
{
    static bool have_last=false,last_on=false;
    static int last_mode=-1,last_intensity=-1,last_values[6]={-1,-1,-1,-1,-1,-1},last_motion_values[6]={-1,-1,-1,-1,-1,-1},generation=0;
    static char last_filename[32]="";
    static bool last_effects[6]={false,false,false,false,false,false},last_motion[6]={false,false,false,false,false,false};
    bool on=prisma_appliance_get_on();int mode=prisma_appliance_get_mode()?1:0;int intensity=prisma_appliance_get_intensity();const char *filename=prisma_appliance_get_filename();
    bool effects[6],motion[6];int values[6],motion_values[6];
    if(!filename||!*filename)filename="F1_MOON.JPG";
    for(int i=0;i<6;i++){effects[i]=prisma_appliance_get_effect(i);values[i]=prisma_appliance_get_effect_value(i);motion[i]=prisma_appliance_get_motion_effect(i);motion_values[i]=prisma_appliance_get_motion_value(i);}effects[5]=true;
    bool changed=!have_last||last_on!=on||last_mode!=mode||last_intensity!=intensity||strcmp(last_filename,filename)!=0;
    for(int i=0;i<6;i++)if(last_effects[i]!=effects[i]||last_values[i]!=values[i]||last_motion[i]!=motion[i]||last_motion_values[i]!=motion_values[i])changed=true;
    if(changed){generation++;if(generation<=0)generation=1;last_on=on;last_mode=mode;last_intensity=intensity;strlcpy(last_filename,filename,sizeof(last_filename));for(int i=0;i<6;i++){last_effects[i]=effects[i];last_values[i]=values[i];last_motion[i]=motion[i];last_motion_values[i]=motion_values[i];}have_last=true;ESP_LOGI(TAG,"WEB STATE gen=%d source=%s motion-sync",generation,filename);}
    char b[1024];
    int n=snprintf(b,sizeof(b),"{\"on\":%s,\"mode\":%d,\"intensity\":%d,\"generation\":%d,\"filename\":\"%s\",\"effects\":[%s,%s,%s,%s,%s,true],\"values\":[%d,%d,%d,%d,%d,%d],\"motionEffects\":[%s,%s,%s,%s,%s,%s],\"motionValues\":[%d,%d,%d,%d,%d,%d]}",on?"true":"false",mode,intensity,generation,filename,effects[0]?"true":"false",effects[1]?"true":"false",effects[2]?"true":"false",effects[3]?"true":"false",effects[4]?"true":"false",values[0],values[1],values[2],values[3],values[4],values[5],motion[0]?"true":"false",motion[1]?"true":"false",motion[2]?"true":"false",motion[3]?"true":"false",motion[4]?"true":"false",motion[5]?"true":"false",motion_values[0],motion_values[1],motion_values[2],motion_values[3],motion_values[4],motion_values[5]);
    if(n<0||n>=(int)sizeof(b)){httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"state overflow");return ESP_FAIL;}
    httpd_resp_set_type(r,"application/json");no_cache(r);return httpd_resp_send(r,b,n);
}

static void checkpoint_start_portal(void)
{
    uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP);char ap[32];snprintf(ap,sizeof(ap),"PRISMA-PI-%02X%02X",mac[4],mac[5]);
    wifi_config_t cfg={0};strlcpy((char*)cfg.ap.ssid,ap,sizeof(cfg.ap.ssid));cfg.ap.ssid_len=strlen(ap);cfg.ap.max_connection=4;cfg.ap.authmode=WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&cfg));ESP_ERROR_CHECK(esp_wifi_start());
    httpd_config_t hc=HTTPD_DEFAULT_CONFIG();hc.uri_match_fn=httpd_uri_match_wildcard;hc.lru_purge_enable=true;hc.max_uri_handlers=16;httpd_handle_t s=NULL;
    if(httpd_start(&s,&hc)==ESP_OK){httpd_uri_t a={.uri="/",.method=HTTP_GET,.handler=page_get};httpd_uri_t b={.uri="/api/state",.method=HTTP_GET,.handler=checkpoint_state_get};httpd_uri_t c={.uri="/style.css",.method=HTTP_GET,.handler=css_get};httpd_uri_t d={.uri="/source/*",.method=HTTP_GET,.handler=source_get};httpd_register_uri_handler(s,&a);httpd_register_uri_handler(s,&b);httpd_register_uri_handler(s,&c);httpd_register_uri_handler(s,&d);prisma_admin_register(s);}ESP_LOGI(TAG,"PRISMA LITE CHECKPOINT A: %s - http://192.168.4.1",ap);
}

void prisma_wifi_start(void)
{
    esp_err_t r=nvs_flash_init();if(r==ESP_ERR_NVS_NO_FREE_PAGES||r==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());ESP_ERROR_CHECK(nvs_flash_init());}
    ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_ap();wifi_init_config_t i=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&i));checkpoint_start_portal();
}
