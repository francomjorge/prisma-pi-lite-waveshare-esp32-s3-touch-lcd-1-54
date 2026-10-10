#include "esp_http_server.h"

#ifndef PRISMA_INTENSITY_PAGE_GET
#define PRISMA_INTENSITY_PAGE_GET checkpoint_page_get
#endif
static esp_err_t PRISMA_INTENSITY_PAGE_GET(httpd_req_t *r);

#define checkpoint_page_get(...) intensity_base_page_get(__VA_ARGS__)
#include "wifi_portal_checkpoint_a.c"
#undef checkpoint_page_get

static const char intensity_web_injection[] =
"\n"
"<style>\n"
".intensityPresetRow{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:14px}\n"
".intensityPresetBtn{height:42px;border:1px solid #2d3c4b;border-radius:11px;background:#172536;color:#788897;display:flex;align-items:center;justify-content:center;font-size:18px;font-weight:900;user-select:none;-webkit-user-select:none}\n"
".intensityPresetBtn.on.free{background:#23dcdc;border-color:#23dcdc;color:#04070c}\n"
".intensityPresetBtn.on.green{background:#23c86e;border-color:#23c86e;color:#f0f5fa}\n"
".intensityPresetBtn.on.yellow{background:#f5be23;border-color:#f5be23;color:#04070c}\n"
".intensityPresetBtn.on.red{background:#e63741;border-color:#e63741;color:#f0f5fa}\n"
"</style>\n"
"<script>(function(){\n"
"var oldIntensityBar=document.getElementById('intensityBar');\n"
"if(oldIntensityBar){\n"
"  var card=oldIntensityBar.closest('.card');\n"
"  if(card){card.innerHTML='<div class=\"row compact\"><div class=\"label stronglabel\">INTENSITY</div></div><div id=\"intensityPresetRow\" class=\"intensityPresetRow\" data-control=\"1\"><div id=\"intensityPreset0\" class=\"intensityPresetBtn on free\">0</div><div id=\"intensityPreset1\" class=\"intensityPresetBtn\">1</div><div id=\"intensityPreset2\" class=\"intensityPresetBtn\">2</div><div id=\"intensityPreset3\" class=\"intensityPresetBtn\">3</div></div>'; }\n"
"}\n"
"function decodeIntensityPreset(s){\n"
"  var state=Math.round(Number(s&&s.intensity));if(!isFinite(state)||state<0)state=0;if(state>9)state=9;\n"
"  if(state===0)return {state:0,button:0,level:0};\n"
"  var z=state-1;return {state:state,button:Math.floor(z/3)+1,level:z%3};\n"
"}\n"
"function drawIntensityPreset(s){\n"
"  var d=decodeIntensityPreset(s),classes=['green','yellow','red'];\n"
"  for(var i=0;i<4;i++){var b=document.getElementById('intensityPreset'+i);if(!b)continue;b.className='intensityPresetBtn';if(i===d.button)b.className+=' on '+(i===0?'free':classes[d.level]);}\n"
"}\n"
"var intensityOldDrawAll=drawAll;drawAll=function(s){intensityOldDrawAll(s);drawIntensityPreset(s);};\n"
"var intensityOldRunFxPipeline=runFxPipeline;runFxPipeline=function(s){\n"
"  if(!s||!Array.isArray(s.motionValues))return intensityOldRunFxPipeline(s);\n"
"  var mapped={},k;for(k in s){if(Object.prototype.hasOwnProperty.call(s,k))mapped[k]=s[k];}\n"
"  mapped.motionValues=s.motionValues.slice();\n"
"  for(var i=0;i<mapped.motionValues.length;i++){var x=Math.max(0,Math.min(100,Number(mapped.motionValues[i])||0))/100;mapped.motionValues[i]=Math.pow(x,1.8)*100;}\n"
"  return intensityOldRunFxPipeline(mapped);\n"
"};\n"
"if(latestState)drawIntensityPreset(latestState);\n"
"})();</script>\n";

/*
 * FX Lab Sources V1 is intentionally browser-only.  It is injected after the
 * stable controller code so app_checkpoint_a.c and the appliance UI remain
 * byte-for-byte untouched.  Local video files stay in the browser as Blob
 * object URLs; they are never written to SPIFFS or NVS.
 */
