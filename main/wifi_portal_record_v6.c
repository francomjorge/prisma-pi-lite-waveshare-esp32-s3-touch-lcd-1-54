#include "esp_http_server.h"
#include <string.h>

/*
 * V6 is intentionally additive.  The validated V5 page remains untouched.
 * Linker --wrap catches the final chunk emitted by the V5/V2 page injector
 * immediately before the FX Lab closure and inserts this browser-only patch
 * inside the same IIFE, so it can reuse the existing layer/compositor state.
 */
extern esp_err_t __real_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len);

static const char fxlab_record_v6_patch[] =
"\n"
"/* FX LAB V6: record final output canvas, 5 s max, download + hidden source. */\n"
"var V6_REC_LIMIT_MS=5000,v6Recorder=null,v6RecChunks=[],v6RecStream=null,v6RecStopTimer=0,v6RecTick=0,v6RecStarted=0,v6RecSeq=0,v6Recording=false;\n"
"var v6BaseHasVideo=v2HasVideo;v2HasVideo=function(){return v6Recording||v6BaseHasVideo();};\n"
"function v6RecMime(){var c=['video/webm;codecs=vp9','video/webm;codecs=vp8','video/webm','video/mp4;codecs=h264','video/mp4'];if(typeof MediaRecorder==='undefined')return '';for(var i=0;i<c.length;i++){try{if(!MediaRecorder.isTypeSupported||MediaRecorder.isTypeSupported(c[i]))return c[i]}catch(e){}}return '';}\n"
"function v6RecExt(m){return String(m||'').toLowerCase().indexOf('mp4')>=0?'mp4':'webm';}\n"
"function v6Pad(n){return n<10?'0'+n:String(n);}\n"
"function v6RecStamp(){var d=new Date();return d.getFullYear()+v6Pad(d.getMonth()+1)+v6Pad(d.getDate())+'_'+v6Pad(d.getHours())+v6Pad(d.getMinutes())+v6Pad(d.getSeconds());}\n"
"var v6RecBtn=document.createElement('button');v6RecBtn.type='button';v6RecBtn.id='labRecordOutputV6';v6RecBtn.className='labRecordOutputV6';v6RecBtn.textContent='REC ●';v6RecBtn.setAttribute('data-control','1');\n"
"var v6RecClock=document.createElement('span');v6RecClock.id='labRecordClockV6';v6RecClock.className='labRecordClockV6';v6RecClock.textContent='00.0 / 05.0';\n"
"if(outputRow){outputRow.appendChild(v6RecClock);outputRow.appendChild(v6RecBtn);}\n"
"var v6Style=document.createElement('style');v6Style.textContent='.labRecordOutputV6{height:20px;border:1px solid #8c3138;border-radius:5px;background:#071019;color:#e66b72;font-size:7px;font-weight:900;letter-spacing:.5px;padding:0 7px;cursor:pointer}.labRecordOutputV6.recording{background:#3b0b10;border-color:#ff4a57;color:#fff;box-shadow:0 0 0 1px #ff4a5733 inset}.labRecordClockV6{min-width:54px;text-align:right;font-size:6.5px;font-weight:900;color:#657585;font-variant-numeric:tabular-nums}.labRecordClockV6.recording{color:#ff6672}@media(max-width:850px){.labRecordClockV6{min-width:48px}}';document.head.appendChild(v6Style);\n"
"function v6RecUi(){if(!v6RecBtn||!v6RecClock)return;if(v6Recording){var ms=Math.max(0,Math.min(V6_REC_LIMIT_MS,performance.now()-v6RecStarted));v6RecBtn.textContent='STOP ■';v6RecBtn.classList.add('recording');v6RecClock.classList.add('recording');v6RecClock.textContent=(ms/1000).toFixed(1)+' / 5.0';}else{v6RecBtn.textContent='REC ●';v6RecBtn.classList.remove('recording');v6RecClock.classList.remove('recording');if(!v6Recorder)v6RecClock.textContent='00.0 / 05.0';}}\n"
"function v6RecClearTimers(){if(v6RecStopTimer){clearTimeout(v6RecStopTimer);v6RecStopTimer=0}if(v6RecTick){clearInterval(v6RecTick);v6RecTick=0}}\n"
"function v6DownloadBlob(blob,name,url){var a=document.createElement('a');a.href=url;a.download=name;a.style.display='none';document.body.appendChild(a);try{a.click()}catch(e){}setTimeout(function(){try{a.remove()}catch(e){}},0);}\n"
"function v6AddRecordedSource(blob,name,url){var video=document.createElement('video');video.muted=true;video.loop=true;video.playsInline=true;video.preload='auto';video.src=url;var failed=false;function fail(){if(failed)return;failed=true;try{URL.revokeObjectURL(url)}catch(e){}v2Status('REC SAVED · BROWSER CANNOT RELOAD '+name,'error');}video.addEventListener('error',fail,{once:true});video.addEventListener('loadeddata',function(){if(failed)return;var duration=isFinite(video.duration)?video.duration:0,layer={id:'video'+(++v2Seq),type:'video',name:name,visible:false,opacity:1,size:1,x:0,y:0,rotation:0,cropX:0,cropY:0,cropW:1,cropH:1,trimIn:0,trimOut:duration,media:video,removable:true,url:url};v2Layers.unshift(layer);v2Selected=layer.id;v2MenuId='';v2RenderList();v2Status(name+' SAVED + ADDED · EYE OFF','ok');var p=video.play();if(p&&p.catch)p.catch(function(){});v2Paint();},{once:true});video.load();var p=video.play();if(p&&p.catch)p.catch(function(){});}\n"
"function v6FinishRecording(){v6RecClearTimers();var rec=v6Recorder,mime=(rec&&rec.mimeType)||v6RecMime()||'video/webm',chunks=v6RecChunks.slice();v6Recorder=null;v6RecChunks=[];v6Recording=false;if(v6RecStream){try{var tracks=v6RecStream.getTracks();for(var i=0;i<tracks.length;i++)tracks[i].stop()}catch(e){}v6RecStream=null;}v6RecUi();if(!chunks.length){v2Status('REC EMPTY','error');return;}var blob=new Blob(chunks,{type:mime}),ext=v6RecExt(mime),name='PRISMA_REC_'+(++v6RecSeq<10?'0':'')+v6RecSeq+'_'+v6RecStamp()+'.'+ext,url=URL.createObjectURL(blob);v6DownloadBlob(blob,name,url);v6AddRecordedSource(blob,name,url);}\n"
"function v6StopRecording(){if(!v6Recording||!v6Recorder)return;v6RecClearTimers();try{if(v6Recorder.state!=='inactive')v6Recorder.stop();else v6FinishRecording()}catch(e){v6FinishRecording();}}\n"
"function v6StartRecording(){if(v6Recording){v6StopRecording();return}if(!labCanvas||typeof labCanvas.captureStream!=='function'||typeof MediaRecorder==='undefined'){v2Status('RECORDING NOT SUPPORTED BY THIS BROWSER','error');return;}var stream=null,rec=null,mime=v6RecMime();try{stream=labCanvas.captureStream(30);rec=mime?new MediaRecorder(stream,{mimeType:mime}):new MediaRecorder(stream);}catch(e){if(stream)try{var tt=stream.getTracks();for(var i=0;i<tt.length;i++)tt[i].stop()}catch(x){}v2Status('CANNOT START CANVAS RECORDER','error');return;}v6RecStream=stream;v6Recorder=rec;v6RecChunks=[];v6Recording=true;v6RecStarted=performance.now();rec.ondataavailable=function(ev){if(ev.data&&ev.data.size)v6RecChunks.push(ev.data)};rec.onerror=function(){v2Status('RECORDING ERROR','error')};rec.onstop=v6FinishRecording;try{rec.start(250)}catch(e){v6Recording=false;v6Recorder=null;try{var tr=stream.getTracks();for(var j=0;j<tr.length;j++)tr[j].stop()}catch(x){}v2Status('CANNOT START CANVAS RECORDER','error');v6RecUi();return;}v2Status('REC OUTPUT · MAX 5.0 S','');v6RecUi();v6RecTick=setInterval(v6RecUi,100);v6RecStopTimer=setTimeout(v6StopRecording,V6_REC_LIMIT_MS);v2EnsureAnimation();}\n"
"if(v6RecBtn){v6RecBtn.onclick=function(ev){ev.stopPropagation();v6StartRecording();};if(typeof MediaRecorder==='undefined'||!labCanvas||typeof labCanvas.captureStream!=='function'){v6RecBtn.disabled=true;v6RecBtn.textContent='REC N/A';v6RecBtn.style.opacity='.35';}}\n";

esp_err_t __wrap_httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)
{
    if (buf && (len == HTTPD_RESP_USE_STRLEN || len < 0)) {
        const char marker[] = "v2RenderList();v2Paint();\n})();";
        if (strstr(buf, marker) != NULL) {
            esp_err_t err = __real_httpd_resp_send_chunk(r, fxlab_record_v6_patch, HTTPD_RESP_USE_STRLEN);
            if (err != ESP_OK) return err;
        }
    }
    return __real_httpd_resp_send_chunk(r, buf, len);
}
