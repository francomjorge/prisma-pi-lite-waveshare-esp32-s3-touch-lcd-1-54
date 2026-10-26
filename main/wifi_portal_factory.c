#include "wifi_portal.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include "admin_api.h"
#include "audio_api.h"
#include "files_plus_api.h"
#include "playback_http.h"
#include "sensors_web.h"
#include "network_service.h"
#include "security_service.h"

static const char *TAG = "ADMIN_WIFI";

static esp_err_t root_get(httpd_req_t *r)
{
    static const char page[] =
        "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>PRISMA-PI Simple Synth</title><style>*{box-sizing:border-box}body{margin:0;background:#05080d;color:#eef7ff;font:14px system-ui,Arial;padding:22px}main{max-width:760px;margin:auto}h1{letter-spacing:3px;margin:0 0 20px;font-size:24px}.c{color:#28dddd}.card{border:1px solid #263545;border-radius:14px;padding:18px;background:#091019;margin:12px 0}.row{display:flex;justify-content:space-between;gap:12px;margin:10px 0;font-family:monospace}.v{color:#28dddd;font-weight:700}button{display:inline-block;padding:7px 11px;border:1px solid #28dddd;border-radius:20px;color:#28dddd;background:#071018;font-weight:700}.on{background:#28dddd;color:#041012}.visual{height:360px;background:#000;border:1px solid #314858;border-radius:10px;overflow:hidden;position:relative}.visual canvas{width:100%;height:100%;display:block}.visual:fullscreen,.visual:-webkit-full-screen{width:100vw;height:100vh;border:0;border-radius:0}.fs{position:absolute;right:10px;top:10px;z-index:4;opacity:0;pointer-events:none;transition:opacity .16s ease}.visual:hover .fs,.visual:focus-within .fs,.visual.fs-visible .fs{opacity:1;pointer-events:auto}a{color:#28dddd;text-decoration:none;margin-left:16px;font-weight:800;letter-spacing:.5px}.bottomnav{text-align:right;margin-top:14px}@media(max-width:600px){.visual{height:300px}}</style></head>"
        "<body><main><h1>PRISMA-PI <span class='c'>SIMPLE SYNTH</span></h1>"
        "<section class='card'><div id='visual' class='visual' tabindex='0'><canvas id='g01'></canvas><button id='fs' class='fs'>FULLSCREEN</button></div></section>"
        "<section class='card'><div class='row'><span>NOTE</span><span id='note' class='v'>--</span></div><div class='row'><span>FREQUENCY</span><span id='hz' class='v'>--</span></div><div class='row'><span>ROOT / SCALE</span><span id='scale' class='v'>--</span></div><div class='row'><span>WAVEFORM</span><span id='wave' class='v'>--</span></div><div class='row'><span>OUTPUT</span><span id='out' class='v'>--</span></div><div class='row'><span>ORIENTATION</span><span id='angles' class='v'>--</span></div><div style='text-align:right'><button id='audio'>WEB AUDIO OFF</button></div></section>"
        "<div class='bottomnav'><a href='/simple-synth'>SIMPLE SYNTH</a><a href='/admin'>ADMIN</a></div>"
        "<script>let webAudio=false,ac=null,osc=null,gain=null,lastB=null,fsTimer=0,keyHold=false;let tx=0,ty=0,tz=0,gx=0,gy=0,gz=0,vx=0,vy=0,baseX=0,baseY=0,gScale=1,holdSensorX=0,holdSensorY=0,holdBaseX=0,holdBaseY=0,holdScale=1,holdPx=0,holdPy=0,holdVx=0,holdVy=0;let vp={stroke:10,detail:38,complexity:50,multiply:100,speed:100,zoom:100,sensitivity:80,rev:0};const waveType=w=>['sine','square','triangle','sawtooth','square'][w]||'sine';audio.onclick=async()=>{webAudio=!webAudio;if(webAudio){ac=ac||new(window.AudioContext||window.webkitAudioContext)();await ac.resume();osc=ac.createOscillator();gain=ac.createGain();gain.gain.value=0;osc.connect(gain).connect(ac.destination);osc.start();audio.textContent='WEB AUDIO ON';audio.classList.add('on')}else{if(osc){osc.stop();osc=null}audio.textContent='WEB AUDIO OFF';audio.classList.remove('on')}};function inFS(){return document.fullscreenElement||document.webkitFullscreenElement}function hideFS(){clearTimeout(fsTimer);visual.classList.remove('fs-visible')}function showFS(){visual.classList.add('fs-visible');fs.textContent=inFS()?'EXIT FULLSCREEN':'FULLSCREEN';clearTimeout(fsTimer);fsTimer=setTimeout(hideFS,1600)}fs.onclick=e=>{e.stopPropagation();let active=inFS(),f=active?(document.exitFullscreen||document.webkitExitFullscreen):(visual.requestFullscreen||visual.webkitRequestFullscreen);if(f)f.call(active?document:visual);setTimeout(hideFS,120)};visual.addEventListener('mousemove',showFS);visual.addEventListener('mouseenter',showFS);visual.addEventListener('mouseleave',hideFS);visual.addEventListener('touchstart',e=>{if(e.target!==fs)showFS()},{passive:true});document.addEventListener('fullscreenchange',hideFS);document.addEventListener('webkitfullscreenchange',hideFS);"
        "function initVisual(){let c=g01,gl=c.getContext('webgl',{antialias:false,powerPreference:'high-performance'});if(!gl)return;let vs='attribute vec2 p;void main(){gl_Position=vec4(p,0.,1.);}';let f='precision mediump float;uniform vec2 R;uniform float T,X,Y,S,ST,CX,MU,IT;void main(){vec2 u=(2.*gl_FragCoord.xy-R)/min(R.x,R.y);u/=S;u+=vec2(X,Y)*.52;vec2 z=u,c=vec2(-.42+.16*X,-.54+.16*Y);float q=0.,tr=9.;for(int i=0;i<56;i++){if(float(i)>=IT)break;float fi=float(i);z=vec2(z.x*z.x-z.y*z.y,2.*z.x*z.y)*MU+c+.025*(.5+CX)*vec2(sin(T*.27+fi*.61),cos(T*.21+fi*.47));float l=length(z);tr=min(tr,abs(l-(.55+.18*CX)));if(dot(z,z)>24.){q=fi/max(IT,1.);break;}}float line=exp(-ST*tr);vec3 col=.08+.65*cos(vec3(0.,2.1,4.2)+6.2831*(q*(.55+.35*CX)+T*.018));col*=.25+(1.1+CX)*line;gl_FragColor=vec4(max(col,0.),1.);}';function sh(t,s){let x=gl.createShader(t);gl.shaderSource(x,s);gl.compileShader(x);return x}let p=gl.createProgram();gl.attachShader(p,sh(gl.VERTEX_SHADER,vs));gl.attachShader(p,sh(gl.FRAGMENT_SHADER,f));gl.linkProgram(p);gl.useProgram(p);let b=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,b);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,1,-1,-1,1,-1,1,1,-1,1,1]),gl.STATIC_DRAW);let a=gl.getAttribLocation(p,'p');gl.enableVertexAttribArray(a);gl.vertexAttribPointer(a,2,gl.FLOAT,false,0,0);let U={};['R','T','X','Y','S','ST','CX','MU','IT'].forEach(k=>U[k]=gl.getUniformLocation(p,k));let prev=performance.now(),lastFrame=0,visualTime=0,renderScale=.96,perfSum=0,perfN=0,badWindows=0;function frame(n){requestAnimationFrame(frame);if(document.hidden||n-lastFrame<16)return;let frameMs=n-lastFrame;lastFrame=n;let dt=Math.min(.05,(n-prev)/1000);prev=n;let k=1-Math.exp(-dt*10);if(!keyHold){let ax=(tx-gx)*32-vx*10,ay=(ty-gy)*32-vy*10;ax=Math.max(-10,Math.min(10,ax));ay=Math.max(-10,Math.min(10,ay));vx+=ax*dt;vy+=ay*dt;gx+=vx*dt;gy+=vy*dt;visualTime+=dt*(vp.speed/100);if(Math.abs(gz)>.035){gScale*=Math.exp(gz*dt*.55);gScale=Math.max(.55,Math.min(2.4,gScale))}}else{let hx=holdBaseX+(tx-holdSensorX)*.10,hy=holdBaseY+(ty-holdSensorY)*.10,hax=(hx-holdPx)*32-holdVx*10,hay=(hy-holdPy)*32-holdVy*10;hax=Math.max(-10,Math.min(10,hax));hay=Math.max(-10,Math.min(10,hay));holdVx+=hax*dt;holdVy+=hay*dt;holdPx+=holdVx*dt;holdPy+=holdVy*dt;gScale=holdScale*Math.exp(gz*.10);gScale=Math.max(.55,Math.min(2.4,gScale))}gz+=(tz-gz)*k;perfSum+=frameMs;perfN++;if(perfN>=36){let av=perfSum/perfN;if(av>34){if(++badWindows>=2){renderScale=Math.max(.68,renderScale-.06);badWindows=0}}else if(av<22){renderScale=Math.min(1,renderScale+.03);badWindows=0}else badWindows=0;perfSum=0;perfN=0}let d=Math.min(devicePixelRatio||1,1.35)*renderScale,w=Math.max(1,Math.floor(c.clientWidth*d)),h=Math.max(1,Math.floor(c.clientHeight*d));if(c.width!=w||c.height!=h){c.width=w;c.height=h;gl.viewport(0,0,w,h)}let sens=vp.sensitivity/100,px=baseX+gx*sens,py=baseY+gy*sens;if(keyHold){px=holdPx;py=holdPy}gl.uniform2f(U.R,w,h);gl.uniform1f(U.T,visualTime);gl.uniform1f(U.X,px);gl.uniform1f(U.Y,py);gl.uniform1f(U.S,(vp.zoom/100)*gScale);gl.uniform1f(U.ST,vp.stroke);gl.uniform1f(U.CX,vp.complexity/100);gl.uniform1f(U.MU,vp.multiply/100);gl.uniform1f(U.IT,vp.detail);gl.drawArrays(gl.TRIANGLES,0,6)}requestAnimationFrame(frame)}initVisual();"
        "function syncAudio(b){lastB=b;if(webAudio&&osc&&ac){osc.type=waveType(b.waveform);osc.frequency.setTargetAtTime(Number(b.hz)||440,ac.currentTime,.025);gain.gain.setTargetAtTime(b.master&&!b.face_down?.08:0,ac.currentTime,.03)}}async function tick(){try{let rr=await Promise.all([fetch('/api/sensors/imu',{cache:'no-store'}),fetch('/api/sensors/visual',{cache:'no-store'})]);if(!rr[0].ok)throw 0;let d=await rr[0].json();if(!d.sample)throw 0;if(rr[1].ok){let nv=await rr[1].json();if(nv&&nv.rev!==vp.rev)vp=nv}let b=d.beep;note.textContent=b.note;hz.textContent=Number(b.hz).toFixed(2)+' HZ';scale.textContent=b.root_name+' / '+b.scale_name;wave.textContent=b.waveform_name;out.textContent=b.master?(b.face_down?'MUTED':'PLAYING'):'STOPPED';angles.textContent='X '+d.angle.x.toFixed(1)+'  Y '+d.angle.y.toFixed(1)+'  Z '+d.angle.z.toFixed(1);tx=Math.max(-1,Math.min(1,d.angle.x/90));ty=Math.max(-1,Math.min(1,d.angle.y/90));tz=Math.max(-1,Math.min(1,d.gyro.z/12000));let kh=!!d.key_hold,sens=vp.sensitivity/100;if(kh&&!keyHold){keyHold=true;holdSensorX=tx;holdSensorY=ty;holdBaseX=baseX+gx*sens;holdBaseY=baseY+gy*sens;holdScale=gScale;holdPx=holdBaseX;holdPy=holdBaseY;holdVx=vx*sens;holdVy=vy*sens}else if(!kh&&keyHold){gx=tx;gy=ty;baseX=holdPx-gx*sens;baseY=holdPy-gy*sens;keyHold=false;vx=holdVx/Math.max(sens,.1);vy=holdVy/Math.max(sens,.1)}syncAudio(b)}catch(e){if(lastB)syncAudio(lastB)}}setInterval(tick,250);tick()</script></main></body></html>";
    httpd_resp_set_type(r, "text/html");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    return httpd_resp_send(r, page, HTTPD_RESP_USE_STRLEN);
}

void prisma_wifi_start(void)
{
    esp_err_t net = prisma_network_init();
    if (net != ESP_OK) { ESP_LOGE(TAG, "network init failed: %s", esp_err_to_name(net)); return; }
    esp_err_t sec = prisma_security_init();
    if (sec != ESP_OK) { ESP_LOGE(TAG, "security init failed: %s", esp_err_to_name(sec)); return; }
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    hc.uri_match_fn = httpd_uri_match_wildcard;
    hc.lru_purge_enable = true;
    hc.max_uri_handlers = 48;
    hc.stack_size = 12288;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &hc) == ESP_OK) {
        httpd_uri_t root = {.uri="/",.method=HTTP_GET,.handler=root_get};
        httpd_register_uri_handler(server, &root);
        prisma_admin_register(server);
        prisma_sensors_web_register(server);
        prisma_playback_http_register(server);
        prisma_files_plus_register(server);
        prisma_audio_api_register(server);
    }
    prisma_network_status_t status; prisma_network_get_status(&status);
    ESP_LOGI(TAG, "%s ready: / read-only, /simple-synth instrument, /admin system", status.ap_ssid);
}
