#include "esp_http_server.h"
#include <string.h>

/* V11 is injected inside the existing FX Lab closure after the V10 patch. */
extern esp_err_t v10_wrapped_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len);

static const char fxlab_record_v11_patch[] =
"\n"
"/* FX LAB V11: chroma ROI + visibility-aware scheduler + lazy buffer release. */\n"
"var v11ChromaRegion=null,v11ChromaReleaseTimer=0,v11MirrorReleaseTimer=0;\n"
"\n"
"function v11ClampRegion(x,y,w,h,cw,ch){\n"
"  x=Math.max(0,Math.floor(x));y=Math.max(0,Math.floor(y));var x2=Math.min(cw,Math.ceil(x+w)),y2=Math.min(ch,Math.ceil(y+h));\n"
"  return {x:x,y:y,w:Math.max(0,x2-x),h:Math.max(0,y2-y)};\n"
"}\n"
"function v11HasChroma(){for(var i=0;i<v2Layers.length;i++){var l=v2Layers[i];if(l&&l.visible&&Number(l.opacity)>0&&l.chromaEnabled)return true;}return false;}\n"
"function v11ReleaseChromaSoon(){\n"
"  if(v11ChromaReleaseTimer)clearTimeout(v11ChromaReleaseTimer);\n"
"  if(v11HasChroma())return;\n"
"  v11ChromaReleaseTimer=setTimeout(function(){if(!v11HasChroma()&&v10ChromaCanvas){v10ChromaCanvas.width=1;v10ChromaCanvas.height=1;}v11ChromaReleaseTimer=0;},1200);\n"
"}\n"
"function v11PauseBackgroundVideos(){for(var i=0;i<v2Layers.length;i++){var l=v2Layers[i];if(l&&l.type==='video'&&l.media&&!l.media.paused)try{l.media.pause();}catch(e){}}}\n"
"function v11DetachedActive(){return !!(v5DetachedWindow&&!v5DetachedWindow.closed);}\n"
"function v11ReleaseMirrorLater(){\n"
"  if(v11MirrorReleaseTimer)clearTimeout(v11MirrorReleaseTimer);\n"
"  v11MirrorReleaseTimer=setTimeout(function(){if(!v7Recording&&!v10RecordWarmup&&v7RecMirror){v7RecMirror.width=1;v7RecMirror.height=1;}v11MirrorReleaseTimer=0;},1500);\n"
"}\n"
"\n"
"function v11Init(){\n"
"  if(typeof v10ApplyChroma!=='function'||typeof v10NeedsAnimation!=='function')return;\n"
"\n"
"  try{var fastCanvas=document.createElement('canvas'),fastCtx=fastCanvas.getContext('2d',{willReadFrequently:true});if(fastCtx){v10ChromaCanvas=fastCanvas;v10ChromaCtx=fastCtx;}}catch(e){}\n"
"\n"
"  var v11BaseApplyChroma=v10ApplyChroma;\n"
"  v10ApplyChroma=function(canvas,color,tolerance){\n"
"    var r=v11ChromaRegion;if(!r||!canvas||!canvas.width||!canvas.height)return v11BaseApplyChroma(canvas,color,tolerance);\n"
"    r=v11ClampRegion(r.x,r.y,r.w,r.h,canvas.width,canvas.height);if(!r.w||!r.h)return canvas;\n"
"    var ctx=(canvas===v10ChromaCanvas&&v10ChromaCtx)?v10ChromaCtx:canvas.getContext('2d',{willReadFrequently:true});if(!ctx)return canvas;\n"
"    var key=v10HexRgb(color),tol=Math.max(0,Math.min(100,Number(tolerance)||0))*4.42,t2=tol*tol,img;\n"
"    try{img=ctx.getImageData(r.x,r.y,r.w,r.h);}catch(e){return canvas;}\n"
"    var d=img.data;for(var i=0;i<d.length;i+=4){if(!d[i+3])continue;var dr=d[i]-key[0],dg=d[i+1]-key[1],db=d[i+2]-key[2];if(dr*dr+dg*dg+db*db<=t2)d[i+3]=0;}\n"
"    ctx.putImageData(img,r.x,r.y);return canvas;\n"
"  };\n"
"\n"
"  var v11BaseDrawLayer=v2DrawLayer;\n"
"  v2DrawLayer=function(ctx,layer,w,h,sw,sh,rot){\n"
"    if(layer&&layer.chromaEnabled){var g=v2LayerGeometry(layer,w,h,rot),pad=6;v11ChromaRegion=v11ClampRegion(g.x-pad,g.y-pad,g.w+pad*2,g.h+pad*2,w,h);if(v11ChromaReleaseTimer){clearTimeout(v11ChromaReleaseTimer);v11ChromaReleaseTimer=0;}}else v11ChromaRegion=null;\n"
"    try{return v11BaseDrawLayer(ctx,layer,w,h,sw,sh,rot);}finally{v11ChromaRegion=null;}\n"
"  };\n"
"\n"
"  v2HasVideo=function(){\n"
"    if(v7Recording||v10RecordWarmup)return true;\n"
"    if(document.hidden&&!v11DetachedActive()){v11PauseBackgroundVideos();return false;}\n"
"    var active=false;for(var i=0;i<v2Layers.length;i++){var l=v2Layers[i];if(l&&l.type==='video')v10SyncLayerPlayback(l);if(v10NeedsAnimation(l))active=true;}return active;\n"
"  };\n"
"\n"
"  document.addEventListener('visibilitychange',function(){\n"
"    if(document.hidden&&!v11DetachedActive()){v11PauseBackgroundVideos();return;}\n"
"    v10SyncAllPlayback();v2EnsureAnimation();v2Paint();\n"
"  });\n"
"\n"
"  var v11BaseRenderList=v2RenderList;\n"
"  v2RenderList=function(){var out=v11BaseRenderList();v11ReleaseChromaSoon();return out;};\n"
"\n"
"  v10IdleExport=function(fn){\n"
"    var fired=false,once=function(){if(fired)return;fired=true;fn();};\n"
"    if(typeof requestIdleCallback==='function')requestIdleCallback(once,{timeout:1200});else setTimeout(once,180);\n"
"  };\n"
"\n"
"  var v11BaseFinishRecording=v7FinishRecording;\n"
"  v7FinishRecording=function(){var out=v11BaseFinishRecording();v11ReleaseMirrorLater();return out;};\n"
"\n"
"  if(typeof v9CleanupReset==='function'){var v11BaseReset=v9CleanupReset;v9CleanupReset=function(){var out=v11BaseReset();v11ReleaseChromaSoon();v11ReleaseMirrorLater();return out;};}\n"
"\n"
"  v11ReleaseChromaSoon();v2EnsureAnimation();\n"
"}\n"
"setTimeout(function(){setTimeout(v11Init,25);},0);\n";

esp_err_t __wrap_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)
{
    if (buf && (len == HTTPD_RESP_USE_STRLEN || len < 0)) {
        const char marker[] = "v2RenderList();v2Paint();\n})();";
        if (strstr(buf, marker) != NULL) {
            esp_err_t err = __real_httpd_resp_send_chunk(r, fxlab_record_v11_patch, HTTPD_RESP_USE_STRLEN);
            if (err != ESP_OK) return err;
        }
    }
    return v10_wrapped_httpd_resp_send_chunk(r, buf, len);
}
