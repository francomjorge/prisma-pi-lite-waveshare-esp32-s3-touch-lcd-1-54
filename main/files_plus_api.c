#include "files_plus_api.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "nvs.h"
#include "security_service.h"
#include "storage.h"

#define COOKIE_NAME "prisma_session"
#define MAX_NAME 63
#define IO_CHUNK 2048
#define CFG_NS "media_cfg"
#define CFG_SAMPLE "sample"

typedef struct {
    char extension[12];
    const char *mime;
    const char *category;
    bool preview;
    int width;
    int height;
    int channels;
    int sample_rate;
    int bits;
    int duration_ms;
} file_meta_t;

static void no_cache(httpd_req_t *r)
{
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(r, "Pragma", "no-cache");
}

static bool cookie_token(httpd_req_t *r, char out[PRISMA_SECURITY_SESSION_HEX + 1])
{
    out[0] = 0;
    size_t len = httpd_req_get_hdr_value_len(r, "Cookie");
    if (!len || len > 512) return false;
    char buf[513];
    if (httpd_req_get_hdr_value_str(r, "Cookie", buf, sizeof(buf)) != ESP_OK) return false;
    const char *p = buf;
    size_t kl = strlen(COOKIE_NAME);
    while (*p) {
        while (*p == ' ' || *p == ';') p++;
        if (strncmp(p, COOKIE_NAME, kl) == 0 && p[kl] == '=') {
            p += kl + 1;
            size_t n = 0;
            while (p[n] && p[n] != ';' && n < PRISMA_SECURITY_SESSION_HEX) n++;
            if (n != PRISMA_SECURITY_SESSION_HEX) return false;
            memcpy(out, p, n); out[n] = 0; return true;
        }
        const char *semi = strchr(p, ';');
        if (!semi) break;
        p = semi + 1;
    }
    return false;
}

static bool authorized(httpd_req_t *r)
{
    prisma_security_status_t st;
    prisma_security_get_status(&st);
    if (!st.password_set) return true;
    char token[PRISMA_SECURITY_SESSION_HEX + 1];
    return cookie_token(r, token) && prisma_security_validate_session(token);
}

static esp_err_t require_auth(httpd_req_t *r)
{
    if (authorized(r)) return ESP_OK;
    httpd_resp_set_status(r, "401 Unauthorized");
    httpd_resp_set_type(r, "application/json");
    return httpd_resp_sendstr(r, "{\"error\":\"auth_required\"}");
}

static void json_escape(const char *src, char *dst, size_t cap)
{
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 1 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if ((c == '\\' || c == '"') && j + 2 < cap) { dst[j++]='\\'; dst[j++]=(char)c; }
        else if (c >= 0x20) dst[j++]=(char)c;
    }
    dst[j]=0;
}

static int hexval(char c)
{
    if (c>='0'&&c<='9') return c-'0';
    if (c>='a'&&c<='f') return c-'a'+10;
    if (c>='A'&&c<='F') return c-'A'+10;
    return -1;
}

static void url_decode(char *s)
{
    char *r=s,*w=s;
    while (*r) {
        if (*r=='%'&&r[1]&&r[2]) {
            int a=hexval(r[1]),b=hexval(r[2]);
            if (a>=0&&b>=0) { *w++=(char)((a<<4)|b); r+=3; continue; }
        }
        *w++=(*r=='+')?' ':*r++;
    }
    *w=0;
}

static bool safe_name(char *dst, size_t cap, const char *src)
{
    if (!src || !*src) return false;
    size_t j=0;
    for (size_t i=0; src[i] && j+1<cap; i++) {
        unsigned char c=(unsigned char)src[i];
        if (isalnum(c)||c=='.'||c=='-'||c=='_') dst[j++]=(char)c;
        else if (c==' '||c=='+') dst[j++]='_';
        else return false;
    }
    dst[j]=0;
    return dst[0] && dst[0]!='.' && strstr(dst,"..") == NULL;
}

static const char *extension_of(const char *name)
{
    const char *dot=strrchr(name,'.');
    return (dot && dot[1]) ? dot+1 : "";
}