static const char fxlab_sources_web_injection[] =
"\n"
"<style>\n"
".labPage.fxlabScrollable{height:calc(100vh - 118px);min-height:0!important;overflow-y:auto!important;overflow-x:hidden!important;overscroll-behavior:contain;touch-action:pan-y;padding-bottom:96px;scroll-behavior:smooth}\n"
".labPage.fxlabScrollable::-webkit-scrollbar{width:8px}.labPage.fxlabScrollable::-webkit-scrollbar-thumb{background:#2d3c4b;border-radius:8px}\n"
".labZoomReadout{font-size:9px;color:#23dcdc;font-weight:900;letter-spacing:1px;border:1px solid #203040;border-radius:8px;padding:5px 7px;white-space:nowrap}\n"
".labSourcesPanel{margin-top:16px;background:#0c1520;border:1px solid #203040;border-radius:16px;padding:12px}\n"
".labSourcesHead{display:flex;align-items:flex-end;justify-content:space-between;gap:10px;margin-bottom:10px}.labSourcesTitle{font-size:12px;font-weight:900;letter-spacing:1.5px}.labSourcesSub{font-size:8px;color:#657585;letter-spacing:1px;margin-top:3px}\n"
".labLayerRow{display:grid;grid-template-columns:38px minmax(0,1fr) minmax(110px,1.25fr) 38px;gap:8px;align-items:center;padding:9px 0;border-top:1px solid #172536}.labLayerRow:first-child{border-top:0}.labLayerMeta{min-width:0}.labLayerName{font-size:11px;font-weight:900;letter-spacing:.8px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.labLayerType{font-size:8px;color:#657585;letter-spacing:.8px;margin-top:3px}.labEye,.labRemove{width:34px;height:32px;border:1px solid #2b4054;background:#071019;color:#f0f5fa;border-radius:9px;font-size:16px;cursor:pointer}.labEye.off{opacity:.35}.labRemove{color:#e63741;font-size:18px}.labRemove.placeholder{visibility:hidden}.labOpacityWrap{display:grid;grid-template-columns:minmax(70px,1fr) 38px;gap:6px;align-items:center}.labOpacity{width:100%;accent-color:#23dcdc}.labOpacityValue{text-align:right;font-size:10px;font-weight:900;color:#9fb0bf}\n"
".labAddSource{width:100%;margin-top:11px;height:40px;border:1px solid #23dcdc;background:#071019;color:#23dcdc;border-radius:10px;font-size:10px;font-weight:900;letter-spacing:1px;cursor:pointer}.labSourceStatus{min-height:16px;margin-top:7px;color:#657585;font-size:8px;letter-spacing:.8px;text-align:center}.labSourceStatus.error{color:#e63741}.labSourceStatus.ok{color:#23c86e}\n"
"@media(max-width:560px){.labLayerRow{grid-template-columns:34px minmax(0,1fr) 34px}.labOpacityWrap{grid-column:2/4}.labPage.fxlabScrollable{height:calc(100vh - 108px)}}\n"
"</style>\n"
"<script>(function(){\n"
"var labPage=document.querySelector('.labPage');if(!labPage||!labCanvas||!labFrame)return;\n"
"labPage.classList.add('fxlabScrollable');\n"
"var labHint=labPage.querySelector('.labHint');if(labHint)labHint.textContent='DRAG FX OR USE LEFT/RIGHT · UP/DOWN SCROLL · I/O ZOOM · F FULLSCREEN';\n"
"var head=labPage.querySelector('.labHead');var zoomReadout=document.createElement('div');zoomReadout.id='labZoomReadout';zoomReadout.className='labZoomReadout';zoomReadout.textContent='ZOOM 100%';if(head)head.appendChild(zoomReadout);\n"
"var panel=document.createElement('div');panel.id='labSourcesPanel';panel.className='labSourcesPanel';panel.setAttribute('data-control','1');panel.innerHTML='<div class=\"labSourcesHead\"><div><div class=\"labSourcesTitle\">SOURCES / LAYERS</div><div class=\"labSourcesSub\">TOP LAYER FIRST · LOCAL BROWSER MEMORY</div></div><div class=\"labSourcesSub\">MP4 / H.264 · MAX 20 MB</div></div><div id=\"labSourcesList\"></div><button id=\"labAddSource\" class=\"labAddSource\" type=\"button\">+ ADD SOURCE</button><input id=\"labFileSource\" type=\"file\" accept=\"video/mp4,.mp4\" hidden><div id=\"labSourceStatus\" class=\"labSourceStatus\">LIVE VIEW IS THE FIXED BASE LAYER</div>';labPage.appendChild(panel);\n"
"var list=document.getElementById('labSourcesList'),add=document.getElementById('labAddSource'),picker=document.getElementById('labFileSource'),status=document.getElementById('labSourceStatus');\n"
"var MAX_LOCAL_VIDEO_BYTES=20*1024*1024,sourceSeq=0,labSources=[{id:'live',type:'live',name:'LIVE VIEW',visible:true,opacity:1,media:sourcePreview,removable:false,url:''}],sourceRaf=0,labZoom=1;\n"
"function sourceStatus(msg,kind){if(!status)return;status.textContent=msg||'';status.className='labSourceStatus'+(kind?' '+kind:'');}\n"
"function requestLabPaint(){if(pg===4||realFsElement()===labFrame||labFrame.classList.contains('pseudoFull'))requestAnimationFrame(function(){renderLabPreview();applyLabZoom();});}\n"
"function removeLayer(layer){if(!layer||!layer.removable)return;var at=labSources.indexOf(layer);if(at<0)return;try{if(layer.media){layer.media.pause();layer.media.removeAttribute('src');layer.media.load();}}catch(e){}if(layer.url)try{URL.revokeObjectURL(layer.url)}catch(e){}labSources.splice(at,1);renderSources();sourceStatus('SOURCE REMOVED','');requestLabPaint();}\n"
"function renderSources(){if(!list)return;list.innerHTML='';for(var i=labSources.length-1;i>=0;i--){(function(layer){var row=document.createElement('div');row.className='labLayerRow';var eye=document.createElement('button');eye.type='button';eye.className='labEye'+(layer.visible?'':' off');eye.innerHTML='&#128065;';eye.title=layer.visible?'Hide layer':'Show layer';eye.onclick=function(){layer.visible=!layer.visible;renderSources();requestLabPaint();};var meta=document.createElement('div');meta.className='labLayerMeta';var nm=document.createElement('div');nm.className='labLayerName';nm.textContent=layer.name;var tp=document.createElement('div');tp.className='labLayerType';tp.textContent=layer.type==='live'?'BASE / FIXED':'LOCAL VIDEO';meta.appendChild(nm);meta.appendChild(tp);var op=document.createElement('div');op.className='labOpacityWrap';var range=document.createElement('input');range.type='range';range.min='0';range.max='100';range.step='1';range.value=String(Math.round(layer.opacity*100));range.className='labOpacity';range.setAttribute('aria-label',layer.name+' opacity');var val=document.createElement('div');val.className='labOpacityValue';val.textContent=range.value+'%';range.oninput=function(){layer.opacity=Math.max(0,Math.min(1,Number(range.value)/100));val.textContent=range.value+'%';requestLabPaint();};op.appendChild(range);op.appendChild(val);var del=document.createElement('button');del.type='button';del.className='labRemove'+(layer.removable?'':' placeholder');del.innerHTML='&times;';del.title='Remove source';if(layer.removable)del.onclick=function(){removeLayer(layer);};row.appendChild(eye);row.appendChild(meta);row.appendChild(op);row.appendChild(del);list.appendChild(row);})(labSources[i]);}}\n"
"function drawContain(ctx,media,w,h){var mw=media.videoWidth||media.naturalWidth||0,mh=media.videoHeight||media.naturalHeight||0;if(!mw||!mh)return;var scale=Math.min(w/mw,h/mh),dw=mw*scale,dh=mh*scale,dx=(w-dw)/2,dy=(h-dh)/2;ctx.drawImage(media,dx,dy,dw,dh);}\n"
"renderRotatedSource=function(w,h,sw,sh,rot){var dst=fxEngine.buffers.a,ctx=dst.getContext('2d');if(!ctx)return null;fxResetContext(ctx);ctx.clearRect(0,0,w,h);ctx.imageSmoothingEnabled=true;for(var i=0;i<labSources.length;i++){var layer=labSources[i];if(!layer.visible||layer.opacity<=0)continue;ctx.save();ctx.globalAlpha=layer.opacity;if(layer.type==='live'){if(sourcePreview&&sourcePreview.complete&&sourcePreview.naturalWidth){if(rot===0){ctx.drawImage(sourcePreview,0,0,w,h);}else{ctx.translate(w/2,h/2);ctx.rotate(rot*Math.PI/180);ctx.drawImage(sourcePreview,-sw/2,-sh/2,sw,sh);}}}else if(layer.media&&layer.media.readyState>=2){drawContain(ctx,layer.media,w,h);}ctx.restore();fxResetContext(ctx);}return dst;};\n"
"var sourceOldRenderLabPreview=renderLabPreview;renderLabPreview=function(){sourceOldRenderLabPreview();var localCount=Math.max(0,labSources.length-1);if(labSourceName)labSourceName.textContent=localCount?('LIVE VIEW + '+localCount+' LOCAL'):((currentSource||'LIVE VIEW').toUpperCase());applyLabZoom();};\n"
"function hasLocalVideo(){for(var i=1;i<labSources.length;i++)if(labSources[i].type==='video')return true;return false;}\n"
"function animateSources(){sourceRaf=0;if(!hasLocalVideo())return;if(pg===4||realFsElement()===labFrame||labFrame.classList.contains('pseudoFull'))renderLabPreview();sourceRaf=requestAnimationFrame(animateSources);}\n"
"function ensureSourceAnimation(){if(!sourceRaf&&hasLocalVideo())sourceRaf=requestAnimationFrame(animateSources);}\n"
"function applyLabZoom(){labZoom=Math.max(.5,Math.min(3,labZoom));labCanvas.style.transformOrigin='50% 50%';labCanvas.style.transform='scale('+labZoom.toFixed(2)+')';if(zoomReadout)zoomReadout.textContent='ZOOM '+Math.round(labZoom*100)+'%';}\n"
"function changeLabZoom(delta){labZoom=Math.round((labZoom+delta)*10)/10;labZoom=Math.max(.5,Math.min(3,labZoom));applyLabZoom();}\n"
"if(add&&picker)add.onclick=function(){picker.click();};\n"
"if(picker)picker.addEventListener('change',function(){var file=picker.files&&picker.files[0];picker.value='';if(!file)return;if(file.size>MAX_LOCAL_VIDEO_BYTES){sourceStatus('FILE TOO LARGE · 20 MB MAX','error');return;}if(!(file.type==='video/mp4'||/\\.mp4$/i.test(file.name))){sourceStatus('USE MP4 / H.264 FOR THIS VERSION','error');return;}var url=URL.createObjectURL(file),video=document.createElement('video'),layer={id:'video'+(++sourceSeq),type:'video',name:file.name,visible:true,opacity:1,media:video,removable:true,url:url};video.muted=true;video.loop=true;video.playsInline=true;video.preload='auto';video.src=url;video.addEventListener('loadeddata',function(){sourceStatus(file.name+' READY · '+(file.size/1048576).toFixed(1)+' MB','ok');var p=video.play();if(p&&p.catch)p.catch(function(){});ensureSourceAnimation();requestLabPaint();});video.addEventListener('error',function(){sourceStatus('VIDEO COULD NOT BE DECODED · USE H.264 MP4','error');});labSources.push(layer);renderSources();sourceStatus('LOADING '+file.name,'');video.load();var p=video.play();if(p&&p.catch)p.catch(function(){});ensureSourceAnimation();});\n"
"labPage.addEventListener('wheel',function(e){if(pg!==4||realFsElement()===labFrame||labFrame.classList.contains('pseudoFull'))return;labPage.scrollTop+=e.deltaY;e.preventDefault();},{passive:false});\n"
"document.addEventListener('keydown',function(e){if(e.altKey||e.ctrlKey||e.metaKey)return;var target=e.target,tag=(target&&target.tagName?target.tagName:'').toLowerCase();if(tag==='input'||tag==='textarea'||tag==='select'||(target&&target.isContentEditable))return;var active=(pg===4||realFsElement()===labFrame||labFrame.classList.contains('pseudoFull'));if(!active)return;var k=(e.key||'').toLowerCase();if(k==='i'){e.preventDefault();changeLabZoom(.1);return;}if(k==='o'){e.preventDefault();changeLabZoom(-.1);return;}if(pg===4&&realFsElement()!==labFrame&&!labFrame.classList.contains('pseudoFull')){if(e.key==='ArrowDown'){e.preventDefault();labPage.scrollBy({top:120,behavior:'smooth'});return;}if(e.key==='ArrowUp'){e.preventDefault();labPage.scrollBy({top:-120,behavior:'smooth'});return;}}});\n"
"renderSources();applyLabZoom();\n"
"})();</script>\n";

