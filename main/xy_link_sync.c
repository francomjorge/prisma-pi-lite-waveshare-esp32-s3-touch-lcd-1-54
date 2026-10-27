#include "xy_link_sync.h"
#include <stdio.h>
#include <stdlib.h>
#include "xy_link_state.h"

const char prisma_xy_link_sync_injection[] =
"<script>(function(){"
"if(document.documentElement.classList.contains('detached-mode'))return;"
"let applying=false,lastRev=-1;"
"function kindOf(el){let c=el&&el.closest('.xy-link-controls');return c&&c.dataset.kind==='audio'?'audio':'fractal';}"
"function sourceNum(k){return k==='audio'?1:0;}"
"function postLock(el){if(applying)return;let k=kindOf(el);fetch('/api/xy/link?locked='+(el.checked?1:0)+'&source='+sourceNum(k),{method:'POST',cache:'no-store'}).catch(function(){});}"
"function postRandom(el){if(applying)return;fetch('/api/xy/link?randomized='+(el.checked?1:0),{method:'POST',cache:'no-store'}).catch(function(){});}"
"function bind(){document.querySelectorAll('.xy-lock').forEach(function(e){if(e.dataset.linkSync)return;e.dataset.linkSync='1';e.addEventListener('change',function(){postLock(e)});});document.querySelectorAll('.xy-random').forEach(function(e){if(e.dataset.linkSync)return;e.dataset.linkSync='1';e.addEventListener('change',function(){postRandom(e)});});}"
"function fire(el,value){if(!el||el.checked===value)return;applying=true;el.checked=value;el.dispatchEvent(new Event('change',{bubbles:true}));applying=false;}"
"function apply(d){bind();let src=d.source==='audio'?'audio':'fractal';let primary=document.querySelector('.xy-link-controls[data-kind=\"'+src+'\"] .xy-lock')||document.querySelector('.xy-lock');let lockNow=primary&&primary.checked;if(!!d.locked!==!!lockNow)fire(primary,!!d.locked);let r=document.querySelector('.xy-random');if(r&&!!d.randomized!==!!r.checked)fire(r,!!d.randomized);}"
"async function poll(){try{let r=await fetch('/api/xy/link',{cache:'no-store'});if(!r.ok)return;let d=await r.json();if(d.rev!==lastRev){lastRev=d.rev;apply(d)}}catch(e){}}"
"bind();poll();setInterval(poll,80);"
"})();</script>";

static bool query_int(httpd_req_t *r, const char *key, int *out) {
    char q[128], v[16];
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) != ESP_OK) return false;
    if (httpd_query_key_value(q, key, v, sizeof(v)) != ESP_OK) return false;
    *out = atoi(v);
    return true;
}

static esp_err_t link_send(httpd_req_t *r) {
    prisma_xy_link_state_t s;
    prisma_xy_link_get(&s);
    char b[160];
    snprintf(b, sizeof(b), "{\"locked\":%s,\"randomized\":%s,\"source\":\"%s\",\"rev\":%u}",
             s.locked ? "true" : "false",
             s.randomized ? "true" : "false",
             s.source == PRISMA_XY_AUDIO ? "audio" : "fractal",
             (unsigned)s.rev);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_sendstr(r, b);
}

static esp_err_t link_get(httpd_req_t *r) {
    return link_send(r);
}

static esp_err_t link_post(httpd_req_t *r) {
    int locked = -1, randomized = -1, source = 0;
    bool hl = query_int(r, "locked", &locked);
    bool hr = query_int(r, "randomized", &randomized);
    query_int(r, "source", &source);
    if (hl) prisma_xy_link_set_lock(source ? PRISMA_XY_AUDIO : PRISMA_XY_FRACTAL, locked != 0);
    if (hr) prisma_xy_link_set_random(randomized != 0);
    return link_send(r);
}

void prisma_xy_link_sync_register(httpd_handle_t server) {
    if (!server) return;
    httpd_uri_t g = {.uri="/api/xy/link", .method=HTTP_GET, .handler=link_get};
    httpd_uri_t p = {.uri="/api/xy/link", .method=HTTP_POST, .handler=link_post};
    httpd_register_uri_handler(server, &g);
    httpd_register_uri_handler(server, &p);
}
