#include "xy_http.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "xy_control.h"

const char prisma_xy_web_injection[] =
"<style>.xy-box{border:1px solid #1d2d3a;border-radius:10px;padding:10px;background:#071018}.xy-title{font:700 11px monospace;letter-spacing:2px;color:#28dddd;margin-bottom:8px}.xy-pad{position:relative;aspect-ratio:1/1;border:1px solid #314858;border-radius:8px;background:#03070b;touch-action:none;user-select:none;overflow:hidden}.xy-pad:before,.xy-pad:after{content:'';position:absolute;background:#19303d;pointer-events:none}.xy-pad:before{left:50%;top:0;bottom:0;width:1px}.xy-pad:after{top:50%;left:0;right:0;height:1px}.xy-dot{position:absolute;width:14px;height:14px;border-radius:50%;background:#28dddd;border:2px solid #eef7ff;transform:translate(-50%,-50%);box-shadow:0 0 10px #28dddd99;pointer-events:none}.xy-note{font:10px monospace;color:#6f8798;margin-top:7px}.xy-visual-row{display:grid;grid-template-columns:minmax(0,1fr) 250px;gap:14px;align-items:start}.xy-visual-row>.visual{min-width:0}.xy-fractal-box{width:250px}.xy-audio-row{display:grid;grid-template-columns:minmax(0,1fr) 230px;gap:18px;align-items:start}.xy-audio-main{min-width:0}.xy-audio-box{width:230px}#g23{position:absolute;inset:0;width:100%;height:100%;display:none}@media(max-width:760px){.xy-visual-row,.xy-audio-row{grid-template-columns:1fr}.xy-fractal-box,.xy-audio-box{width:100%;max-width:320px}.xy-pad{max-height:320px}}</style><script>"
"(function(){\n"
"const detached=document.documentElement.classList.contains('detached-mode');\n"
"let cards=document.querySelectorAll('section.card');\n"
"function box(kind,title,note,cls){let b=document.createElement('div');b.className='xy-box '+cls;b.innerHTML='<div class=\"xy-title\">'+title+'</div><div class=\"xy-pad\" id=\"xy-'+kind+'\"><i class=\"xy-dot\"></i></div><div class=\"xy-note\">'+note+'</div>';return b;}\n"
"if(!detached&&cards.length>=2){\n"
" let visual=cards[0].querySelector('.visual');if(visual&&!document.getElementById('xy-fractal')){let row=document.createElement('div');row.className='xy-visual-row';visual.parentNode.insertBefore(row,visual);row.appendChild(visual);row.appendChild(box('fractal','FRACTAL X/Y','SHARED WITH APPLIANCE','xy-fractal-box'));}\n"
" let abody=cards[1].querySelector('.fold-body')||cards[1];if(abody&&!document.getElementById('xy-audio')){let row=document.createElement('div');row.className='xy-audio-row';let main=document.createElement('div');main.className='xy-audio-main';while(abody.firstChild)main.appendChild(abody.firstChild);row.appendChild(main);row.appendChild(box('audio','AUDIO X/Y','MAPPING RESERVED','xy-audio-box'));abody.appendChild(row);}\n"
"}\n"
"let drag={fractal:false,audio:false},pending={fractal:null,audio:null},timer={fractal:0,audio:0},rev={fractal:0,audio:0};\n"
"function paint(kind,x,y){let xx=Math.max(0,Math.min(1,x)),yy=Math.max(0,Math.min(1,y)),p=document.getElementById('xy-'+kind),d=p&&p.querySelector('.xy-dot');if(d){d.style.left=(xx*100)+'%';d.style.top=(yy*100)+'%';}if(kind==='fractal'){window.PRISMA_FRACTAL_XY={x:xx,y:yy};window.dispatchEvent(new CustomEvent('prisma-fractal-xy',{detail:window.PRISMA_FRACTAL_XY}));}}\n"
"function flush(kind){timer[kind]=0;let v=pending[kind];if(!v)return;pending[kind]=null;fetch('/api/xy/'+kind+'?x='+v.x.toFixed(5)+'&y='+v.y.toFixed(5),{method:'POST',cache:'no-store'}).catch(function(){});}\n"
"function queue(kind,x,y){paint(kind,x,y);pending[kind]={x:x,y:y};if(!timer[kind])timer[kind]=setTimeout(function(){flush(kind)},35);}\n"
"function pos(e,p){let r=p.getBoundingClientRect();return{x:Math.max(0,Math.min(1,(e.clientX-r.left)/r.width)),y:Math.max(0,Math.min(1,(e.clientY-r.top)/r.height))};}\n"
"if(!detached)['fractal','audio'].forEach(function(kind){let p=document.getElementById('xy-'+kind);if(!p)return;p.addEventListener('pointerdown',function(e){drag[kind]=true;p.setPointerCapture(e.pointerId);let q=pos(e,p);queue(kind,q.x,q.y)});p.addEventListener('pointermove',function(e){if(!drag[kind])return;let q=pos(e,p);queue(kind,q.x,q.y)});function up(){drag[kind]=false}p.addEventListener('pointerup',up);p.addEventListener('pointercancel',up);});\n"
"async function sync(kind){try{let r=await fetch('/api/xy/'+kind,{cache:'no-store'});if(!r.ok)return;let d=await r.json();if((detached||!drag[kind])&&d.rev!==rev[kind]){rev[kind]=d.rev;paint(kind,d.x,d.y)}}catch(e){}}\n"
"sync('fractal');if(!detached)sync('audio');setInterval(function(){sync('fractal');if(!detached)sync('audio')},50);\n"
"})();\n"
"\n"
"(function(){\n"
"const detached=document.documentElement.classList.contains('detached-mode');\n"
"const qs=new URLSearchParams(location.search);\n"
"let sel=document.getElementById('generator'),visual=document.getElementById('visual'),g01=document.getElementById('g01');if(!visual||!g01)return;\n"
"let c=document.createElement('canvas');c.id='g23';visual.insertBefore(c,visual.firstChild);let gl=c.getContext('webgl',{antialias:false,powerPreference:'high-performance'});if(!gl)return;\n"
"let vs='attribute vec2 p;void main(){gl_Position=vec4(p,0.,1.);}';\n"
"let fs='precision highp float;uniform vec2 R;uniform vec2 XY;uniform float MODE;uniform vec4 P;uniform float INSIDE;void main(){vec2 uv=(2.0*gl_FragCoord.xy-R)/min(R.x,R.y);vec2 z;vec2 cc;bool julia=MODE<2.5;float mx=(XY.x-.5)*2.0,my=(.5-XY.y)*2.0,maxI=P.x;if(julia){z=uv*1.45;cc=vec2(-1.0+2.0*XY.x,1.0-2.0*XY.y);}else{float r2=mx*mx+my*my,zoom=1.0+2.8*r2,ang=.38*mx-.28*my,ca=cos(ang),sa=sin(ang);vec2 q=vec2(ca*uv.x-sa*uv.y,sa*uv.x+ca*uv.y);vec2 center=vec2(-.75+mx*.68,my*.58);cc=q*(1.75/zoom)+center;z=vec2(0.0);maxI=min(384.0,max(24.0,P.x*(.72+.28*(1.0-abs(my)))+80.0*abs(mx)));}float k=maxI;float escaped=0.0;for(int i=0;i<384;i++){float fi=float(i);if(fi>=maxI)break;float m=z.x*z.x,n=z.y*z.y;if(m+n>4.0){k=fi;escaped=1.0;break;}vec2 z2=vec2(m-n,2.0*z.x*z.y);if(julia){z=z2+cc;}else{float morph=abs(mx)*.55;vec2 z3=vec2(z2.x*z.x-z2.y*z.y,z2.x*z.y+z2.y*z.x);vec2 zn=mix(z2,z3,morph),tw=vec2(zn.x-my*.22*zn.y,zn.y+my*.22*zn.x);z=tw+cc;}}vec3 col;if(escaped>0.5){if(P.z>1.0){float u=0.0,v=0.0;if(k>(maxI*P.w))u=50.0+P.z+k;else v=100.0+P.z+k;col=clamp(vec3(P.z+P.y*k+18.0*abs(mx),u+35.0*abs(my),v+25.0*abs(mx-my))/255.0,0.0,1.0);}else col=vec3(1.0);}else{col=INSIDE>1.5?vec3(1.0):(INSIDE>0.5?vec3(0.0,0.0,0.5):vec3(0.0));}gl_FragColor=vec4(col,1.0);}';\n"
"function sh(t,s){let x=gl.createShader(t);gl.shaderSource(x,s);gl.compileShader(x);return x}let pr=gl.createProgram();gl.attachShader(pr,sh(gl.VERTEX_SHADER,vs));gl.attachShader(pr,sh(gl.FRAGMENT_SHADER,fs));gl.linkProgram(pr);gl.useProgram(pr);let b=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,b);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,1,-1,-1,1,-1,1,1,-1,1,1]),gl.STATIC_DRAW);let ap=gl.getAttribLocation(pr,'p');gl.enableVertexAttribArray(ap);gl.vertexAttribPointer(ap,2,gl.FLOAT,false,0,0);let uR=gl.getUniformLocation(pr,'R'),uXY=gl.getUniformLocation(pr,'XY'),uM=gl.getUniformLocation(pr,'MODE'),uP=gl.getUniformLocation(pr,'P'),uI=gl.getUniformLocation(pr,'INSIDE');\n"
"const JP=[\n"
" {id:'creation',label:'CREATION',a:.285,b:.535,e:50,d:8,c:85,f:.7,inside:0},\n"
" {id:'rare1',label:'RARE 1',a:.27,b:-.5,e:150,d:8,c:75,f:.4,inside:0},\n"
" {id:'rare2',label:'RARE 2',a:.285,b:.01,e:25,d:15,c:95,f:.2,inside:0},\n"
" {id:'flower1',label:'FLOWER 1',a:-.7,b:-.4,e:60,d:8,c:75,f:.7,inside:0},\n"
" {id:'flower2',label:'FLOWER 2',a:-.70176,b:-.3842,e:75,d:4,c:75,f:.55,inside:0},\n"
" {id:'flower3',label:'FLOWER 3',a:-.4,b:.6,e:85,d:5,c:75,f:.65,inside:0},\n"
" {id:'lightning',label:'LIGHTNING',a:0,b:1,e:20,d:10,c:75,f:.6,inside:0},\n"
" {id:'beauty',label:'BEAUTY',a:-.4,b:.6,e:140,d:1,c:15,f:.5,inside:2}\n"
"];\n"
"const MP=[\n"
" {id:'m1',label:'MANDELBROT 1',x:.5,y:.5,e:1000,d:4,c:2,f:.1,inside:0},\n"
" {id:'m2',label:'MANDELBROT 2',x:.5,y:.5,e:450,d:3,c:2,f:.4,inside:0},\n"
" {id:'m3',label:'MANDELBROT 3',x:.5,y:.5,e:700,d:4,c:55,f:.1,inside:1}\n"
"];\n"
"let xy=window.PRISMA_FRACTAL_XY||{x:.5,y:.5},active=detached?(qs.get('generator')||'g01'):(sel?sel.value:'g01'),queued=false,pop=null;\n"
"let state={g02:{...JP[0]},g03:{...MP[0]}},seen={g02:false,g03:false};\n"
"if(detached&&active!=='g01'){state[active]={id:'detached',label:'DETACHED',e:+qs.get('e')||80,d:+qs.get('d')||4,c:+qs.get('c')||75,f:+qs.get('f')||.5,inside:+qs.get('inside')||0};}\n"
"function current(){return state[active]||{e:80,d:4,c:75,f:.5,inside:0}}\n"
"function title(v){let h=document.querySelector('section.card .fold-head h2')||document.querySelector('section.card h2');if(h)h.textContent=v==='g02'?'VISUAL OUTPUT · G02 JULIA':v==='g03'?'VISUAL OUTPUT · G03 MANDELBROT':'VISUAL OUTPUT · G01';}\n"
"function render(){queued=false;if(active==='g01')return;let r=visual.getBoundingClientRect(),dpr=Math.min(window.devicePixelRatio||1,1.25),w=Math.max(2,Math.floor(r.width*dpr)),h=Math.max(2,Math.floor(r.height*dpr)),s=current(),it=Math.min(384,Math.max(8,+s.e||80));if(c.width!==w||c.height!==h){c.width=w;c.height=h}gl.viewport(0,0,w,h);gl.useProgram(pr);gl.uniform2f(uR,w,h);gl.uniform2f(uXY,xy.x,xy.y);gl.uniform1f(uM,active==='g02'?2.0:3.0);gl.uniform4f(uP,it,+s.d||4,+s.c||2,+s.f||.5);gl.uniform1f(uI,+s.inside||0);gl.drawArrays(gl.TRIANGLES,0,6);}\n"
"function request(){if(!queued){queued=true;requestAnimationFrame(render)}}\n"
"function setXY(x,y){xy={x:Math.max(0,Math.min(1,x)),y:Math.max(0,Math.min(1,y))};window.PRISMA_FRACTAL_XY=xy;fetch('/api/xy/fractal?x='+xy.x.toFixed(5)+'&y='+xy.y.toFixed(5),{method:'POST',cache:'no-store'}).catch(function(){});request();}\n"
"function fromJulia(p){return{x:(p.a+1)/2,y:(1-p.b)/2}}\n"
"let preset=null,ctl=document.querySelector('.vctl');\n"
"function refreshPreset(){if(!preset)return;if(active==='g01'){preset.style.display='none';return}preset.style.display='';let list=active==='g02'?JP:MP;preset.innerHTML='<option value=\"custom\">CUSTOM</option>'+list.map(p=>'<option value=\"'+p.id+'\">'+p.label+'</option>').join('');let id=current().id;preset.value=list.some(p=>p.id===id)?id:'custom';}\n"
"function applyPreset(p){if(!p)return;state[active]={...p};if(active==='g02'){let q=fromJulia(p);setXY(q.x,q.y)}else setXY(p.x,p.y);refreshPreset();broadcast();request();}\n"
"function resetEngine(){applyPreset(active==='g02'?JP[0]:MP[0]);}\n"
"function randomEngine(){if(active==='g02'){state.g02={id:'custom',label:'CUSTOM',e:25+Math.floor(Math.random()*136),d:1+Math.floor(Math.random()*15),c:2+Math.floor(Math.random()*99),f:.15+Math.random()*.7,inside:Math.random()<.12?2:0};setXY(.08+Math.random()*.84,.08+Math.random()*.84)}else{state.g03={id:'custom',label:'CUSTOM',e:90+Math.floor(Math.random()*295),d:2+Math.floor(Math.random()*5),c:2+Math.floor(Math.random()*79),f:.05+Math.random()*.5,inside:Math.random()<.25?1:0};setXY(.18+Math.random()*.64,.18+Math.random()*.64)}refreshPreset();broadcast();request();}\n"
"function broadcast(){if(pop&&!pop.closed&&active!=='g01'){let s=current();pop.postMessage({type:'prisma-fractal-state',generator:active,params:s},location.origin)}}\n"
"function activate(v){active=v;window.PRISMA_ACTIVE_GENERATOR=v;if(v==='g01'){c.style.display='none';g01.style.display='block';if(ctl)ctl.style.display='';}else{g01.style.display='none';c.style.display='block';if(ctl)ctl.style.display='none';if(!seen[v]&&!detached){seen[v]=true;applyPreset(v==='g02'?JP[0]:MP[0]);}request();}title(v);refreshPreset();broadcast();}\n"
"window.addEventListener('prisma-fractal-xy',function(e){xy=e.detail||xy;request()});window.addEventListener('message',function(e){if(e.origin!==location.origin||!e.data||e.data.type!=='prisma-fractal-state')return;active=e.data.generator||active;if(e.data.params)state[active]={...e.data.params};activate(active);request()});window.addEventListener('resize',request);if(window.ResizeObserver)new ResizeObserver(request).observe(visual);\n"
"if(!detached){\n"
" let actions=document.querySelector('.visual-actions');if(actions&&sel){preset=document.createElement('select');preset.id='fractalPreset';sel.insertAdjacentElement('afterend',preset);preset.onchange=function(){if(preset.value==='custom')return;let list=active==='g02'?JP:MP;applyPreset(list.find(p=>p.id===preset.value));};}\n"
" let oldRandom=window.randomVisual,oldReset=window.resetVisual;window.randomVisual=function(){if(active==='g01')return oldRandom();randomEngine()};window.resetVisual=function(){if(active==='g01')return oldReset();resetEngine()};\n"
" if(sel)sel.addEventListener('change',function(){activate(sel.value)});\n"
" let d=document.getElementById('detach');if(d){d.onclick=function(){if(pop&&!pop.closed){pop.close();pop=null;d.textContent='DE-ATTACH';visual.style.visibility='';return}let s=current(),url='/simple-synth?detached=1&generator='+encodeURIComponent(active);if(active!=='g01')url+='&e='+encodeURIComponent(s.e)+'&d='+encodeURIComponent(s.d)+'&c='+encodeURIComponent(s.c)+'&f='+encodeURIComponent(s.f)+'&inside='+encodeURIComponent(s.inside);pop=window.open(url,'prisma_visual','popup=yes,width=960,height=720,scrollbars=no,resizable=yes');if(pop){d.textContent='RE-ATTACH';visual.style.visibility='hidden';let w=setInterval(function(){if(!pop||pop.closed){clearInterval(w);pop=null;d.textContent='DE-ATTACH';visual.style.visibility=''}},500);setTimeout(broadcast,250)}};}\n"
" activate(sel?sel.value:'g01');\n"
"}else activate(active);\n"
"})();\n"
"</script>";

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
