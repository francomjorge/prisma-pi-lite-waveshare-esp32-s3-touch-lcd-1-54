#include "xy_http.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "xy_control.h"

const char prisma_xy_web_injection[] =
"<style>"
".xy-box{border:1px solid #1d2d3a;border-radius:10px;padding:10px;background:#071018}.xy-title{font:700 11px monospace;letter-spacing:2px;color:#28dddd;margin-bottom:8px}.xy-pad{position:relative;aspect-ratio:1/1;border:1px solid #314858;border-radius:8px;background:#03070b;touch-action:none;user-select:none;overflow:hidden}.xy-pad:before,.xy-pad:after{content:'';position:absolute;background:#19303d;pointer-events:none}.xy-pad:before{left:50%;top:0;bottom:0;width:1px}.xy-pad:after{top:50%;left:0;right:0;height:1px}.xy-dot{position:absolute;width:14px;height:14px;border-radius:50%;background:#28dddd;border:2px solid #eef7ff;transform:translate(-50%,-50%);box-shadow:0 0 10px #28dddd99;pointer-events:none}.xy-note{font:10px monospace;color:#6f8798;margin-top:7px}.xy-visual-row{display:grid;grid-template-columns:minmax(0,1fr) 250px;gap:14px;align-items:start}.xy-visual-row>.visual{min-width:0}.xy-fractal-box{width:250px}.xy-audio-row{display:grid;grid-template-columns:minmax(0,1fr) 230px;gap:18px;align-items:start}.xy-audio-main{min-width:0}.xy-audio-box{width:230px}@media(max-width:760px){.xy-visual-row,.xy-audio-row{grid-template-columns:1fr}.xy-fractal-box,.xy-audio-box{width:100%;max-width:320px}.xy-pad{max-height:320px}}"
"</style><script>(function(){"
"if(document.documentElement.classList.contains('detached-mode'))return;"
"let cards=document.querySelectorAll('section.card');if(cards.length<2)return;"
"function box(kind,title,note,cls){let b=document.createElement('div');b.className='xy-box '+cls;b.innerHTML='<div class=\"xy-title\">'+title+'</div><div class=\"xy-pad\" id=\"xy-'+kind+'\"><i class=\"xy-dot\"></i></div><div class=\"xy-note\">'+note+'</div>';return b;}"
"let visual=cards[0].querySelector('.visual'),vbody=cards[0].querySelector('.fold-body')||cards[0];if(visual){let row=document.createElement('div');row.className='xy-visual-row';visual.parentNode.insertBefore(row,visual);row.appendChild(visual);row.appendChild(box('fractal','FRACTAL X/Y','SHARED WITH APPLIANCE','xy-fractal-box'));}"
"let abody=cards[1].querySelector('.fold-body')||cards[1];if(abody){let row=document.createElement('div');row.className='xy-audio-row';let main=document.createElement('div');main.className='xy-audio-main';while(abody.firstChild)main.appendChild(abody.firstChild);row.appendChild(main);row.appendChild(box('audio','AUDIO X/Y','MAPPING RESERVED','xy-audio-box'));abody.appendChild(row);}"
"let drag={fractal:false,audio:false},pending={fractal:null,audio:null},timer={fractal:0,audio:0},rev={fractal:0,audio:0};"
"function paint(kind,x,y){let p=document.getElementById('xy-'+kind),d=p&&p.querySelector('.xy-dot');if(!d)return;d.style.left=(Math.max(0,Math.min(1,x))*100)+'%';d.style.top=(Math.max(0,Math.min(1,y))*100)+'%';}"
"function flush(kind){timer[kind]=0;let v=pending[kind];if(!v)return;pending[kind]=null;fetch('/api/xy/'+kind+'?x='+v.x.toFixed(5)+'&y='+v.y.toFixed(5),{method:'POST',cache:'no-store'}).catch(function(){});}"
"function queue(kind,x,y){paint(kind,x,y);pending[kind]={x:x,y:y};if(!timer[kind])timer[kind]=setTimeout(function(){flush(kind)},35);}"
"function pos(e,p){let r=p.getBoundingClientRect();return{x:Math.max(0,Math.min(1,(e.clientX-r.left)/r.width)),y:Math.max(0,Math.min(1,(e.clientY-r.top)/r.height))};}"
"['fractal','audio'].forEach(function(kind){let p=document.getElementById('xy-'+kind);if(!p)return;p.addEventListener('pointerdown',function(e){drag[kind]=true;p.setPointerCapture(e.pointerId);let q=pos(e,p);queue(kind,q.x,q.y)});p.addEventListener('pointermove',function(e){if(!drag[kind])return;let q=pos(e,p);queue(kind,q.x,q.y)});function up(){drag[kind]=false}p.addEventListener('pointerup',up);p.addEventListener('pointercancel',up);});"
"async function sync(kind){try{let r=await fetch('/api/xy/'+kind,{cache:'no-store'});if(!r.ok)return;let d=await r.json();if(!drag[kind]&&d.rev!==rev[kind]){rev[kind]=d.rev;paint(kind,d.x,d.y)}}catch(e){}}"
"sync('fractal');sync('audio');setInterval(function(){sync('fractal');sync('audio')},50);"
"})();</script>";

static bool query_float(httpd_req_t *r, const char *key, float *out) {
    char q[192], v[32];
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) != ESP_OK) return false;
    if (httpd_query_key_value(q, key, v, sizeof(v)) != ESP_OK) return false;
    char *end = NULL;
    float f = strtof(v, &end);
    if (end == v) return false;
    *out = f;
    return true;
}

static esp_err_t xy_send(httpd_req_t *r, prisma_xy_channel_t channel) {
    prisma_xy_state_t s;
    prisma_xy_get(channel, &s);
    char b[128];
    snprintf(b, sizeof(b), "{\"x\":%.5f,\"y\":%.5f,\"rev\":%u}", s.x, s.y, (unsigned)s.rev);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_sendstr(r, b);
}

static esp_err_t xy_post_common(httpd_req_t *r, prisma_xy_channel_t channel) {
    prisma_xy_state_t s;
    prisma_xy_get(channel, &s);
    float x = s.x, y = s.y;
    bool hx = query_float(r, "x", &x), hy = query_float(r, "y", &y);
    if (hx || hy) prisma_xy_set(channel, x, y);
    return xy_send(r, channel);
}

static esp_err_t fractal_get(httpd_req_t *r) { return xy_send(r, PRISMA_XY_FRACTAL); }
static esp_err_t fractal_post(httpd_req_t *r) { return xy_post_common(r, PRISMA_XY_FRACTAL); }
static esp_err_t audio_get(httpd_req_t *r) { return xy_send(r, PRISMA_XY_AUDIO); }
static esp_err_t audio_post(httpd_req_t *r) { return xy_post_common(r, PRISMA_XY_AUDIO); }

void prisma_xy_http_register(httpd_handle_t server) {
    if (!server) return;
    httpd_uri_t fg = {.uri="/api/xy/fractal", .method=HTTP_GET, .handler=fractal_get};
    httpd_uri_t fp = {.uri="/api/xy/fractal", .method=HTTP_POST, .handler=fractal_post};
    httpd_uri_t ag = {.uri="/api/xy/audio", .method=HTTP_GET, .handler=audio_get};
    httpd_uri_t ap = {.uri="/api/xy/audio", .method=HTTP_POST, .handler=audio_post};
    httpd_register_uri_handler(server, &fg);
    httpd_register_uri_handler(server, &fp);
    httpd_register_uri_handler(server, &ag);
    httpd_register_uri_handler(server, &ap);
}