static esp_err_t PRISMA_INTENSITY_PAGE_GET(httpd_req_t *r)
{
    (void)intensity_base_page_get;
    const char needle[]="</body>";
    const uint8_t *s=web_index_html_start,*e=web_index_html_end,*cut=NULL;
    size_t len=(size_t)(e-s),nlen=sizeof(needle)-1;
    for(size_t i=0;i+nlen<=len;i++){
        if(memcmp(s+i,needle,nlen)==0){cut=s+i;break;}
    }
    httpd_resp_set_type(r,"text/html");
    no_cache(r);
    if(!cut)return httpd_resp_send(r,(const char*)s,(ssize_t)len);
    esp_err_t err=httpd_resp_send_chunk(r,(const char*)s,(ssize_t)(cut-s));
    if(err!=ESP_OK)return err;
    err=httpd_resp_send_chunk(r,checkpoint_web_injection,HTTPD_RESP_USE_STRLEN);
    if(err!=ESP_OK)return err;
    err=httpd_resp_send_chunk(r,intensity_web_injection,HTTPD_RESP_USE_STRLEN);
    if(err!=ESP_OK)return err;
    err=httpd_resp_send_chunk(r,fxlab_sources_web_injection,HTTPD_RESP_USE_STRLEN);
    if(err!=ESP_OK)return err;
    err=httpd_resp_send_chunk(r,(const char*)cut,(ssize_t)(e-cut));
    if(err!=ESP_OK)return err;
    return httpd_resp_send_chunk(r,NULL,0);
}