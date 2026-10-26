#include "xy_http.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "xy_control.h"

const char prisma_xy_web_injection[] =
"<style>"
".xy-box{border:1px solid #1d2d3a;border-radius:10px;padding:10px;background:#071018}.xy-title{font:700 11px monospace;letter-spacing:2px;color:#28dddd;margin-bottom:8px}.xy-pad{position:relative;aspect-ratio:1/1;border:1px solid #314858;border-radius:8px;background:#03070b;touch-action:none;user-select:none;overflow:hidden}.xy-pad:before,.xy-pad:after{content:'';position:absolute;background:#19303d;pointer-events:none}.xy-pad:before{left:50%;top:0;bottom:0;width:1px}.xy-pad:after{top:50%;left:0;right:0;height:1px}.xy-dot{position:absolute;width:14px;height:14px;border-radius:50%;background:#28dddd;border:2px solid #eef7ff;transform:translate(-50%,-50%);box-shadow:0 0 10px #28dddd99;pointer-events:none}.xy-note{font:10px monospace;color:#6f8798;margin-top:7px}.xy-visual-row{display:grid;grid-template-columns:minmax(0,1fr) 250px;gap:14px;align-items:start}.xy-visual-row>.visual{min-width:0}.xy-fractal-box{width:250px}.xy-audio-row{display:grid;grid-template-columns:minmax(0,1fr) 230px;gap:18px;align-items:start}.xy-audio-main{min-width:0}.xy-audio-box{width:230px}#g23{position:absolute;inset:0;width:100%;height:100%;display:none}@media(max-width:760px){.xy-visual-row,.xy-audio-row{grid-template-columns:1fr}.xy-fractal-box,.xy-audio-box{width:100%;max-width:320px}.xy-pad{max-height:320px}}"
"</style><script>(function(){"
"if(document.documentElement.classList.contains('detached-mode'))return;"
"let cards=document.querySelectorAll('section.card');if(cards.length<2)return;"
"function box(kind,title,note,cls){let b=document.createElement('div');b.className='xy-box '+cls;b.innerHTML='<div class=\"xy-title\">'+title+'</div><div class=\"xy-pad\" id=\"xy-'+kind+'\"><i class=\"xy-dot\"></i></div><div class=\"xy-note\">'+note+'</div>';return b;}"
"let visual=cards[0].querySelector('.visual'),vbody=cards[0].querySelector('.fold-body')||cards[0];if(visual){let row=document.createElement('div');row.className='xy-visual-row';visual.parentNode.insertBefore(row,visual);row.appendChild(visual);row.appendChild(box('fractal','FRACTAL X/Y','SHARED WITH APPLIANCE','xy-fractal-box'));}"
"let abody=cards[1].querySelector('.fold-body')||cards[1];if(abody){let row=document.createElement('div');row.className='xy-audio-row';let main=document.createElement('div');main.className='xy-audio-main';while(abody.firstChild)main.appendChild(abody.firstChild);row.appendChild(main);row.appendChild(box('audio','AUDIO X/Y','MAPPING RESERVED','xy-audio-box'));abody.appendChild(row);}"
"let drag={fractal:false,audio:false},pending={fractal:null,audio:null},timer={fractal:0,audio:0},rev={fractal:0,audio:0};"
"function paint(kind,x,y){let xx=Math.max(0,Math.min(1,x)),yy=Math.max(0,Math.min(1,y)),p=document.getElementById('xy-'+kind),d=p&&p.querySelector('.xy-dot');if(d){d.style.left=(xx*100)+'%';d.style.top=(yy*100)+'%';}if(kind==='fractal'){window.PRISMA_FRACTAL_XY={x:xx,y:yy};window.dispatchEvent(new CustomEvent('prisma-fractal-xy',{detail:window.PRISMA_FRACTAL_XY}));}}"
"function flush(kind){timer[kind]=0;let v=pending[kind];if(!v)return;pending[kind]=null;fetch('/api/xy/'+kind+'?x='+v.x.toFixed(5)+'&y='+v.y.toFixed(5),{method:'POST',cache:'no-store'}).catch(function(){});}"
"function queue(kind,x,y){paint(kind,x,y);pending[kind]={x:x,y:y};if(!timer[kind])timer[kind]=setTimeout(function(){flush(kind)},35);}"
"function pos(e,p){let r=p.getBoundingClientRect();return{x:Math.max(0,Math.min(1,(e.clientX-r.left)/r.width)),y:Math.max(0,Math.min(1,(e.clientY-r.top)/r.height))};}"
"['fractal','audio'].forEach(function(kind){let p=document.getElementById('xy-'+kind);if(!p)return;p.addEventListener('pointerdown',function(e){drag[kind]=true;p.setPointerCapture(e.pointerId);let q=pos(e,p);queue(kind,q.x,q.y)});p.addEventListener('pointermove',function(e){if(!drag[kind])return;let q=pos(e,p);queue(kind,q.x,q.y)});function up(){drag[kind]=false}p.addEventListener('pointerup',up);p.addEventListener('pointercancel',up);});"
"async function sync(kind){try{let r=await fetch('/api/xy/'+kind,{cache:'no-store'});if(!r.ok)return;let d=await r.json();if(!drag[kind]&&d.rev!==rev[kind]){rev[kind]=d.rev;paint(kind,d.x,d.y)}}catch(e){}}"
"sync('fractal');sync('audio');setInterval(function(){sync('fractal');sync('audio')},50);"
"})();</script>"
"<script>(function(){"
"if(document.documentElement.classList.contains('detached-mode'))return;"
"let sel=document.getElementById('generator'),visual=document.getElementById('visual'),g01=document.getElementById('g01');if(!sel||!visual||!g01)return;"
"let c=document.createElement('canvas');c.id='g23';visual.insertBefore(c,visual.firstChild);let gl=c.getContext('webgl',{antialias:false,powerPreference:'high-performance'});if(!gl)return;"
"let vs='attribute vec2 p;void main(){gl_Position=vec4(p,0.,1.);}';"
"let fs='precision highp float;uniform vec2 R;uniform vec2 XY;uniform float MODE;void main(){vec2 uv=(2.0*gl_FragCoord.xy-R)/min(R.x,R.y);vec2 z;vec2 cc;float maxI;bool julia=MODE<2.5;if(julia){z=uv*1.45;cc=vec2(-1.0+2.0*XY.x,1.0-2.0*XY.y);maxI=150.0;}else{vec2 center=vec2(-0.75+(XY.x-0.5)*2.5,(0.5-XY.y)*2.0);cc=uv*1.75+center;z=vec2(0.0);maxI=180.0;}float q=0.0;float escaped=0.0;for(int i=0;i<192;i++){float fi=float(i);if(fi>=maxI)break;float m=z.x*z.x,n=z.y*z.y;if(m+n>4.0){q=fi/maxI;escaped=1.0;break;}z=vec2(m-n+cc.x,2.0*z.x*z.y+cc.y);}vec3 col=vec3(0.0);if(escaped>0.5){if(julia){col=0.45+0.55*cos(vec3(0.0,2.1,4.2)+6.2831*(q*1.35+0.08));col*=0.45+1.1*sqrt(q);}else{float t=sqrt(q);col=mix(vec3(0.02,0.08,0.35),vec3(1.0,0.18,0.05),smoothstep(0.08,0.75,t));col+=0.18*vec3(t,t*t,0.0);}}gl_FragColor=vec4(max(col,0.0),1.0);}';"
"function sh(t,s){let x=gl.createShader(t);gl.shaderSource(x,s);gl.compileShader(x);return x}let pr=gl.createProgram();gl.attachShader(pr,sh(gl.VERTEX_SHADER,vs));gl.attachShader(pr,sh(gl.FRAGMENT_SHADER,fs));gl.linkProgram(pr);gl.useProgram(pr);let b=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,b);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,1,-1,-1,1,-1,1,1,-1,1,1]),gl.STATIC_DRAW);let ap=gl.getAttribLocation(pr,'p');gl.enableVertexAttribArray(ap);gl.vertexAttribPointer(ap,2,gl.FLOAT,false,0,0);let uR=gl.getUniformLocation(pr,'R'),uXY=gl.getUniformLocation(pr,'XY'),uM=gl.getUniformLocation(pr,'MODE');let xy=window.PRISMA_FRACTAL_XY||{x:.5,y:.5},active='g01',queued=false;"
"function title(v){let h=document.querySelector('section.card .fold-head h2')||document.querySelector('section.card h2');if(h)h.textContent=v==='g02'?'VISUAL OUTPUT · G02 JULIA':v==='g03'?'VISUAL OUTPUT · G03 MANDELBROT':'VISUAL OUTPUT · G01';}"
"function render(){queued=false;if(active==='g01')return;let r=visual.getBoundingClientRect(),d=Math.min(window.devicePixelRatio||1,1.35),w=Math.max(2,Math.floor(r.width*d)),h=Math.max(2,Math.floor(r.height*d));if(c.width!==w||c.height!==h){c.width=w;c.height=h}gl.viewport(0,0,w,h);gl.useProgram(pr);gl.uniform2f(uR,w,h);gl.uniform2f(uXY,xy.x,xy.y);gl.uniform1f(uM,active==='g02'?2.0:3.0);gl.drawArrays(gl.TRIANGLES,0,6);}"
"function request(){if(!queued){queued=true;requestAnimationFrame(render)}}"
"function activate(v){active=v;window.PRISMA_ACTIVE_GENERATOR=v;if(v==='g01'){c.style.display='none';g01.style.display='block';}else{g01.style.display='none';c.style.display='block';request();}title(v);}"
"window.addEventListener('prisma-fractal-xy',function(e){xy=e.detail||xy;request()});sel.addEventListener('change',function(){activate(sel.value)});window.addEventListener('resize',request);if(window.ResizeObserver)new ResizeObserver(request).observe(visual);activate(sel.value||'g01');"
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