static const char *mime_for(const char *name)
{
    const char *e=extension_of(name);
    if (!strcasecmp(e,"wav")) return "audio/wav";
    if (!strcasecmp(e,"jpg")||!strcasecmp(e,"jpeg")) return "image/jpeg";
    if (!strcasecmp(e,"png")) return "image/png";
    if (!strcasecmp(e,"gif")) return "image/gif";
    if (!strcasecmp(e,"json")) return "application/json";
    if (!strcasecmp(e,"txt")||!strcasecmp(e,"log")||!strcasecmp(e,"csv")) return "text/plain";
    return "application/octet-stream";
}

static uint16_t le16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static uint16_t be16(const uint8_t *p) { return ((uint16_t)p[0]<<8) | p[1]; }
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | p[3]; }

static void parse_png(FILE *f, file_meta_t *m)
{
    uint8_t h[24];
    rewind(f);
    if (fread(h,1,sizeof(h),f)==sizeof(h) && !memcmp(h,"\x89PNG\r\n\x1a\n",8)) {
        m->width=(int)be32(h+16); m->height=(int)be32(h+20);
    }
}

static void parse_jpeg(FILE *f, file_meta_t *m)
{
    uint8_t b[4]; rewind(f);
    if (fread(b,1,2,f)!=2 || b[0]!=0xff || b[1]!=0xd8) return;
    while (fread(b,1,2,f)==2) {
        if (b[0]!=0xff) continue;
        uint8_t marker=b[1];
        if (marker==0xd8 || marker==0xd9) continue;
        uint8_t l[2]; if (fread(l,1,2,f)!=2) break;
        uint16_t len=be16(l); if (len<2) break;
        if ((marker>=0xc0&&marker<=0xc3)||(marker>=0xc5&&marker<=0xc7)||(marker>=0xc9&&marker<=0xcb)||(marker>=0xcd&&marker<=0xcf)) {
            uint8_t d[5]; if (fread(d,1,5,f)==5) { m->height=be16(d+1); m->width=be16(d+3); }
            break;
        }
        if (fseek(f,len-2,SEEK_CUR)!=0) break;
    }
}

static void parse_wav(FILE *f, file_meta_t *m)
{
    uint8_t h[12]; rewind(f);
    if (fread(h,1,12,f)!=12 || memcmp(h,"RIFF",4) || memcmp(h+8,"WAVE",4)) return;
    uint32_t byte_rate=0, data_bytes=0;
    while (1) {
        uint8_t ch[8]; if (fread(ch,1,8,f)!=8) break;
        uint32_t n=le32(ch+4);
        if (!memcmp(ch,"fmt ",4)) {
            uint8_t fmt[32]={0}; size_t take=n<sizeof(fmt)?n:sizeof(fmt);
            if (fread(fmt,1,take,f)!=take) break;
            if (take>=16) { m->channels=le16(fmt+2); m->sample_rate=(int)le32(fmt+4); byte_rate=le32(fmt+8); m->bits=le16(fmt+14); }
            if (n>take) fseek(f,n-take,SEEK_CUR);
        } else if (!memcmp(ch,"data",4)) {
            data_bytes=n; fseek(f,n,SEEK_CUR);
        } else fseek(f,n,SEEK_CUR);
        if (n&1) fseek(f,1,SEEK_CUR);
    }
    if (byte_rate) m->duration_ms=(int)(((uint64_t)data_bytes*1000ULL)/byte_rate);
}

static void meta_for(const char *path, const char *name, file_meta_t *m)
{
    memset(m,0,sizeof(*m));
    const char *e=extension_of(name);
    snprintf(m->extension,sizeof(m->extension),"%s",e[0]?e:"-");
    for (char *p=m->extension; *p; p++) *p=(char)toupper((unsigned char)*p);
    m->mime=mime_for(name);
    if (!strncasecmp(m->mime,"image/",6)) { m->category="IMAGE"; m->preview=true; }
    else if (!strcasecmp(m->mime,"audio/wav")) { m->category="AUDIO"; m->preview=true; }
    else if (!strncasecmp(m->mime,"text/",5)||!strcasecmp(m->mime,"application/json")) { m->category="TEXT"; m->preview=false; }
    else { m->category="DATA"; m->preview=false; }
    FILE *f=fopen(path,"rb"); if (!f) return;
    if (!strcasecmp(e,"png")) parse_png(f,m);
    else if (!strcasecmp(e,"jpg")||!strcasecmp(e,"jpeg")) parse_jpeg(f,m);
    else if (!strcasecmp(e,"wav")) parse_wav(f,m);
    fclose(f);
}

