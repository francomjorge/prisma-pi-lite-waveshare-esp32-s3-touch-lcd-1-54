#include "esp_http_server.h"
#include <string.h>

/*
 * V9 is additive over the physically-tested V8 implementation.
 * Rename the V8 linker wrapper, then prepend a small patch into the same
 * FX Lab IIFE. The patch initializes after V8 so it can safely reuse the
 * existing browser-local compositor, layer list, DETACH surface and recorder.
 */
#define __wrap_httpd_resp_send_chunk v8_wrapped_httpd_resp_send_chunk
#include "wifi_portal_record_v6.c"
#undef __wrap_httpd_resp_send_chunk

static const char fxlab_record_v9_patch[] =
"\n"
"/* FX LAB V9: cleanup reset + DETACH I/O zoom + 15 second recording. */\n"
"var v9DetachZoom=1,v9Resetting=false,v9ResetBtn=null;\n"
"\n"
"function v9ResetMainPreviewZoom(){\n"
"  try{\n"
"    for(var i=0;i<30;i++)document.dispatchEvent(new KeyboardEvent('keydown',{key:'o'}));\n"
"    for(var j=0;j<5;j++)document.dispatchEvent(new KeyboardEvent('keydown',{key:'i'}));\n"
"  }catch(e){}\n"
"  if(labCanvas){labCanvas.style.transformOrigin='50% 50%';labCanvas.style.transform='scale(1.00)';}\n"
"}\n"
"\n"
"function v9ReleaseLayer(layer){\n"
"  if(!layer||layer.type==='live'||layer.id==='live')return;\n"
"  try{if(layer.media){layer.media.pause();layer.media.removeAttribute('src');layer.media.load();}}catch(e){}\n"
"  if(layer.url)try{URL.revokeObjectURL(layer.url)}catch(e){}\n"
"  layer.recordedBlob=null;layer.media=null;layer.url='';\n"
"}\n"
"\n"
"function v9StopRecorderForReset(){\n"
"  v7RecClearTimers();\n"
"  try{if(v7Recorder){v7Recorder.onstop=null;v7Recorder.ondataavailable=null;if(v7Recorder.state!=='inactive')v7Recorder.stop();}}catch(e){}\n"
"  if(v7RecStream){try{var tracks=v7RecStream.getTracks();for(var i=0;i<tracks.length;i++)tracks[i].stop();}catch(e){}}\n"
"  v7Recorder=null;v7RecChunks=[];v7RecStream=null;v7RecTrack=null;v7Recording=false;\n"
"}\n"
"\n"
"function v9CleanupReset(){\n"
"  if(v9Resetting)return;v9Resetting=true;\n"
"  v9StopRecorderForReset();\n"
"  try{if(v5DetachedWindow&&!v5DetachedWindow.closed)v5DetachedWindow.close();}catch(e){}\n"
"  v5DetachedWindow=null;v7SetLocalDetached(false);v9DetachZoom=1;\n"
"\n"
"  var live=null;\n"
"  for(var i=v2Layers.length-1;i>=0;i--){var layer=v2Layers[i];if(layer&&((layer.type==='live')||(layer.id==='live')))live=layer;else v9ReleaseLayer(layer);}\n"
"  v2Layers.length=0;\n"
"  if(!live)live={id:'live',type:'live',name:'APPLIANCE IMAGEN',visible:true,opacity:1,size:1,x:0,y:0,rotation:0,cropX:0,cropY:0,cropW:1,cropH:1,trimIn:0,trimOut:0,media:sourcePreview,removable:false,url:''};\n"
"  live.name='APPLIANCE IMAGEN';live.visible=true;live.opacity=1;live.size=1;live.x=0;live.y=0;live.rotation=0;live.cropX=0;live.cropY=0;live.cropW=1;live.cropH=1;live.trimIn=0;live.trimOut=0;live.media=sourcePreview;live.removable=false;live.url='';\n"
"  v2Layers.push(live);v2Seq=0;v2Selected='';v2MenuId='';v2DragId='';v2Pointer=null;\n"
"\n"
"  v2Aspect='source';v2Resolution='source';\n"
"  if(aspectSelect)aspectSelect.value='source';if(resolutionSelect)resolutionSelect.value='source';\n"
"  try{localStorage.removeItem('prismaFxLabAspectV1');localStorage.removeItem('prismaFxLabResolutionV1');}catch(e){}\n"
"  try{var addMenu=document.getElementById('labAddMenuV5');if(addMenu)addMenu.classList.remove('open');if(picker)picker.value='';}catch(e){}\n"
"  v9ResetMainPreviewZoom();\n"
"  if(v7RecClock)v7RecClock.textContent='00.0 / 15.0';\n"
"  v2RenderList();v2Status('RESET · APPLIANCE IMAGE · LOCAL SOURCES RELEASED','ok');\n"
"  requestAnimationFrame(function(){renderLabPreview();v8FitOutputSurfaces();v7SyncRecordMirror();v9Resetting=false;});\n"
"}\n"
"\n"
"function v9DrawDetachedZoom(){\n"
"  var dc=v7DetachedCanvas();if(!dc||!labCanvas||!dc.width||!dc.height)return;\n"
"  try{\n"
"    var ctx=dc.getContext('2d'),w=dc.width,h=dc.height;if(!ctx)return;\n"
"    ctx.setTransform(1,0,0,1,0,0);ctx.globalAlpha=1;ctx.globalCompositeOperation='source-over';ctx.filter='none';ctx.clearRect(0,0,w,h);\n"
"    ctx.save();ctx.translate(w/2,h/2);ctx.scale(v9DetachZoom,v9DetachZoom);ctx.imageSmoothingEnabled=false;ctx.drawImage(labCanvas,-w/2,-h/2,w,h);ctx.restore();\n"
"    v8FitDetachedCanvas();v7SyncRecordMirror();\n"
"  }catch(e){}\n"
"}\n"
"\n"
"function v9ChangeDetachedZoom(delta){\n"
"  v9DetachZoom=Math.round((v9DetachZoom+delta)*10)/10;v9DetachZoom=Math.max(.5,Math.min(3,v9DetachZoom));v9DrawDetachedZoom();\n"
"}\n"
"\n"
"function v9BindDetachedZoom(){\n"
"  if(!v5DetachedWindow||v5DetachedWindow.closed)return;\n"
"  try{\n"
"    var win=v5DetachedWindow;if(win.__prismaV9ZoomBound)return;win.__prismaV9ZoomBound=true;\n"
"    win.addEventListener('keydown',function(ev){\n"
"      if(ev.altKey||ev.ctrlKey||ev.metaKey)return;var target=ev.target,tag=(target&&target.tagName?target.tagName:'').toLowerCase();if(tag==='input'||tag==='textarea'||tag==='select'||(target&&target.isContentEditable))return;\n"
"      var k=(ev.key||'').toLowerCase();if(k==='i'){ev.preventDefault();ev.stopPropagation();v9ChangeDetachedZoom(.1);}else if(k==='o'){ev.preventDefault();ev.stopPropagation();v9ChangeDetachedZoom(-.1);}\n"
"    },true);\n"
"  }catch(e){}\n"
"}\n"
"\n"
"function v9Init(){\n"
"  V7_REC_LIMIT_MS=15000;\n"
"  var v9BaseRecUi=v7RecUi;\n"
"  v7RecUi=function(){\n"
"    if(!v7RecBtn||!v7RecClock)return;\n"
"    if(v7Recording){var ms=Math.max(0,Math.min(V7_REC_LIMIT_MS,performance.now()-v7RecStarted));v7RecBtn.textContent='STOP ■';v7RecBtn.classList.add('recording');v7RecClock.classList.add('recording');v7RecClock.textContent=(ms/1000).toFixed(1)+' / 15.0';}\n"
"    else{v7RecBtn.textContent='REC ●';v7RecBtn.classList.remove('recording');v7RecClock.classList.remove('recording');if(!v7Recorder)v7RecClock.textContent='00.0 / 15.0';}\n"
"  };\n"
"  if(v7RecClock)v7RecClock.textContent='00.0 / 15.0';\n"
"\n"
"  var v9BaseBeginRecording=v7BeginRecording;\n"
"  v7BeginRecording=function(){v9BaseBeginRecording();if(v7Recording)v2Status('REC OUTPUT '+v7RecMirror.width+'×'+v7RecMirror.height+' · MAX 15.0 S','');};\n"
"\n"
"  var v9BaseFinishRecording=v7FinishRecording;\n"
"  v7FinishRecording=function(){if(v9Resetting){v7RecClearTimers();v7RecChunks=[];v7Recording=false;return;}v9BaseFinishRecording();};\n"
"\n"
"  var v9BasePushDetached=v5PushDetached;\n"
"  v5PushDetached=function(){v9BasePushDetached();if(v5DetachedWindow&&!v5DetachedWindow.closed){v9BindDetachedZoom();v9DrawDetachedZoom();}};\n"
"\n"
"  if(detachBtn&&detachBtn.parentNode&&!document.getElementById('labResetOutputV9')){\n"
"    v9ResetBtn=document.createElement('button');v9ResetBtn.type='button';v9ResetBtn.id='labResetOutputV9';v9ResetBtn.className=(detachBtn.className||'labDetachOutputV5')+' labResetOutputV9';v9ResetBtn.textContent='RESET';v9ResetBtn.title='Release local sources and restore appliance image';v9ResetBtn.setAttribute('data-control','1');\n"
"    detachBtn.parentNode.insertBefore(v9ResetBtn,detachBtn.nextSibling);v9ResetBtn.onclick=function(ev){ev.stopPropagation();v9CleanupReset();};\n"
"  }\n"
"}\n"
"setTimeout(v9Init,0);\n";

esp_err_t __wrap_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)
{
    if (buf && (len == HTTPD_RESP_USE_STRLEN || len < 0)) {
        const char marker[] = "v2RenderList();v2Paint();\n})();";
        if (strstr(buf, marker) != NULL) {
            esp_err_t err = __real_httpd_resp_send_chunk(r, fxlab_record_v9_patch, HTTPD_RESP_USE_STRLEN);
            if (err != ESP_OK) return err;
        }
    }
    return v8_wrapped_httpd_resp_send_chunk(r, buf, len);
}
