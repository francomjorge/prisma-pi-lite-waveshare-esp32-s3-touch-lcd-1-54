#include "playback_http.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "security_service.h"

#define COOKIE_NAME "prisma_session"
#define MAX_NAME 63
#define IO_CHUNK 4096

static bool cookie_token(httpd_req_t *r, char out[PRISMA_SECURITY_SESSION_HEX + 1]) {
    out[0]=0; size_t len=httpd_req_get_hdr_value_len(r,"Cookie"); if(!len||len>512)return false;
    char b[513]; if(httpd_req_get_hdr_value_str(r,"Cookie",b,sizeof(b))!=ESP_OK)return false;
    const char *p=b; size_t k=strlen(COOKIE_NAME); while(*p){while(*p==' '||*p==';')p++; if(!strncmp(p,COOKIE_NAME,k)&&p[k]=='='){p+=k+1;size_t n=0;while(p[n]&&p[n]!=';'&&n<PRISMA_SECURITY_SESSION_HEX)n++;if(n!=PRISMA_SECURITY_SESSION_HEX)return false;memcpy(out,p,n);out[n]=0;return true;}const char*s=strchr(p,';');if(!s)break;p=s+1;} return false;
}
static bool authorized(httpd_req_t*r){prisma_security_status_t st;prisma_security_get_status(&st);if(!st.password_set)return true;char t[PRISMA_SECURITY_SESSION_HEX+1];return cookie_token(r,t)&&prisma_security_validate_session(t);}
static bool require_auth(httpd_req_t*r){if(authorized(r))return true;httpd_resp_set_status(r,"401 Unauthorized");httpd_resp_set_type(r,"application/json");httpd_resp_sendstr(r,"{\"error\":\"auth_required\"}");return false;}
static int hexval(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return-1;}
static void url_decode(char*s){char*r=s,*w=s;while(*r){if(*r=='%'&&r[1]&&r[2]){int a=hexval(r[1]),b=hexval(r[2]);if(a>=0&&b>=0){*w++=(char)((a<<4)|b);r+=3;continue;}}*w++=(*r=='+')?' ':*r++;}*w=0;}
static bool safe_name(char*d,size_t cap,const char*s){if(!s||!*s)return false;size_t j=0;for(size_t i=0;s[i]&&j+1<cap;i++){unsigned char c=s[i];if(isalnum(c)||c=='.'||c=='-'||c=='_')d[j++]=c;else if(c==' '||c=='+')d[j++]='_';else return false;}d[j]=0;return d[0]&&d[0]!='.'&&!strstr(d,"..");}
static const char*mime_for(const char*n){const char*d=strrchr(n,'.');if(!d)return"application/octet-stream";if(!strcasecmp(d,".wav"))return"audio/wav";if(!strcasecmp(d,".jpg")||!strcasecmp(d,".jpeg"))return"image/jpeg";if(!strcasecmp(d,".png"))return"image/png";if(!strcasecmp(d,".gif"))return"image/gif";if(!strcasecmp(d,".json"))return"application/json";if(!strcasecmp(d,".txt")||!strcasecmp(d,".log")||!strcasecmp(d,".csv"))return"text/plain";return"application/octet-stream";}
static bool u64(const char*s,uint64_t*v){if(!s||!*s)return false;uint64_t x=0;for(;*s;s++){if(*s<'0'||*s>'9')return false;uint64_t y=x*10+(*s-'0');if(y<x)return false;x=y;}*v=x;return true;}
static int get_range(httpd_req_t*r,size_t total,size_t*start,size_t*end){*start=0;*end=total?total-1:0;size_t n=httpd_req_get_hdr_value_len(r,"Range");if(!n)return 0;if(!total||n>=96)return-1;char h[96];if(httpd_req_get_hdr_value_str(r,"Range",h,sizeof(h))!=ESP_OK||strncmp(h,"bytes=",6)||strchr(h+6,','))return-1;char*s=h+6,*dash=strchr(s,'-');if(!dash)return-1;*dash=0;if(*s){uint64_t a=0,b=total-1;if(!u64(s,&a)||(dash[1]&&!u64(dash+1,&b))||a>=total||b<a)return-1;if(b>=total)b=total-1;*start=a;*end=b;return 1;}uint64_t suffix=0;if(!u64(dash+1,&suffix)||!suffix)return-1;*start=suffix>=total?0:total-suffix;*end=total-1;return 1;}
static esp_err_t send_all(httpd_req_t*r,const char*b,size_t n){while(n){int s=httpd_send(r,b,n);if(s<=0)return ESP_FAIL;b+=s;n-=s;}return ESP_OK;}

