#include "xy_link_web.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xy_control.h"

const char prisma_xy_link_web_injection[] =
"<style>"
".xy-link-controls{margin-top:10px;padding-top:9px;border-top:1px solid #18303d;display:grid;gap:7px}.xy-link-row{display:flex;align-items:center;justify-content:space-between;gap:10px;font:700 10px monospace;letter-spacing:1px;color:#8da8b8}.xy-switch{position:relative;width:42px;height:22px;display:inline-block;flex:0 0 auto}.xy-switch input{opacity:0;width:0;height:0}.xy-switch i{position:absolute;inset:0;border:1px solid #315064;border-radius:20px;background:#0a151d;transition:.18s}.xy-switch i:before{content:'';position:absolute;width:16px;height:16px;left:2px;top:2px;border-radius:50%;background:#7893a3;transition:.18s}.xy-switch input:checked+i{border-color:#28dddd;background:#0b2b30;box-shadow:0 0 8px #28dddd55}.xy-switch input:checked+i:before{transform:translateX(20px);background:#eafcff}.xy-link-state{font:9px monospace;color:#587381;letter-spacing:1px;text-align:center;min-height:11px}.xy-link-controls.linked .xy-link-state{color:#28dddd}"
"</style><script>(function(){"
"if(document.documentElement.classList.contains('detached-mode'))return;"
"const K=['fractal','audio'];let state={locked:false,source:'fractal',rev:0};"
"function controls(){K.forEach(function(k){let pad=document.getElementById('xy-'+k),box=pad&&pad.closest('.xy-box');if(!box||box.querySelector('.xy-link-controls'))return;let c=document.createElement('div');c.className='xy-link-controls';c.dataset.kind=k;c.innerHTML='<div class=\"xy-link-row\"><span>LOCK X/Y</span><label class=\"xy-switch\"><input type=\"checkbox\" class=\"xy-lock\"><i></i></label></div><div class=\"xy-link-state\">INDEPENDENT</div>';box.appendChild(c);c.querySelector('.xy-lock').addEventListener('change',function(){setLock(k,this.checked)});});ui();}"
"function ui(){document.querySelectorAll('.xy-link-controls').forEach(function(c){c.classList.toggle('linked',state.locked);let l=c.querySelector('.xy-lock'),s=c.querySelector('.xy-link-state');if(l)l.checked=state.locked;if(s)s.textContent=state.locked?'LINKED · MIRROR':'INDEPENDENT';});}"
"function apply(d){if(!d)return;state.locked=!!d.locked;state.source=d.source==='audio'?'audio':'fractal';state.rev=+d.rev||state.rev;ui();}"
"async function setLock(kind,on){try{let r=await fetch('/api/xy/lock?on='+(on?1:0)+'&source='+kind,{method:'POST',cache:'no-store'});if(r.ok)apply(await r.json());else ui();}catch(e){ui();}}"
"async function sync(){try{let r=await fetch('/api/xy/lock',{cache:'no-store'});if(!r.ok)return;let d=await r.json();if((+d.rev||0)!==state.rev||!!d.locked!==state.locked)apply(d);}catch(e){}}"
"controls();sync();setInterval(sync,100);"
"})();"
"(function(){"
"let xy=window.PRISMA_FRACTAL_XY||{x:.5,y:.5},phase=0,last=performance.now(),appliedX=0,appliedY=0,baseZoom=null,lastWrittenZoom=null;"
"let zoomEl=document.getElementById('zoom');if(!zoomEl)return;"
"function clamp(v,a,b){return v<a?a:(v>b?b:v);}"
"function activeG01(){return (window.PRISMA_ACTIVE_GENERATOR||'g01')==='g01';}"
"function readHold(){try{return !!keyHold}catch(e){return false}}"
"function readSpeed(){try{return Math.max(.2,(+speed.value||100)/100)}catch(e){return 1}}"
"function restore(){try{baseX-=appliedX;baseY-=appliedY}catch(e){}appliedX=0;appliedY=0;if(baseZoom!==null){zoomEl.value=String(Math.round(clamp(baseZoom,50,235)));lastWrittenZoom=+zoomEl.value;} }"
"function captureManualZoom(){let z=+zoomEl.value||100;if(lastWrittenZoom===null){baseZoom=z;lastWrittenZoom=z;return}if(Math.abs(z-lastWrittenZoom)>.5){baseZoom=clamp(z,50,235);lastWrittenZoom=z;}}"
"window.addEventListener('prisma-fractal-xy',function(e){if(e&&e.detail)xy=e.detail;});"
"function frame(now){requestAnimationFrame(frame);let dt=Math.min(.05,Math.max(0,(now-last)/1000));last=now;let hold=readHold();window.PRISMA_VIDEO_HOLD=hold;captureManualZoom();if(!activeG01()){if(appliedX!==0||appliedY!==0)restore();return;}let fx=clamp((xy.x-.5)*2,-1,1),fy=clamp((.5-xy.y)*2,-1,1),mag=Math.min(1,Math.hypot(fx,fy));if(!hold)phase+=dt*(.65+1.65*mag)*readSpeed();let driftX=hold?0:Math.sin(phase*1.07+fy*.8)*(.11*mag),driftY=hold?0:Math.cos(phase*.83+fx*.7)*(.075*mag);let wantX=fx*.38+driftX,wantY=fy*.26+driftY;try{baseX+=wantX-appliedX;baseY+=wantY-appliedY}catch(e){}appliedX=wantX;appliedY=wantY;if(baseZoom===null)baseZoom=+zoomEl.value||100;let breathe=hold?0:Math.sin(phase*.72)*(.075*mag),factor=Math.exp(fy*.28+breathe),z=clamp(baseZoom*factor,50,235);zoomEl.value=String(Math.round(z));lastWrittenZoom=+zoomEl.value;}"
"requestAnimationFrame(frame);"
"})();</script>";

static esp_err_t lock_send(httpd_req_t *r) {
    prisma_xy_lock_state_t s;
    prisma_xy_lock_get(&s);
    char b[128];
    snprintf(b, sizeof(b), "{\"locked\":%s,\"source\":\"%s\",\"rev\":%u}",
             s.locked ? "true" : "false",
             s.source == PRISMA_XY_AUDIO ? "audio" : "fractal",
             (unsigned)s.rev);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_sendstr(r, b);
}

static bool query_value(httpd_req_t *r, const char *key, char *out, size_t out_len) {
    char q[160];
    if (httpd_req_get_url_query_str(r, q, sizeof(q)) != ESP_OK) return false;
    return httpd_query_key_value(q, key, out, out_len) == ESP_OK;
}

static esp_err_t lock_get(httpd_req_t *r) {
    return lock_send(r);
}

static esp_err_t lock_post(httpd_req_t *r) {
    char on[12] = {0};
    char source[20] = {0};
    bool has_on = query_value(r, "on", on, sizeof(on));
    query_value(r, "source", source, sizeof(source));
    if (has_on) {
        bool enabled = atoi(on) != 0;
        prisma_xy_channel_t src = strcmp(source, "audio") == 0 ? PRISMA_XY_AUDIO : PRISMA_XY_FRACTAL;
        prisma_xy_lock_set(enabled, src);
    }
    return lock_send(r);
}

void prisma_xy_link_http_register(httpd_handle_t server) {
    if (!server) return;
    httpd_uri_t lg = {.uri="/api/xy/lock", .method=HTTP_GET, .handler=lock_get};
    httpd_uri_t lp = {.uri="/api/xy/lock", .method=HTTP_POST, .handler=lock_post};
    httpd_register_uri_handler(server, &lg);
    httpd_register_uri_handler(server, &lp);
}