static void load_sample(char out[MAX_NAME+1])
{
    out[0]=0; nvs_handle_t h;
    if (nvs_open(CFG_NS,NVS_READONLY,&h)!=ESP_OK) return;
    size_t n=MAX_NAME+1;
    if (nvs_get_str(h,CFG_SAMPLE,out,&n)!=ESP_OK) out[0]=0;
    nvs_close(h);
    if (out[0]) {
        char path[96]; snprintf(path,sizeof(path),"/storage/%s",out);
        struct stat st;
        if (stat(path,&st)!=0 || !S_ISREG(st.st_mode)) {
            nvs_handle_t w; if (nvs_open(CFG_NS,NVS_READWRITE,&w)==ESP_OK) { nvs_erase_key(w,CFG_SAMPLE); nvs_commit(w); nvs_close(w); }
            out[0]=0;
        }
    }
}

static esp_err_t save_sample(const char *name)
{
    nvs_handle_t h; esp_err_t e=nvs_open(CFG_NS,NVS_READWRITE,&h); if (e!=ESP_OK) return e;
    if (name && name[0]) e=nvs_set_str(h,CFG_SAMPLE,name); else e=nvs_erase_key(h,CFG_SAMPLE);
    if (e==ESP_OK || e==ESP_ERR_NVS_NOT_FOUND) e=nvs_commit(h);
    nvs_close(h); return e==ESP_ERR_NVS_NOT_FOUND?ESP_OK:e;
}