static esp_err_t serve(httpd_req_t *r, const char *prefix, bool attachment, bool head)
{
    if (!require_auth(r)) {
        return ESP_OK;
    }

    char dec[96];
    strlcpy(dec, r->uri + strlen(prefix), sizeof(dec));
    char *q = strchr(dec, '?');
    if (q) *q = 0;
    url_decode(dec);

    char name[MAX_NAME + 1];
    if (!safe_name(name, sizeof(name), dec)) {
        return httpd_resp_send_err(r, 400, "invalid name");
    }

    char path[128];
    snprintf(path, sizeof(path), "/storage/%s", name);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return httpd_resp_send_err(r, 404, "not found");
    }

    size_t total = (size_t)st.st_size;
    size_t start = 0, end = 0;
    int rg = get_range(r, total, &start, &end);
    if (rg < 0) {
        char h[256];
        int n = snprintf(h, sizeof(h), "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Length: 0\r\nAccept-Ranges: bytes\r\nContent-Range: bytes */%u\r\n\r\n", (unsigned)total);
        return send_all(r, h, (size_t)n);
    }

    size_t len = total ? end - start + 1 : 0;
    char rh[96] = "";
    if (rg == 1) {
        snprintf(rh, sizeof(rh), "Content-Range: bytes %u-%u/%u\r\n", (unsigned)start, (unsigned)end, (unsigned)total);
    }

    char h[768];
    int hn = snprintf(h, sizeof(h),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nAccept-Ranges: bytes\r\n%sContent-Disposition: %s; filename=\"%s\"\r\nContent-Transfer-Encoding: binary\r\nCache-Control: no-store, no-cache, must-revalidate, max-age=0\r\nPragma: no-cache\r\n\r\n",
        rg == 1 ? "206 Partial Content" : "200 OK", mime_for(name), (unsigned)len, rh,
        attachment ? "attachment" : "inline", name);
    if (hn <= 0 || hn >= (int)sizeof(h)) return ESP_FAIL;
    if (send_all(r, h, (size_t)hn) != ESP_OK) return ESP_FAIL;
    if (head || !len) return ESP_OK;

    FILE *f = fopen(path, "rb");
    if (!f) return ESP_FAIL;
    if (start && fseek(f, (long)start, SEEK_SET) != 0) {
        fclose(f);
        return ESP_FAIL;
    }

    char b[IO_CHUNK];
    size_t left = len;
    esp_err_t e = ESP_OK;
    while (left) {
        size_t want = left < sizeof(b) ? left : sizeof(b);
        size_t got = fread(b, 1, want, f);
        if (!got || send_all(r, b, got) != ESP_OK) {
            e = ESP_FAIL;
            break;
        }
        left -= got;
    }
    fclose(f);
    return e;
}

static esp_err_t media_get(httpd_req_t*r){return serve(r,"/media/",false,false);} static esp_err_t media_head(httpd_req_t*r){return serve(r,"/media/",false,true);} static esp_err_t files_get(httpd_req_t*r){return serve(r,"/files/",true,false);} static esp_err_t files_head(httpd_req_t*r){return serve(r,"/files/",true,true);}
void prisma_playback_http_register(httpd_handle_t s){if(!s)return;httpd_uri_t a={.uri="/media/*",.method=HTTP_GET,.handler=media_get},b={.uri="/media/*",.method=HTTP_HEAD,.handler=media_head},c={.uri="/files/*",.method=HTTP_GET,.handler=files_get},d={.uri="/files/*",.method=HTTP_HEAD,.handler=files_head};httpd_register_uri_handler(s,&a);httpd_register_uri_handler(s,&b);httpd_register_uri_handler(s,&c);httpd_register_uri_handler(s,&d);}
