#include "audio_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_service.h"
#include "security_service.h"

#define COOKIE_NAME "prisma_session"

static void no_cache(httpd_req_t *r)
{
    httpd_resp_set_hdr(r,"Cache-Control","no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(r,"Pragma","no-cache");
}

static bool cookie_token(httpd_req_t *r,char out[PRISMA_SECURITY_SESSION_HEX+1])
{
    out[0]=0;
    size_t len=httpd_req_get_hdr_value_len(r,"Cookie");
    if(!len||len>512)return false;
    char buf[513];
    if(httpd_req_get_hdr_value_str(r,"Cookie",buf,sizeof(buf))!=ESP_OK)return false;
    const char *p=buf; size_t k=strlen(COOKIE_NAME);
    while(*p){
        while(*p==' '||*p==';')p++;
        if(!strncmp(p,COOKIE_NAME,k)&&p[k]=='='){
            p+=k+1; size_t n=0;
            while(p[n]&&p[n]!=';'&&n<PRISMA_SECURITY_SESSION_HEX)n++;
            if(n!=PRISMA_SECURITY_SESSION_HEX)return false;
            memcpy(out,p,n);out[n]=0;return true;
        }
        const char *s=strchr(p,';');if(!s)break;p=s+1;
    }
    return false;
}

static bool authorized(httpd_req_t *r)
{
    prisma_security_status_t st; prisma_security_get_status(&st);
    if(!st.password_set)return true;
    char token[PRISMA_SECURITY_SESSION_HEX+1];
    return cookie_token(r,token)&&prisma_security_validate_session(token);
}

static esp_err_t require_auth(httpd_req_t *r)
{
    if(authorized(r))return ESP_OK;
    httpd_resp_set_status(r,"401 Unauthorized");
    httpd_resp_set_type(r,"application/json");
    return httpd_resp_sendstr(r,"{\"error\":\"auth_required\"}");
}

static void json_escape(const char *src,char *dst,size_t cap)
{
    size_t j=0;
    for(size_t i=0;src&&src[i]&&j+1<cap;i++){
        unsigned char c=(unsigned char)src[i];
        if((c=='\\'||c=='\"')&&j+2<cap){dst[j++]='\\';dst[j++]=(char)c;}
        else if(c>=0x20)dst[j++]=(char)c;
    }
    dst[j]=0;
}

static esp_err_t status_get(httpd_req_t *r)
{
    if(require_auth(r)!=ESP_OK)return ESP_OK;
    prisma_audio_status_t s; prisma_audio_get_status(&s);
    char selected[140],playing[140],err[220];
    json_escape(s.selected_sample,selected,sizeof(selected));
    json_escape(s.playing_name,playing,sizeof(playing));
    json_escape(s.last_error,err,sizeof(err));
    char b[900];
    snprintf(b,sizeof(b),
        "{\"initialized\":%s,\"es8311\":%s,\"es7210\":%s,\"playing\":%s,\"tone\":%s,"
        "\"volume\":%d,\"saved_volume\":%d,\"selected_sample\":\"%s\",\"playing_name\":\"%s\","
        "\"source_rate\":%d,\"source_channels\":%d,\"source_bits\":%d,"
        "\"position_ms\":%d,\"duration_ms\":%d,\"last_error\":\"%s\","
        "\"output_rate\":16000,\"output_bits\":16,\"output_channels\":1}",
        s.initialized?"true":"false",s.es8311_present?"true":"false",s.es7210_present?"true":"false",
        s.playing?"true":"false",s.tone?"true":"false",s.volume,s.saved_volume,selected,playing,
        s.source_rate,s.source_channels,s.source_bits,s.position_ms,s.duration_ms,err);
    httpd_resp_set_type(r,"application/json");no_cache(r);return httpd_resp_sendstr(r,b);
}

static bool volume_form(httpd_req_t *r,char *volume,size_t volume_cap,char *save,size_t save_cap)
{
    if(r->content_len<=0||r->content_len>255)return false;
    char body[256];int left=r->content_len,pos=0;
    while(left>0){int n=httpd_req_recv(r,body+pos,left);if(n==HTTPD_SOCK_ERR_TIMEOUT)continue;if(n<=0)return false;pos+=n;left-=n;}body[pos]=0;
    if(httpd_query_key_value(body,"volume",volume,volume_cap)!=ESP_OK)return false;
    if(httpd_query_key_value(body,"save",save,save_cap)!=ESP_OK)strlcpy(save,"1",save_cap);
    return true;
}

static esp_err_t volume_post(httpd_req_t *r)
{
    if(require_auth(r)!=ESP_OK)return ESP_OK;
    char v[16],save[8];
    if(!volume_form(r,v,sizeof(v),save,sizeof(save)))return httpd_resp_send_err(r,400,"volume required");
    char *end=NULL;
    long parsed=strtol(v,&end,10);
    if(end==v||*end||parsed<0||parsed>100)return httpd_resp_send_err(r,400,"volume must be 0-100");
    if(strcmp(save,"0")&&strcmp(save,"1"))return httpd_resp_send_err(r,400,"save must be 0 or 1");
    int volume=(int)parsed;
    bool persist=strcmp(save,"0")!=0;
    esp_err_t e=persist?prisma_audio_set_volume(volume):prisma_audio_preview_volume(volume);
    if(e!=ESP_OK)return httpd_resp_send_err(r,400,"volume must be 0-100 and audio must be ready");
    char response[96];
    snprintf(response,sizeof(response),"{\"ok\":true,\"volume\":%d,\"saved\":%s}",volume,persist?"true":"false");
    httpd_resp_set_type(r,"application/json");no_cache(r);return httpd_resp_sendstr(r,response);
}

static esp_err_t tone_post(httpd_req_t *r)
{
    if(require_auth(r)!=ESP_OK)return ESP_OK;
    esp_err_t e=prisma_audio_play_tone();
    if(e!=ESP_OK)return httpd_resp_send_err(r,409,"audio busy or unavailable");
    httpd_resp_set_type(r,"application/json");no_cache(r);return httpd_resp_sendstr(r,"{\"ok\":true,\"mode\":\"tone\"}");
}

static esp_err_t play_post(httpd_req_t *r)
{
    if(require_auth(r)!=ESP_OK)return ESP_OK;
    esp_err_t e=prisma_audio_play_selected();
    if(e==ESP_ERR_NOT_FOUND)return httpd_resp_send_err(r,404,"no WAV selected in FILES+");
    if(e!=ESP_OK)return httpd_resp_send_err(r,409,"audio busy or unavailable");
    httpd_resp_set_type(r,"application/json");no_cache(r);return httpd_resp_sendstr(r,"{\"ok\":true,\"mode\":\"sample\"}");
}

static esp_err_t stop_post(httpd_req_t *r)
{
    if(require_auth(r)!=ESP_OK)return ESP_OK;
    prisma_audio_stop();
    httpd_resp_set_type(r,"application/json");no_cache(r);return httpd_resp_sendstr(r,"{\"ok\":true}");
}

void prisma_audio_api_register(httpd_handle_t server)
{
    if(!server)return;
    httpd_uri_t s={.uri="/api/audio/status",.method=HTTP_GET,.handler=status_get};
    httpd_uri_t v={.uri="/api/audio/volume",.method=HTTP_POST,.handler=volume_post};
    httpd_uri_t t={.uri="/api/audio/tone",.method=HTTP_POST,.handler=tone_post};
    httpd_uri_t p={.uri="/api/audio/play",.method=HTTP_POST,.handler=play_post};
    httpd_uri_t x={.uri="/api/audio/stop",.method=HTTP_POST,.handler=stop_post};
    httpd_register_uri_handler(server,&s);
    httpd_register_uri_handler(server,&v);
    httpd_register_uri_handler(server,&t);
    httpd_register_uri_handler(server,&p);
    httpd_register_uri_handler(server,&x);
}