static esp_err_t list_get(httpd_req_t *r)
{
    if (require_auth(r)!=ESP_OK) return ESP_OK;
    size_t total=0,used=0;
    if (!prisma_storage_info(&total,&used)) return httpd_resp_send_err(r,500,"storage unavailable");
    char selected[MAX_NAME+1]; load_sample(selected);
    char selesc[140]; json_escape(selected,selesc,sizeof(selesc));
    httpd_resp_set_type(r,"application/json"); no_cache(r);
    char head[320];
    snprintf(head,sizeof(head),"{\"total\":%u,\"used\":%u,\"free\":%u,\"max_files\":%d,\"selected_sample\":\"%s\",\"files\":[",
             (unsigned)total,(unsigned)used,(unsigned)(total>used?total-used:0),prisma_storage_max_files(),selesc);
    if (httpd_resp_send_chunk(r,head,HTTPD_RESP_USE_STRLEN)!=ESP_OK) return ESP_FAIL;
    DIR *dir=opendir("/storage"); bool first=true;
    if (dir) {
        struct dirent *e;
        while ((e=readdir(dir))) {
            if (!e->d_name[0] || e->d_name[0]=='.' || !strcmp(e->d_name,"upload.tmp")) continue;
            char path[128]; if (snprintf(path,sizeof(path),"/storage/%s",e->d_name)>=(int)sizeof(path)) continue;
            struct stat st; if (stat(path,&st)!=0 || !S_ISREG(st.st_mode)) continue;
            file_meta_t m; meta_for(path,e->d_name,&m);
            char name[140]; json_escape(e->d_name,name,sizeof(name));
            char item[640];
            snprintf(item,sizeof(item),"%s{\"name\":\"%s\",\"size\":%u,\"extension\":\"%s\",\"mime\":\"%s\",\"category\":\"%s\",\"preview\":%s,\"selected_sample\":%s,\"width\":%d,\"height\":%d,\"channels\":%d,\"sample_rate\":%d,\"bits\":%d,\"duration_ms\":%d}",
                     first?"":",",name,(unsigned)st.st_size,m.extension,m.mime,m.category,m.preview?"true":"false",!strcmp(selected,e->d_name)?"true":"false",m.width,m.height,m.channels,m.sample_rate,m.bits,m.duration_ms);
            first=false;
            if (httpd_resp_send_chunk(r,item,HTTPD_RESP_USE_STRLEN)!=ESP_OK) { closedir(dir); return ESP_FAIL; }
        }
        closedir(dir);
    }
    if (httpd_resp_send_chunk(r,"]}",2)!=ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(r,NULL,0);
}

static esp_err_t media_get(httpd_req_t *r)
{
    if (require_auth(r)!=ESP_OK) return ESP_OK;
    const char *raw=r->uri+strlen("/media/");
    char tmp[96]; strlcpy(tmp,raw,sizeof(tmp)); char *q=strchr(tmp,'?'); if(q)*q=0; url_decode(tmp);
    char name[MAX_NAME+1]; if(!safe_name(name,sizeof(name),tmp)) return httpd_resp_send_err(r,400,"invalid name");
    char path[128]; snprintf(path,sizeof(path),"/storage/%s",name);
    FILE *f=fopen(path,"rb"); if(!f) return httpd_resp_send_err(r,404,"not found");
    httpd_resp_set_type(r,mime_for(name)); no_cache(r); httpd_resp_set_hdr(r,"Content-Disposition","inline");
    char buf[IO_CHUNK]; size_t n; esp_err_t err=ESP_OK;
    while ((n=fread(buf,1,sizeof(buf),f))>0) if (httpd_resp_send_chunk(r,buf,n)!=ESP_OK) { err=ESP_FAIL; break; }
    fclose(f); if(err!=ESP_OK)return err; return httpd_resp_send_chunk(r,NULL,0);
}

static bool form_name(httpd_req_t *r, char out[MAX_NAME+1])
{
    char body[160]; if (r->content_len<=0 || r->content_len>=(int)sizeof(body)) return false;
    int left=r->content_len,pos=0;
    while(left>0){int n=httpd_req_recv(r,body+pos,left); if(n==HTTPD_SOCK_ERR_TIMEOUT)continue; if(n<=0)return false;pos+=n;left-=n;} body[pos]=0;
    char raw[96]; if(httpd_query_key_value(body,"name",raw,sizeof(raw))!=ESP_OK)return false; url_decode(raw);
    if (!raw[0]) { out[0]=0; return true; }
    return safe_name(out,MAX_NAME+1,raw);
}

static esp_err_t sample_post(httpd_req_t *r)
{
    if (require_auth(r)!=ESP_OK) return ESP_OK;
    char name[MAX_NAME+1]; if(!form_name(r,name)) return httpd_resp_send_err(r,400,"name required");
    if (name[0]) {
        if (strcasecmp(extension_of(name),"wav")) return httpd_resp_send_err(r,400,"selected sample must be WAV");
        char path[128]; snprintf(path,sizeof(path),"/storage/%s",name); struct stat st;
        if (stat(path,&st)!=0 || !S_ISREG(st.st_mode)) return httpd_resp_send_err(r,404,"file not found");
    }
    if (save_sample(name)!=ESP_OK) return httpd_resp_send_err(r,500,"cannot save selection");
    httpd_resp_set_type(r,"application/json"); no_cache(r);
    return httpd_resp_sendstr(r,name[0]?"{\"ok\":true,\"selected\":true}":"{\"ok\":true,\"selected\":false}");
}

void prisma_files_plus_register(httpd_handle_t server)
{
    if (!server) return;
    httpd_uri_t list={.uri="/api/filesplus",.method=HTTP_GET,.handler=list_get};
    httpd_uri_t media={.uri="/media/*",.method=HTTP_GET,.handler=media_get};
    httpd_uri_t sample={.uri="/api/files/sample",.method=HTTP_POST,.handler=sample_post};
    httpd_register_uri_handler(server,&list);
    httpd_register_uri_handler(server,&media);
    httpd_register_uri_handler(server,&sample);
}
