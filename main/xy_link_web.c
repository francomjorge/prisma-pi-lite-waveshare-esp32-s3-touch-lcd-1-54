#include "xy_link_web.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xy_control.h"

const char prisma_xy_link_web_injection[] =
"<style>"
".xy-link-controls{margin-top:10px;padding-top:9px;border-top:1px solid #18303d;display:grid;gap:7px}.xy-link-row{display:flex;align-items:center;justify-content:space-between;gap:10px;font:700 10px monospace;letter-spacing:1px;color:#8da8b8}.xy-switch{position:relative;width:42px;height:22px;display:inline-block;flex:0 0 auto}.xy-switch input{opacity:0;width:0;height:0}.xy-switch i{position:absolute;inset:0;border:1px solid #315064;border-radius:20px;background:#0a151d;transition:.18s}.xy-switch i:before{content:'';position:absolute;width:16px;height:16px;left:2px;top:2px;border-radius:50%;background:#7893a3;transition:.18s}.xy-switch input:checked+i{border-color:#28dddd;background:#0b2b30;box-shadow:0 0 8px #28dddd55}.xy-switch input:checked+i:before{transform:translateX(20px);background:#eafcff}.xy-link-state{font:9px monospace;color:#587381;letter-spacing:1px;text-align:center;min-height:11px}.xy-link-controls.linked .xy-link-state{color:#28dddd}.g01-perf{position:absolute;left:10px;top:10px;z-index:5;padding:4px 7px;border:1px solid #22404f;border-radius:10px;background:#041017cc;color:#77e6e6;font:700 10px monospace;pointer-events:none;display:none}.g01-mirror{position:absolute;inset:0;width:100%;height:100%;display:block;background:#000;z-index:1}"
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
"let xy=window.PRISMA_FRACTAL_XY||{x:.5,y:.5},phase=0,rafLast=performance.now(),stepLast=rafLast,appliedX=0,appliedY=0,baseZoom=null,lastWrittenZoom=null,perfSum=0,perfN=0;"
"let zoomEl=document.getElementById('zoom'),visualEl=document.getElementById('visual');if(!zoomEl||!visualEl)return;let perf=document.createElement('div');perf.className='g01-perf';perf.textContent='FPS --';visualEl.appendChild(perf);"
"function clamp(v,a,b){return v<a?a:(v>b?b:v);}"
"function activeG01(){return (window.PRISMA_ACTIVE_GENERATOR||'g01')==='g01';}"
"function readHold(){try{return !!keyHold}catch(e){return false}}"
"function readSpeed(){try{return Math.max(.2,(+speed.value||100)/100)}catch(e){return 1}}"
"function restore(){try{baseX-=appliedX;baseY-=appliedY}catch(e){}appliedX=0;appliedY=0;if(baseZoom!==null){let rz=Math.round(clamp(baseZoom,50,235));if(+zoomEl.value!==rz)zoomEl.value=String(rz);lastWrittenZoom=rz;}}"
"function captureManualZoom(){let z=+zoomEl.value||100;if(lastWrittenZoom===null){baseZoom=z;lastWrittenZoom=z;return}if(Math.abs(z-lastWrittenZoom)>.5){baseZoom=clamp(z,50,235);lastWrittenZoom=z;}}"
"window.addEventListener('prisma-fractal-xy',function(e){if(e&&e.detail)xy=e.detail;});"
"function frame(now){requestAnimationFrame(frame);let raw=now-rafLast;rafLast=now;if(raw>0&&raw<120){perfSum+=raw;perfN++;if(perfN>=30){let fps=1000/(perfSum/perfN);perf.textContent='FPS '+fps.toFixed(0);perfSum=0;perfN=0;}}let on=activeG01();perf.style.display=on?'block':'none';if(now-stepLast<32)return;let dt=Math.min(.08,Math.max(0,(now-stepLast)/1000));stepLast=now;let hold=readHold();window.PRISMA_VIDEO_HOLD=hold;captureManualZoom();if(!on){if(appliedX!==0||appliedY!==0)restore();return;}let fx=clamp((xy.x-.5)*2,-1,1),fy=clamp((.5-xy.y)*2,-1,1),mag=Math.min(1,Math.hypot(fx,fy));if(!hold)phase+=dt*(.65+1.65*mag)*readSpeed();let driftX=hold?0:Math.sin(phase*1.07+fy*.8)*(.11*mag),driftY=hold?0:Math.cos(phase*.83+fx*.7)*(.075*mag);let wantX=fx*.38+driftX,wantY=fy*.26+driftY;try{baseX+=wantX-appliedX;baseY+=wantY-appliedY}catch(e){}appliedX=wantX;appliedY=wantY;if(baseZoom===null)baseZoom=+zoomEl.value||100;let breathe=hold?0:Math.sin(phase*.72)*(.075*mag),factor=Math.exp(fy*.28+breathe),z=Math.round(clamp(baseZoom*factor,50,235));if(+zoomEl.value!==z){zoomEl.value=String(z);lastWrittenZoom=z;}}"
"requestAnimationFrame(frame);"
"})();"
"(function(){"
"const detached=document.documentElement.classList.contains('detached-mode'),visual=document.getElementById('visual');if(!visual)return;"
"if(detached){let qs=new URLSearchParams(location.search),gen=qs.get('generator')||'g01';if(gen!=='g01'||!window.opener||window.opener.closed)return;let own=document.getElementById('g01'),other=document.getElementById('g23');if(own)own.style.display='none';if(other)other.style.display='none';let mirror=document.createElement('canvas');mirror.className='g01-mirror';visual.insertBefore(mirror,visual.firstChild);let ctx=mirror.getContext('2d',{alpha:false});function draw(){if(!window.opener||window.opener.closed)return;try{let src=window.opener.document.getElementById('g01');if(src&&src.width&&src.height){let d=Math.min(devicePixelRatio||1,1.35),w=Math.max(1,Math.floor(visual.clientWidth*d)),h=Math.max(1,Math.floor(visual.clientHeight*d));if(mirror.width!==w||mirror.height!==h){mirror.width=w;mirror.height=h}ctx.drawImage(src,0,0,w,h)}}catch(e){}requestAnimationFrame(draw)}requestAnimationFrame(draw);return;}"
"let btn=document.getElementById('detach');if(!btn)return;let legacy=btn.onclick,child=null,watch=0;function restore(){let v=document.getElementById('visual');if(v)v.style.visibility='';btn.textContent='DE-ATTACH';if(watch){clearInterval(watch);watch=0}child=null;}btn.onclick=function(e){let sel=document.getElementById('generator'),gen=sel?sel.value:'g01';if(gen!=='g01'){return legacy?legacy.call(this,e):undefined}if(e)e.preventDefault();if(child&&!child.closed){child.close();restore();return false}child=window.open('/simple-synth?detached=1&generator=g01','prisma_g01','popup=yes,width=960,height=720,scrollbars=no,resizable=yes');if(child){btn.textContent='RE-ATTACH';let v=document.getElementById('visual');if(v)v.style.visibility='hidden';watch=setInterval(function(){if(!child||child.closed)restore()},350)}return false};"
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
