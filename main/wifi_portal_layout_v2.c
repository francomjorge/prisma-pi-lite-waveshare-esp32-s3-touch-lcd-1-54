#include "esp_http_server.h"
#include <string.h>

static esp_err_t layout_v2_page_get(httpd_req_t *r);

/* Keep the stable intensity/checkpoint wrapper intact. Intercept only root. */
static esp_err_t layout_v2_register_uri(httpd_handle_t h, const httpd_uri_t *u)
{
    if (u && u->uri && strcmp(u->uri, "/") == 0 && u->method == HTTP_GET) {
        httpd_uri_t copy = *u;
        copy.handler = layout_v2_page_get;
        return httpd_register_uri_handler(h, &copy);
    }
    return httpd_register_uri_handler(h, u);
}

#define httpd_register_uri_handler(...) layout_v2_register_uri(__VA_ARGS__)
#include "wifi_portal_intensity.c"
#undef httpd_register_uri_handler

/* Browser-only FX Lab composition console. Physical appliance UI is untouched. */
static const char fxlab_layout_v2_injection[] =
"\n"
"<style>\n"
".labWorkbenchV2{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px;align-items:stretch;margin-bottom:12px}\n"
".labWorkbenchV2 .labFrame{grid-column:1/3;height:260px!important;min-height:260px}\n"
".labSourcesPanelV2{grid-column:3;height:260px;background:#0c1520;border:1px solid #203040;border-radius:16px;padding:8px;display:flex;flex-direction:column;min-width:0}\n"
".labSourcesPanelV2Head{height:26px;display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:5px}.labSourcesPanelV2Title{min-width:0;font-size:11px;font-weight:900;letter-spacing:1.2px;white-space:nowrap}.labSourcesPanelV2Count{font-size:7px;color:#657585;font-weight:800;letter-spacing:.5px;margin-left:5px}.labAddSourceV2{width:24px;height:24px;min-width:24px;padding:0;border:1px solid #23dcdc;background:#071019;color:#23dcdc;border-radius:7px;font-size:17px;line-height:20px;font-weight:700;cursor:pointer}\n"
".labSourcesListV2{flex:1;min-height:0;overflow-y:auto;padding-right:2px}.labSourcesListV2::-webkit-scrollbar{width:5px}.labSourcesListV2::-webkit-scrollbar-thumb{background:#2d3c4b;border-radius:5px}\n"
".labLayerV2{height:27px;display:grid;grid-template-columns:14px 18px minmax(0,1fr) 64px 28px 18px;gap:4px;align-items:center;border:1px solid #233548;border-radius:7px;background:#09111a;margin-bottom:3px;padding:0 5px;cursor:pointer;user-select:none;-webkit-user-select:none;box-sizing:border-box}.labLayerV2.selected{border-color:#23dcdc}.labLayerV2.dragging{opacity:.38}.labLayerV2.dragBefore{border-top:2px solid #af37ff}.labLayerV2.dragAfter{border-bottom:2px solid #af37ff}\n"
".labDragV2{font-size:12px;line-height:1;color:#657585;text-align:center;cursor:grab}.labEyeV2,.labDeleteV2{width:18px;height:18px;border:1px solid #2b4054;border-radius:5px;background:#071019;color:#f0f5fa;padding:0;cursor:pointer;display:flex;align-items:center;justify-content:center}.labEyeV2{font-size:10px}.labEyeV2.off{opacity:.3}.labDeleteV2{font-size:12px;line-height:1;color:#e66b72}.labDeleteV2.fixed{visibility:hidden}.labLayerV2Name{min-width:0;font-size:9px;font-weight:900;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.labLayerV2Opacity{width:64px;height:12px;margin:0;accent-color:#23dcdc;cursor:pointer}.labLayerV2Pct{text-align:right;font-size:7px;font-weight:900;color:#9fb0bf;font-variant-numeric:tabular-nums}\n"
".labSourceStatusV2{height:11px;min-height:11px;margin-top:3px;text-align:center;font-size:6.5px;line-height:11px;letter-spacing:.45px;color:#657585;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.labSourceStatusV2.ok{color:#23c86e}.labSourceStatusV2.error{color:#e63741}\n"
"@media(max-width:850px){.labWorkbenchV2{grid-template-columns:1fr}.labWorkbenchV2 .labFrame{grid-column:1;height:240px!important;min-height:240px}.labSourcesPanelV2{grid-column:1;height:220px}.labLayerV2{grid-template-columns:14px 18px minmax(0,1fr) 74px 28px 18px}}\n"
"</style>\n"
"<script>(function(){\n"
"var labPage=document.querySelector('.labPage');if(!labPage||!labFrame||!labCanvas)return;\n"
"var oldPanel=document.getElementById('labSourcesPanel');if(oldPanel)oldPanel.style.display='none';\n"
"var pipeHead=labPage.querySelector('.labPipeHead');var work=document.createElement('div');work.id='labWorkbenchV2';work.className='labWorkbenchV2';labPage.insertBefore(work,pipeHead||labPage.firstChild);work.appendChild(labFrame);\n"
"var panel=document.createElement('div');panel.id='labSourcesPanelV2';panel.className='labSourcesPanelV2';panel.setAttribute('data-control','1');panel.innerHTML='<div class=\"labSourcesPanelV2Head\"><div class=\"labSourcesPanelV2Title\">SOURCES / LAYERS <span id=\"labSourcesCountV2\" class=\"labSourcesPanelV2Count\">1</span></div><button id=\"labAddSourceV2\" class=\"labAddSourceV2\" type=\"button\" title=\"Add source\">+</button></div><div id=\"labSourcesListV2\" class=\"labSourcesListV2\"></div><input id=\"labFileSourceV2\" type=\"file\" accept=\"video/*,.mp4,.mov,.m4v,.webm,.3gp\" hidden><div id=\"labSourceStatusV2\" class=\"labSourceStatusV2\">LIVE VIEW FIXED · DRAG = ORDER</div>';work.appendChild(panel);\n"
"var list=document.getElementById('labSourcesListV2'),add=document.getElementById('labAddSourceV2'),picker=document.getElementById('labFileSourceV2'),status=document.getElementById('labSourceStatusV2'),count=document.getElementById('labSourcesCountV2');\n"
"var V2_MAX_BYTES=50*1024*1024,v2Seq=0,v2DragId='',v2Selected='live',v2Raf=0;\n"
"var v2Layers=[{id:'live',type:'live',name:'LIVE VIEW',visible:true,opacity:1,media:sourcePreview,removable:false,url:''}];\n"
"function v2Status(msg,kind){if(!status)return;status.textContent=msg||'';status.className='labSourceStatusV2'+(kind?' '+kind:'');}\n"
"function v2LayerById(id){for(var i=0;i<v2Layers.length;i++)if(v2Layers[i].id===id)return v2Layers[i];return null;}\n"
"function v2Select(id){if(v2LayerById(id)){v2Selected=id;v2RenderList();v2Paint();}}\n"
"function v2ClearDragMarks(){if(!list)return;var rows=list.querySelectorAll('.labLayerV2');for(var i=0;i<rows.length;i++){rows[i].classList.remove('dragBefore');rows[i].classList.remove('dragAfter');}}\n"
"function v2Move(fromId,toId,after){if(!fromId||!toId||fromId===toId)return;var a=-1,b=-1;for(var i=0;i<v2Layers.length;i++){if(v2Layers[i].id===fromId)a=i;if(v2Layers[i].id===toId)b=i;}if(a<0||b<0)return;var item=v2Layers.splice(a,1)[0];if(a<b)b--;if(after)b++;if(b<0)b=0;if(b>v2Layers.length)b=v2Layers.length;v2Layers.splice(b,0,item);v2RenderList();v2Paint();}\n"
"function v2Remove(layer){if(!layer||!layer.removable)return;var at=v2Layers.indexOf(layer);if(at<0)return;try{if(layer.media){layer.media.pause();layer.media.removeAttribute('src');layer.media.load();}}catch(e){}if(layer.url)try{URL.revokeObjectURL(layer.url)}catch(e){}v2Layers.splice(at,1);if(v2Selected===layer.id)v2Selected=v2Layers.length?v2Layers[0].id:'live';v2RenderList();v2Status('SOURCE REMOVED','');v2Paint();}\n"
"function v2RenderList(){if(!list)return;list.innerHTML='';if(count)count.textContent=String(v2Layers.length);for(var i=0;i<v2Layers.length;i++){(function(layer){var row=document.createElement('div');row.className='labLayerV2'+(v2Selected===layer.id?' selected':'');row.draggable=true;row.dataset.layerId=layer.id;row.title=(layer.type==='live'?'APPLIANCE / LIVE VIEW':'LOCAL VIDEO')+' · drag to reorder';row.addEventListener('click',function(){v2Select(layer.id)});row.addEventListener('dragstart',function(ev){v2DragId=layer.id;row.classList.add('dragging');v2ClearDragMarks();try{ev.dataTransfer.effectAllowed='move';ev.dataTransfer.setData('text/plain',layer.id)}catch(e){}});row.addEventListener('dragend',function(){v2DragId='';row.classList.remove('dragging');v2ClearDragMarks()});row.addEventListener('dragover',function(ev){ev.preventDefault();ev.stopPropagation();v2ClearDragMarks();var rect=row.getBoundingClientRect(),after=ev.clientY>rect.top+rect.height/2;row.dataset.dropAfter=after?'1':'0';row.classList.add(after?'dragAfter':'dragBefore');try{ev.dataTransfer.dropEffect='move'}catch(e){}});row.addEventListener('dragleave',function(){row.classList.remove('dragBefore');row.classList.remove('dragAfter')});row.addEventListener('drop',function(ev){ev.preventDefault();ev.stopPropagation();var from=v2DragId;try{if(!from)from=ev.dataTransfer.getData('text/plain')}catch(e){}var rect=row.getBoundingClientRect(),after=ev.clientY>rect.top+rect.height/2;v2ClearDragMarks();v2Move(from,layer.id,after)});var drag=document.createElement('div');drag.className='labDragV2';drag.textContent='≡';drag.title='Drag layer';var eye=document.createElement('button');eye.type='button';eye.className='labEyeV2'+(layer.visible?'':' off');eye.innerHTML='&#128065;';eye.title=layer.visible?'Hide layer':'Show layer';eye.onclick=function(ev){ev.stopPropagation();layer.visible=!layer.visible;v2RenderList();v2Paint();};var nm=document.createElement('div');nm.className='labLayerV2Name';nm.textContent=layer.name;var range=document.createElement('input');range.className='labLayerV2Opacity';range.type='range';range.min='0';range.max='100';range.step='1';range.value=String(Math.round(layer.opacity*100));range.setAttribute('aria-label',layer.name+' opacity');var pct=document.createElement('div');pct.className='labLayerV2Pct';pct.textContent=range.value+'%';range.oninput=function(ev){ev.stopPropagation();layer.opacity=Math.max(0,Math.min(1,Number(range.value)/100));pct.textContent=range.value+'%';v2Paint();};range.onclick=function(ev){ev.stopPropagation();v2Select(layer.id)};var del=document.createElement('button');del.type='button';del.className='labDeleteV2'+(layer.removable?'':' fixed');del.innerHTML='&times;';del.title='Remove source';if(layer.removable)del.onclick=function(ev){ev.stopPropagation();v2Remove(layer)};row.appendChild(drag);row.appendChild(eye);row.appendChild(nm);row.appendChild(range);row.appendChild(pct);row.appendChild(del);list.appendChild(row);})(v2Layers[i]);}}\n"
"list.addEventListener('dragover',function(ev){if(ev.target!==list)return;ev.preventDefault();try{ev.dataTransfer.dropEffect='move'}catch(e){}});\n"
"list.addEventListener('drop',function(ev){if(ev.target!==list)return;ev.preventDefault();var from=v2DragId;try{if(!from)from=ev.dataTransfer.getData('text/plain')}catch(e){}if(!from||!v2Layers.length)return;v2Move(from,v2Layers[v2Layers.length-1].id,true)});\n"
"function v2DrawContain(ctx,media,w,h){var mw=media.videoWidth||media.naturalWidth||0,mh=media.videoHeight||media.naturalHeight||0;if(!mw||!mh)return;var sc=Math.min(w/mw,h/mh),dw=mw*sc,dh=mh*sc;ctx.drawImage(media,(w-dw)/2,(h-dh)/2,dw,dh);}\n"
"renderRotatedSource=function(w,h,sw,sh,rot){var dst=fxEngine.buffers.a,ctx=dst.getContext('2d');if(!ctx)return null;fxResetContext(ctx);ctx.clearRect(0,0,w,h);ctx.imageSmoothingEnabled=true;for(var i=v2Layers.length-1;i>=0;i--){var layer=v2Layers[i];if(!layer.visible||layer.opacity<=0)continue;ctx.save();ctx.globalAlpha=layer.opacity;if(layer.type==='live'){if(sourcePreview&&sourcePreview.complete&&sourcePreview.naturalWidth){if(rot===0){ctx.drawImage(sourcePreview,0,0,w,h)}else{ctx.translate(w/2,h/2);ctx.rotate(rot*Math.PI/180);ctx.drawImage(sourcePreview,-sw/2,-sh/2,sw,sh)}}}else if(layer.media&&layer.media.readyState>=2){v2DrawContain(ctx,layer.media,w,h)}ctx.restore();fxResetContext(ctx);}return dst;};\n"
"var v2OldRender=renderLabPreview;renderLabPreview=function(){v2OldRender();var s=v2LayerById(v2Selected);if(labSourceName)labSourceName.textContent=(s?s.name:'COMPOSITE')+' · '+v2Layers.length+(v2Layers.length===1?' LAYER':' LAYERS');};\n"
"function v2HasVideo(){for(var i=0;i<v2Layers.length;i++)if(v2Layers[i].type==='video')return true;return false;}\n"
"function v2Animate(){v2Raf=0;if(!v2HasVideo())return;if(pg===4||realFsElement()===labFrame||labFrame.classList.contains('pseudoFull'))renderLabPreview();v2Raf=requestAnimationFrame(v2Animate);}\n"
"function v2EnsureAnimation(){if(!v2Raf&&v2HasVideo())v2Raf=requestAnimationFrame(v2Animate);}\n"
"function v2Paint(){if(pg===4||realFsElement()===labFrame||labFrame.classList.contains('pseudoFull'))requestAnimationFrame(renderLabPreview);}\n"
"if(add&&picker)add.onclick=function(){picker.click();};\n"
"if(picker)picker.addEventListener('change',function(){var file=picker.files&&picker.files[0];picker.value='';if(!file)return;if(file.size>V2_MAX_BYTES){v2Status('FILE TOO LARGE · 50 MB MAX','error');return;}var url=URL.createObjectURL(file),video=document.createElement('video');video.muted=true;video.loop=true;video.playsInline=true;video.preload='auto';video.src=url;var failed=false;function fail(){if(failed)return;failed=true;try{URL.revokeObjectURL(url)}catch(e){}v2Status('BROWSER CANNOT DECODE '+file.name,'error');}video.addEventListener('error',fail,{once:true});video.addEventListener('loadeddata',function(){if(failed)return;var layer={id:'video'+(++v2Seq),type:'video',name:file.name,visible:true,opacity:1,media:video,removable:true,url:url};v2Layers.unshift(layer);v2Selected=layer.id;v2RenderList();v2Status(file.name+' READY · '+(file.size/1048576).toFixed(1)+' MB','ok');var p=video.play();if(p&&p.catch)p.catch(function(){});v2EnsureAnimation();v2Paint();},{once:true});v2Status('TESTING BROWSER DECODER · '+file.name,'');video.load();var p=video.play();if(p&&p.catch)p.catch(function(){});});\n"
"window.addEventListener('beforeunload',function(){for(var i=0;i<v2Layers.length;i++)if(v2Layers[i].url)try{URL.revokeObjectURL(v2Layers[i].url)}catch(e){}});\n"
"v2RenderList();v2Paint();\n"
"})();</script>\n";

static esp_err_t layout_v2_page_get(httpd_req_t *r)
{
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
    err=httpd_resp_send_chunk(r,fxlab_layout_v2_injection,HTTPD_RESP_USE_STRLEN);
    if(err!=ESP_OK)return err;
    err=httpd_resp_send_chunk(r,(const char*)cut,(ssize_t)(e-cut));
    if(err!=ESP_OK)return err;
    return httpd_resp_send_chunk(r,NULL,0);
}
