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
        "<title>PRISMA-PI Simple Synth</title><style>*{box-sizing:border-box}body{margin:0;background:#05080d;color:#eef7ff;font:14px system-ui,Arial;padding:22px}main{max-width:760px;margin:auto}h1{letter-spacing:3px;margin:0;font-size:24px}.c{color:#28dddd}.sub{color:#8ea6b8;letter-spacing:2px;margin:5px 0 20px}.card{border:1px solid #263545;border-radius:14px;padding:18px;background:#091019;margin:12px 0}.row{display:flex;justify-content:space-between;gap:12px;margin:10px 0;font-family:monospace}.v{color:#28dddd;font-weight:700}.live,button{display:inline-block;padding:7px 11px;border:1px solid #28dddd;border-radius:20px;color:#28dddd;background:#071018;font-weight:700}.on{background:#28dddd;color:#041012}.visual{height:360px;background:#000;border:1px solid #314858;border-radius:10px;overflow:hidden;position:relative}.visual canvas{width:100%;height:100%;display:block}.visual:fullscreen{width:100vw;height:100vh;border:0}.fs{position:absolute;right:10px;top:10px;z-index:4}a{color:#28dddd;text-decoration:none;margin-left:16px}.topnav{display:flex;align-items:center;justify-content:flex-end;gap:14px;flex-wrap:wrap;margin-bottom:14px}.topnav a{margin-left:0;font-weight:800;letter-spacing:.5px}@media(max-width:600px){.visual{height:300px}.topnav{justify-content:flex-start;gap:10px}} </style></head>"
        "<body><main><nav class='topnav'><a href='/'>LIVE</a><a href='/admin'>ADMIN</a><a href='/simple-synth'>SIMPLE SYNTH</a></nav><div style='display:flex;justify-content:space-between;align-items:center'><div><h1>PRISMA-PI <span class='c'>SIMPLE SYNTH</span></h1><div class='sub'>PUBLIC READ-ONLY MONITOR</div></div><span id='live' class='live'>CONNECTING</span></div>"
        "<section class='card'><div id='visual' class='visual'><canvas id='g01'></canvas><button id='fs' class='fs'>FULLSCREEN</button></div></section>"
        "<section class='card'><div class='row'><span>NOTE</span><span id='note' class='v'>--</span></div><div class='row'><span>FREQUENCY</span><span id='hz' class='v'>--</span></div><div class='row'><span>ROOT / SCALE</span><span id='scale' class='v'>--</span></div><div class='row'><span>WAVEFORM</span><span id='wave' class='v'>--</span></div><div class='row'><span>OUTPUT</span><span id='out' class='v'>--</span></div><div class='row'><span>ORIENTATION</span><span id='angles' class='v'>--</span></div><div style='text-align:right'><button id='audio'>WEB AUDIO OFF</button></div></section>"
        "<div style='text-align:right'><a href='/simple-synth'>SIMPLE SYNTH</a><a href='/admin'>ADMIN</a></div>"
        "<script>let gx=0,gy=0,webAudio=false,ac=null,osc=null,gain=null,lastGood=0,lastB=null;const waveType=w=>['sine','square','triangle','sawtooth','square'][w]||'sine';audio.onclick=async()=>{webAudio=!webAudio;if(webAudio){ac=ac||new(window.AudioContext||window.webkitAudioContext)();await ac.resume();osc=ac.createOscillator();gain=ac.createGain();gain.gain.value=0;osc.connect(gain).connect(ac.destination);osc.start();audio.textContent='WEB AUDIO ON';audio.classList.add('on')}else{if(osc){osc.stop();osc=null}audio.textContent='WEB AUDIO OFF';audio.classList.remove('on')}};fs.onclick=()=>document.fullscreenElement?document.exitFullscreen():visual.requestFullscreen();document.addEventListener('fullscreenchange',()=>fs.textContent=document.fullscreenElement?'EXIT FULLSCREEN':'FULLSCREEN');"
        "function initVisual(){let c=g01,gl=c.getContext('webgl',{antialias:false});if(!gl)return;let vs='attribute vec2 p;void main(){gl_Position=vec4(p,0.,1.);}';let f='precision highp float;uniform vec2 R;uniform float T,X,Y;void main(){vec2 u=(2.*gl_FragCoord.xy-R)/min(R.x,R.y);u+=vec2(X,Y)*.52;vec2 z=u,c=vec2(-.42+.16*X,-.54+.16*Y);float q=0.,tr=9.;for(int i=0;i<40;i++){z=vec2(z.x*z.x-z.y*z.y,2.*z.x*z.y)+c+.03*vec2(sin(T*.27+float(i)*.61),cos(T*.21+float(i)*.47));float l=length(z);tr=min(tr,abs(l-.65));if(dot(z,z)>24.){q=float(i)/40.;break;}}float line=exp(-10.*tr);vec3 col=.08+.65*cos(vec3(0.,2.1,4.2)+6.2831*(q*.7+T*.018));col*=.28+1.6*line;gl_FragColor=vec4(max(col,0.),1.);}';function sh(t,s){let x=gl.createShader(t);gl.shaderSource(x,s);gl.compileShader(x);return x}let p=gl.createProgram();gl.attachShader(p,sh(gl.VERTEX_SHADER,vs));gl.attachShader(p,sh(gl.FRAGMENT_SHADER,f));gl.linkProgram(p);gl.useProgram(p);let b=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,b);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,1,-1,-1,1,-1,1,1,-1,1,1]),gl.STATIC_DRAW);let a=gl.getAttribLocation(p,'p');gl.enableVertexAttribArray(a);gl.vertexAttribPointer(a,2,gl.FLOAT,false,0,0);let R=gl.getUniformLocation(p,'R'),T=gl.getUniformLocation(p,'T'),X=gl.getUniformLocation(p,'X'),Y=gl.getUniformLocation(p,'Y'),t0=performance.now();function frame(n){let d=devicePixelRatio||1,w=Math.floor(c.clientWidth*d),h=Math.floor(c.clientHeight*d);if(c.width!=w||c.height!=h){c.width=w;c.height=h;gl.viewport(0,0,w,h)}gl.uniform2f(R,w,h);gl.uniform1f(T,(n-t0)/1000);gl.uniform1f(X,gx);gl.uniform1f(Y,gy);gl.drawArrays(gl.TRIANGLES,0,6);requestAnimationFrame(frame)}requestAnimationFrame(frame)}initVisual();"
        "function syncAudio(b){lastB=b;if(webAudio&&osc&&ac){osc.type=waveType(b.waveform);osc.frequency.setTargetAtTime(Number(b.hz)||440,ac.currentTime,.025);gain.gain.setTargetAtTime(b.master&&!b.face_down?.08:0,ac.currentTime,.03)}}async function tick(){try{let r=await fetch('/api/sensors/imu',{cache:'no-store'});if(!r.ok)throw 0;let d=await r.json();if(!d.sample)throw 0;lastGood=Date.now();live.textContent='LIVE';let b=d.beep;note.textContent=b.note;hz.textContent=Number(b.hz).toFixed(2)+' HZ';scale.textContent=b.root_name+' / '+b.scale_name;wave.textContent=b.waveform_name;out.textContent=b.master?(b.face_down?'MUTED':'PLAYING'):'STOPPED';angles.textContent='X '+d.angle.x.toFixed(1)+'  Y '+d.angle.y.toFixed(1)+'  Z '+d.angle.z.toFixed(1);gx+=(Math.max(-1,Math.min(1,d.angle.x/90))-gx)*.2;gy+=(Math.max(-1,Math.min(1,d.angle.y/90))-gy)*.2;syncAudio(b)}catch(e){if(Date.now()-lastGood>1800)live.textContent='NO SAMPLE';if(lastB)syncAudio(lastB)}}setInterval(tick,300);tick()</script></main></body></html>";
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