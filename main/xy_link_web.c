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
"controls();sync();setInterval(sync,250);"
"})();"
"(function(){"
"let xy=window.PRISMA_FRACTAL_XY||{x:.5,y:.5},phase=0,stepLast=performance.now(),appliedX=0,appliedY=0;"
"function clamp(v,a,b){return v<a?a:(v>b?b:v);}"
"function activeG01(){return (window.PRISMA_ACTIVE_GENERATOR||'g01')==='g01';}"
"function readHold(){try{return !!keyHold}catch(e){return false}}"
"function readSpeed(){try{return Math.max(.2,(+speed.value||100)/100)}catch(e){return 1}}"
"function restore(){try{baseX-=appliedX;baseY-=appliedY}catch(e){}appliedX=0;appliedY=0;}"
"window.PRISMA_G01_LINK_EXPORT=function(){return{xy:{x:+xy.x||.5,y:+xy.y||.5},phase:phase,appliedX:appliedX,appliedY:appliedY}};"
"window.PRISMA_G01_LINK_IMPORT=function(s){if(!s)return;try{if(s.xy)xy={x:Number.isFinite(+s.xy.x)?+s.xy.x:.5,y:Number.isFinite(+s.xy.y)?+s.xy.y:.5};if(Number.isFinite(+s.phase))phase=+s.phase;if(Number.isFinite(+s.appliedX))appliedX=+s.appliedX;if(Number.isFinite(+s.appliedY))appliedY=+s.appliedY;stepLast=performance.now()}catch(e){}};"
"window.addEventListener('prisma-fractal-xy',function(e){if(e&&e.detail)xy=e.detail;});"
"function frame(now){requestAnimationFrame(frame);if(now-stepLast<32)return;let dt=Math.min(.08,Math.max(0,(now-stepLast)/1000));stepLast=now;let hold=readHold();window.PRISMA_VIDEO_HOLD=hold;if(!activeG01()){if(appliedX!==0||appliedY!==0)restore();return;}if(hold){try{gScale=holdScale;gz=0;tz=0}catch(e){}return;}let fx=clamp((xy.x-.5)*2,-1,1),fy=clamp((.5-xy.y)*2,-1,1),mag=Math.min(1,Math.hypot(fx,fy));phase+=dt*(.65+1.65*mag)*readSpeed();let driftX=Math.sin(phase*1.07+fy*.8)*(.11*mag),driftY=Math.cos(phase*.83+fx*.7)*(.075*mag);let wantX=fx*.38+driftX,wantY=fy*.26+driftY;try{baseX+=wantX-appliedX;baseY+=wantY-appliedY}catch(e){}appliedX=wantX;appliedY=wantY;}"
"requestAnimationFrame(frame);"
"})();"
"(function(){"
"const PARAMS=['stroke','detail','complexity','multiply','speed','zoom','sensitivity'];"
"function n(id){let e=document.getElementById(id);return e?+e.value:0}"
"function snapshot(){try{let s={baseX:baseX,baseY:baseY,gScale:gScale,gx:gx,gy:gy,gz:gz,vx:vx,vy:vy,tx:tx,ty:ty,tz:tz,keyHold:keyHold,holdSensorX:holdSensorX,holdSensorY:holdSensorY,holdBaseX:holdBaseX,holdBaseY:holdBaseY,holdScale:holdScale,holdPx:holdPx,holdPy:holdPy,holdVx:holdVx,holdVy:holdVy};PARAMS.forEach(function(k){s[k]=n(k)});if(window.PRISMA_G01_LINK_EXPORT)s.link=window.PRISMA_G01_LINK_EXPORT();return s}catch(e){return null}}"
"function load(s){if(!s)return false;try{if(Number.isFinite(+s.baseX))baseX=+s.baseX;if(Number.isFinite(+s.baseY))baseY=+s.baseY;if(Number.isFinite(+s.gScale))gScale=+s.gScale;if(Number.isFinite(+s.gx))gx=+s.gx;if(Number.isFinite(+s.gy))gy=+s.gy;if(Number.isFinite(+s.gz))gz=+s.gz;if(Number.isFinite(+s.vx))vx=+s.vx;if(Number.isFinite(+s.vy))vy=+s.vy;if(Number.isFinite(+s.tx))tx=+s.tx;if(Number.isFinite(+s.ty))ty=+s.ty;if(Number.isFinite(+s.tz))tz=+s.tz;keyHold=!!s.keyHold;if(Number.isFinite(+s.holdSensorX))holdSensorX=+s.holdSensorX;if(Number.isFinite(+s.holdSensorY))holdSensorY=+s.holdSensorY;if(Number.isFinite(+s.holdBaseX))holdBaseX=+s.holdBaseX;if(Number.isFinite(+s.holdBaseY))holdBaseY=+s.holdBaseY;if(Number.isFinite(+s.holdScale))holdScale=+s.holdScale;if(Number.isFinite(+s.holdPx))holdPx=+s.holdPx;if(Number.isFinite(+s.holdPy))holdPy=+s.holdPy;if(Number.isFinite(+s.holdVx))holdVx=+s.holdVx;if(Number.isFinite(+s.holdVy))holdVy=+s.holdVy;PARAMS.forEach(function(k){let e=document.getElementById(k);if(e&&Number.isFinite(+s[k])){e.value=String(s[k]);e.dispatchEvent(new Event('input',{bubbles:true}));}});if(s.link&&window.PRISMA_G01_LINK_IMPORT)window.PRISMA_G01_LINK_IMPORT(s.link);return true}catch(e){return false}}"
"window.PRISMA_G01_EXPORT=snapshot;window.PRISMA_G01_IMPORT=load;"
"const detached=document.documentElement.classList.contains('detached-mode'),visual=document.getElementById('visual');if(!visual)return;"
"if(detached){let qs=new URLSearchParams(location.search),gen=qs.get('generator')||'g01';if(gen!=='g01'||!window.opener||window.opener.closed)return;let pulled=false,pullTimer=0,pushTimer=0;function pull(){try{if(window.opener&&!window.opener.closed&&window.opener.PRISMA_G01_EXPORT){pulled=load(window.opener.PRISMA_G01_EXPORT());if(pulled&&pullTimer){clearInterval(pullTimer);pullTimer=0}}}catch(e){}}function push(){try{if(window.opener&&!window.opener.closed&&window.opener.PRISMA_G01_IMPORT)window.opener.PRISMA_G01_IMPORT(snapshot())}catch(e){}}pull();if(!pulled)pullTimer=setInterval(pull,60);pushTimer=setInterval(push,180);window.addEventListener('beforeunload',function(){if(pullTimer)clearInterval(pullTimer);if(pushTimer)clearInterval(pushTimer);push();try{let pc=window.opener.document.getElementById('g01');if(pc)pc.style.display='';let pv=window.opener.document.getElementById('visual');if(pv)pv.style.visibility='';}catch(e){}});return;}"
"let btn=document.getElementById('detach');if(!btn)return;let legacy=btn.onclick,child=null,watch=0,pushInit=0;function importChild(){try{if(child&&!child.closed&&child.PRISMA_G01_EXPORT)return load(child.PRISMA_G01_EXPORT())}catch(e){}return false}function exportChild(){try{if(child&&!child.closed&&child.PRISMA_G01_IMPORT)return child.PRISMA_G01_IMPORT(snapshot())}catch(e){}return false}function restore(){importChild();let v=document.getElementById('visual'),c=document.getElementById('g01');if(c)c.style.display='';if(v)v.style.visibility='';btn.textContent='DE-ATTACH';if(watch){clearInterval(watch);watch=0}if(pushInit){clearInterval(pushInit);pushInit=0}child=null;}btn.onclick=function(e){let sel=document.getElementById('generator'),gen=sel?sel.value:'g01';if(gen!=='g01'){return legacy?legacy.call(this,e):undefined}if(e)e.preventDefault();if(child&&!child.closed){importChild();child.close();restore();return false}child=window.open('/simple-synth?detached=1&generator=g01','prisma_g01','popup=yes,width=960,height=720,scrollbars=no,resizable=yes');if(child){btn.textContent='RE-ATTACH';let v=document.getElementById('visual'),c=document.getElementById('g01');if(c)c.style.display='none';if(v)v.style.visibility='hidden';pushInit=setInterval(function(){if(exportChild()){clearInterval(pushInit);pushInit=0}},60);watch=setInterval(function(){if(!child||child.closed)restore()},250)}return false};"
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
