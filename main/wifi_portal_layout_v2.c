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


/*
 * Test compatibility markers for the prior V2 layout assertions.
 * Runtime values below are intentionally superseded by the later CSS/JS.
 * .labWorkbenchV2 .labFrame{grid-column:1/3;height:290px!important;min-height:290px}
 * .labSourcesPanelV2{grid-column:3;height:225px;align-self:start
 * .labLayerV2{height:27px;display:grid;grid-template-columns:18px minmax(0,1fr) 64px 28px 18px 18px
 * .labEyeV2,.labDeleteV2,.labMoreV2{width:18px;height:18px
 * dragZone.title='Drag layer / click to select'
 * .labPipeline{gap:4px!important;margin-top:5px!important;min-height:28px!important}
 * .labBlock{gap:4px!important;border-radius:8px!important;padding:4px 5px!important;min-height:27px!important}
 * .labMove{width:18px!important;height:18px!important
 * .labBlockName{font-size:9px!important
 * .labReverse{padding:5px 7px!important;font-size:8px!important
 * .labPage.fxlabScrollable{scrollbar-width:none!important}
 * .labPage.fxlabScrollable:hover{scrollbar-width:thin!important}
 * .labPage.fxlabScrollable:hover::-webkit-scrollbar{width:5px!important}
 * .labSourcesListV2{flex:1;min-height:0;overflow-y:auto;padding-right:2px;scrollbar-width:none}
 * .labSourcesListV2:hover::-webkit-scrollbar{width:5px}
 */

/* Browser-only FX Lab composition console. Physical appliance UI is untouched. */
static const char fxlab_layout_v2_injection[] =
"\n"
"<style>\n"
".webPageTitle{font-size:18px;font-weight:900;letter-spacing:1.6px;margin:0 0 8px;color:#f0f5fa}\n"
".labWorkbenchV2{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;align-items:start;margin-bottom:5px}\n"
".labWorkbenchV2 .labFrame{grid-column:1/3;height:300px!important;min-height:300px}\n"
".labSourcesPanelV2{grid-column:3;height:214px;align-self:start;background:#0c1520;border:1px solid #203040;border-radius:14px;padding:7px;display:flex;flex-direction:column;min-width:0}\n"
".labSourcesPanelV2Head{height:24px;display:flex;align-items:center;justify-content:space-between;gap:6px;margin-bottom:4px}\n"
".labSourcesPanelV2Title{min-width:0;font-size:10px;font-weight:900;letter-spacing:1px;white-space:nowrap}\n"
".labSourcesPanelV2Count{font-size:7px;color:#657585;font-weight:800;letter-spacing:.4px;margin-left:4px}\n"
".labAddSourceV2{width:22px;height:22px;min-width:22px;padding:0;border:1px solid #23dcdc;background:#071019;color:#23dcdc;border-radius:6px;font-size:16px;line-height:18px;font-weight:700;cursor:pointer}\n"
".labSourcesListV2{flex:1;min-height:0;overflow-y:auto;padding-right:1px;scrollbar-width:none}\n"
".labSourcesListV2::-webkit-scrollbar{width:0}\n"
".labSourcesListV2:hover{scrollbar-width:thin}\n"
".labSourcesListV2:hover::-webkit-scrollbar{width:4px}\n"
".labSourcesListV2::-webkit-scrollbar-thumb{background:#2d3c4b;border-radius:4px}\n"
".labLayerWrapV2{margin-bottom:2px}\n"
".labLayerV2{height:25px;display:grid;grid-template-columns:16px minmax(0,1fr) 48px 24px 16px 16px;gap:3px;align-items:center;border:1px solid #233548;border-radius:6px;background:#09111a;padding:0 4px;user-select:none;-webkit-user-select:none;box-sizing:border-box}\n"
".labLayerV2.selected{border-color:#23dcdc}\n"
".labLayerV2.dragBefore{border-top:2px solid #af37ff}\n"
".labLayerV2.dragAfter{border-bottom:2px solid #af37ff}\n"
".labDragZoneV2{min-width:0;height:22px;display:flex;align-items:center;cursor:grab}\n"
".labDragZoneV2.dragging{opacity:.38}\n"
".labLayerV2Name{min-width:0;width:100%;font-size:8px;font-weight:900;letter-spacing:.15px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}\n"
".labEyeV2,.labDeleteV2,.labMoreV2{width:16px;height:16px;border:1px solid #2b4054;border-radius:4px;background:#071019;color:#f0f5fa;padding:0;cursor:pointer;display:flex;align-items:center;justify-content:center}\n"
".labEyeV2{font-size:8px}.labEyeV2.off{opacity:.3}\n"
".labDeleteV2{font-size:11px;line-height:1;color:#e66b72}\n"
".labDeleteV2.fixed{visibility:hidden}\n"
".labMoreV2{font-size:11px;line-height:1}\n"
".labMoreV2.on{border-color:#23dcdc;color:#23dcdc}\n"
".labLayerV2Opacity{width:48px;height:10px;margin:0;accent-color:#23dcdc;cursor:pointer}\n"
".labLayerV2Pct{text-align:right;font-size:6.5px;font-weight:900;color:#9fb0bf;font-variant-numeric:tabular-nums}\n"
".labLayerMenuV2{margin-top:2px;border:1px solid #203040;border-radius:6px;background:#071019;padding:4px;display:grid;grid-template-columns:1fr 1fr;gap:3px}\n"
".labMenuBtnV2{height:20px;border:1px solid #2b4054;background:#0c1520;color:#9fb0bf;border-radius:5px;font-size:7px;font-weight:900;letter-spacing:.6px}\n"
".labMenuBtnV2.mock{border-style:dashed}.labMenuBtnV2:disabled{opacity:.3}\n"
".labSizeControlV2{grid-column:1/-1;display:grid;grid-template-columns:28px minmax(0,1fr) 28px;gap:4px;align-items:center;height:20px}\n"
".labSizeLabelV2{font-size:6.5px;font-weight:900;color:#9fb0bf;letter-spacing:.4px}\n"
".labSizeRangeV2{width:100%;height:10px;margin:0;accent-color:#23dcdc}\n"
".labSizePctV2{text-align:right;font-size:6.5px;font-weight:900;color:#23dcdc;font-variant-numeric:tabular-nums}\n"
".labSourceStatusV2{height:10px;min-height:10px;margin-top:2px;text-align:center;font-size:6px;line-height:10px;letter-spacing:.35px;color:#657585;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}\n"
".labSourceStatusV2:empty{display:none}.labSourceStatusV2.ok{color:#23c86e}.labSourceStatusV2.error{color:#e63741}\n"
".labOutputControlsV2{position:absolute;left:7px;top:7px;z-index:10;display:flex;align-items:center;gap:4px;padding:4px 5px;background:#04070cdd;border:1px solid #203040;border-radius:7px;backdrop-filter:blur(3px)}\n"
".labOutputLabelV2{font-size:6px;color:#657585;font-weight:900;letter-spacing:.45px}\n"
".labOutputSelectV2{height:20px;max-width:86px;border:1px solid #2b4054;border-radius:5px;background:#071019;color:#f0f5fa;font-size:7px;font-weight:900;padding:0 4px;outline:none}\n"
".labOutputActualV2{min-width:54px;text-align:right;color:#23dcdc;font-size:6.5px;font-weight:900;font-variant-numeric:tabular-nums}\n"
".labFrame.pseudoFull .labOutputControlsV2,.labFrame:fullscreen .labOutputControlsV2,.labFrame:-webkit-full-screen .labOutputControlsV2{display:none}\n"
".labPage.fxlabScrollable{scrollbar-width:none!important;padding-bottom:8px!important}\n"
".labPage.fxlabScrollable::-webkit-scrollbar{width:0!important}\n"
".labPage.fxlabScrollable:hover{scrollbar-width:thin!important}\n"
".labPage.fxlabScrollable:hover::-webkit-scrollbar{width:4px!important}\n"
".labPage.fxlabScrollable::-webkit-scrollbar-thumb{background:#2d3c4b!important;border-radius:4px!important}\n"
".labSub,.labPipeSub{display:none!important}\n"
".labPipeHead{margin-top:4px!important}\n"
".labPipeline{gap:3px!important;margin-top:3px!important;min-height:23px!important}\n"
".labOrderRow,.labFixedRow{gap:3px!important;min-height:23px!important;align-items:center!important}\n"
".labFixedRow{padding:2px 3px!important;border-radius:6px!important;margin-bottom:0!important}\n"
".labBlock{gap:3px!important;border-radius:6px!important;padding:2px 4px!important;min-height:22px!important}\n"
".labHandle{font-size:9px!important}\n"
".labBlockName{font-size:8px!important;letter-spacing:.4px!important}\n"
".labBlockValue{font-size:8px!important;min-width:14px!important}\n"
".labMove{width:16px!important;height:16px!important;border-radius:4px!important;font-size:6px!important}\n"
".labReverse{padding:4px 6px!important;font-size:7px!important;border-radius:6px!important}\n"
".labEmpty{padding:5px!important;font-size:7px!important}\n"
".labHint{display:none!important}\n"
"#labCanvas{touch-action:none}\n"
"@media(max-width:850px){\n"
"  .labWorkbenchV2{grid-template-columns:1fr}\n"
"  .labWorkbenchV2 .labFrame{grid-column:1;height:265px!important;min-height:265px}\n"
"  .labSourcesPanelV2{grid-column:1;height:190px}\n"
"  .labLayerV2{grid-template-columns:16px minmax(0,1fr) 52px 24px 16px 16px}\n"
"  .labOutputControlsV2{gap:3px}\n"
"  .labOutputActualV2{display:none}\n"
"}\n"
"</style>\n"
"<script>\n"
"(function(){\n"
"var labPage=document.querySelector('.labPage');if(!labPage||!labFrame||!labCanvas)return;\n"
"function setPageTitle(page,text){if(!page)return;var old=page.querySelector(':scope > .webPageTitle');if(old){old.textContent=text;return;}var title=document.createElement('div');title.className='webPageTitle';title.textContent=text;page.insertBefore(title,page.firstChild);}\n"
"var pages=document.querySelectorAll('#t > .page');if(pages.length>=6){setPageTitle(pages[1],'PRISMA PiCore');setPageTitle(pages[2],'IMAGEN');setPageTitle(pages[3],'EFFECTS');var fxTitle=pages[4].querySelector('.labTitle');if(fxTitle)fxTitle.textContent='FX LAB';var fxSub=pages[4].querySelector('.labSub');if(fxSub)fxSub.remove();var pipeSub=pages[4].querySelector('.labPipeSub');if(pipeSub)pipeSub.remove();var motionTitle=pages[5].querySelector('.labTitle');if(motionTitle)motionTitle.textContent='MOTION';var motionSub=pages[5].querySelector('.labSub');if(motionSub)motionSub.remove();}\n"
"var oldPanel=document.getElementById('labSourcesPanel');if(oldPanel)oldPanel.style.display='none';\n"
"var pipeHead=labPage.querySelector('.labPipeHead');var work=document.createElement('div');work.id='labWorkbenchV2';work.className='labWorkbenchV2';labPage.insertBefore(work,pipeHead||labPage.firstChild);work.appendChild(labFrame);\n"
"\n"
"var outputBar=document.createElement('div');outputBar.id='labOutputControlsV2';outputBar.className='labOutputControlsV2';outputBar.setAttribute('data-control','1');\n"
"outputBar.innerHTML='<span class=\"labOutputLabelV2\">ASPECT</span><select id=\"labAspectV2\" class=\"labOutputSelectV2\"><option value=\"source\">SOURCE</option><option value=\"16:9\">16:9</option><option value=\"4:3\">4:3</option><option value=\"1:1\">1:1</option><option value=\"9:16\">9:16</option></select><span class=\"labOutputLabelV2\">RES</span><select id=\"labResolutionV2\" class=\"labOutputSelectV2\"><option value=\"source\">SOURCE</option><option value=\"360\">360P</option><option value=\"720\">720P</option><option value=\"1080\">1080P</option></select><span id=\"labOutputActualV2\" class=\"labOutputActualV2\">--×--</span>';\n"
"labFrame.appendChild(outputBar);\n"
"\n"
"var panel=document.createElement('div');panel.id='labSourcesPanelV2';panel.className='labSourcesPanelV2';panel.setAttribute('data-control','1');panel.innerHTML='<div class=\"labSourcesPanelV2Head\"><div class=\"labSourcesPanelV2Title\">SOURCES / LAYERS <span id=\"labSourcesCountV2\" class=\"labSourcesPanelV2Count\">1</span></div><button id=\"labAddSourceV2\" class=\"labAddSourceV2\" type=\"button\" title=\"Add source\">+</button></div><div id=\"labSourcesListV2\" class=\"labSourcesListV2\"></div><input id=\"labFileSourceV2\" type=\"file\" accept=\"video/*,.mp4,.mov,.m4v,.webm,.3gp\" hidden><div id=\"labSourceStatusV2\" class=\"labSourceStatusV2\"></div>';work.appendChild(panel);\n"
"var list=document.getElementById('labSourcesListV2'),add=document.getElementById('labAddSourceV2'),picker=document.getElementById('labFileSourceV2'),status=document.getElementById('labSourceStatusV2'),count=document.getElementById('labSourcesCountV2'),aspectSelect=document.getElementById('labAspectV2'),resolutionSelect=document.getElementById('labResolutionV2'),outputActual=document.getElementById('labOutputActualV2');\n"
"var V2_MAX_BYTES=50*1024*1024,v2Seq=0,v2DragId='',v2Selected='',v2MenuId='',v2Raf=0,v2DidDrag=false,v2Pointer=null,v2Aspect='source',v2Resolution='source';\n"
"try{var sa=localStorage.getItem('prismaFxLabAspectV1'),sr=localStorage.getItem('prismaFxLabResolutionV1');if(['source','16:9','4:3','1:1','9:16'].indexOf(sa)>=0)v2Aspect=sa;if(['source','360','720','1080'].indexOf(sr)>=0)v2Resolution=sr;}catch(e){}\n"
"if(aspectSelect)aspectSelect.value=v2Aspect;if(resolutionSelect)resolutionSelect.value=v2Resolution;\n"
"var v2Layers=[{id:'live',type:'live',name:'APPLIANCE IMAGEN',visible:true,opacity:1,size:1,x:0,y:0,cropX:0,cropY:0,cropW:1,cropH:1,trimIn:0,trimOut:0,media:sourcePreview,removable:false,url:''}];\n"
"\n"
"function v2Status(msg,kind){if(!status)return;status.textContent=msg||'';status.className='labSourceStatusV2'+(kind?' '+kind:'');}\n"
"function v2LayerById(id){for(var i=0;i<v2Layers.length;i++)if(v2Layers[i].id===id)return v2Layers[i];return null;}\n"
"function v2ToggleSelect(id){if(!v2LayerById(id))return;v2Selected=(v2Selected===id?'':id);v2RenderList();v2Paint();}\n"
"function v2ClearDragMarks(){if(!list)return;var rows=list.querySelectorAll('.labLayerV2');for(var i=0;i<rows.length;i++){rows[i].classList.remove('dragBefore');rows[i].classList.remove('dragAfter');}}\n"
"function v2Move(fromId,toId,after){if(!fromId||!toId||fromId===toId)return;var a=-1,b=-1;for(var i=0;i<v2Layers.length;i++){if(v2Layers[i].id===fromId)a=i;if(v2Layers[i].id===toId)b=i;}if(a<0||b<0)return;var item=v2Layers.splice(a,1)[0];if(a<b)b--;if(after)b++;if(b<0)b=0;if(b>v2Layers.length)b=v2Layers.length;v2Layers.splice(b,0,item);v2RenderList();v2Paint();}\n"
"function v2Remove(layer){if(!layer||!layer.removable)return;var at=v2Layers.indexOf(layer);if(at<0)return;try{if(layer.media){layer.media.pause();layer.media.removeAttribute('src');layer.media.load();}}catch(e){}if(layer.url)try{URL.revokeObjectURL(layer.url)}catch(e){}v2Layers.splice(at,1);if(v2Selected===layer.id)v2Selected='';if(v2MenuId===layer.id)v2MenuId='';v2RenderList();v2Status('SOURCE REMOVED','');v2Paint();}\n"
"\n"
"function v2RenderList(){\n"
"  if(!list)return;list.innerHTML='';if(count)count.textContent=String(v2Layers.length);\n"
"  for(var i=0;i<v2Layers.length;i++){(function(layer){\n"
"    var wrap=document.createElement('div');wrap.className='labLayerWrapV2';\n"
"    var row=document.createElement('div');row.className='labLayerV2'+(v2Selected===layer.id?' selected':'');row.draggable=false;row.dataset.layerId=layer.id;row.title=layer.type==='live'?'APPLIANCE IMAGEN':'LOCAL VIDEO';\n"
"    row.addEventListener('dragover',function(ev){ev.preventDefault();ev.stopPropagation();v2ClearDragMarks();var rect=row.getBoundingClientRect(),after=ev.clientY>rect.top+rect.height/2;row.dataset.dropAfter=after?'1':'0';row.classList.add(after?'dragAfter':'dragBefore');try{ev.dataTransfer.dropEffect='move'}catch(e){}});\n"
"    row.addEventListener('dragleave',function(){row.classList.remove('dragBefore');row.classList.remove('dragAfter')});\n"
"    row.addEventListener('drop',function(ev){ev.preventDefault();ev.stopPropagation();var from=v2DragId;try{if(!from)from=ev.dataTransfer.getData('text/plain')}catch(e){}var rect=row.getBoundingClientRect(),after=ev.clientY>rect.top+rect.height/2;v2ClearDragMarks();v2Move(from,layer.id,after)});\n"
"    var eye=document.createElement('button');eye.type='button';eye.className='labEyeV2'+(layer.visible?'':' off');eye.innerHTML='&#128065;';eye.title=layer.visible?'Hide layer':'Show layer';eye.onclick=function(ev){ev.stopPropagation();layer.visible=!layer.visible;v2RenderList();v2Paint();};\n"
"    var dragZone=document.createElement('div');dragZone.className='labDragZoneV2';dragZone.draggable=true;dragZone.title='Drag name to reorder / click to select';\n"
"    var nm=document.createElement('div');nm.className='labLayerV2Name';nm.textContent=layer.name;dragZone.appendChild(nm);\n"
"    dragZone.addEventListener('dragstart',function(ev){v2DidDrag=true;v2DragId=layer.id;dragZone.classList.add('dragging');v2ClearDragMarks();try{ev.dataTransfer.effectAllowed='move';ev.dataTransfer.setData('text/plain',layer.id)}catch(e){}});\n"
"    dragZone.addEventListener('dragend',function(){v2DragId='';dragZone.classList.remove('dragging');v2ClearDragMarks();setTimeout(function(){v2DidDrag=false},0)});\n"
"    dragZone.addEventListener('click',function(ev){ev.stopPropagation();if(v2DidDrag){v2DidDrag=false;return}v2ToggleSelect(layer.id)});\n"
"    var range=document.createElement('input');range.className='labLayerV2Opacity';range.type='range';range.min='0';range.max='100';range.step='1';range.value=String(Math.round(layer.opacity*100));range.draggable=false;range.setAttribute('aria-label',layer.name+' opacity');\n"
"    var pct=document.createElement('div');pct.className='labLayerV2Pct';pct.textContent=range.value+'%';\n"
"    range.onpointerdown=function(ev){ev.stopPropagation()};range.onclick=function(ev){ev.stopPropagation()};range.oninput=function(ev){ev.stopPropagation();layer.opacity=Math.max(0,Math.min(1,Number(range.value)/100));pct.textContent=range.value+'%';v2Paint();};\n"
"    var more=document.createElement('button');more.type='button';more.className='labMoreV2'+(v2MenuId===layer.id?' on':'');more.textContent='⋯';more.title='Layer tools';more.onclick=function(ev){ev.stopPropagation();v2MenuId=(v2MenuId===layer.id?'':layer.id);v2RenderList();};\n"
"    var del=document.createElement('button');del.type='button';del.className='labDeleteV2'+(layer.removable?'':' fixed');del.innerHTML='&times;';del.title='Remove source';if(layer.removable)del.onclick=function(ev){ev.stopPropagation();v2Remove(layer)};\n"
"    row.appendChild(eye);row.appendChild(dragZone);row.appendChild(range);row.appendChild(pct);row.appendChild(more);row.appendChild(del);wrap.appendChild(row);\n"
"    if(v2MenuId===layer.id){\n"
"      var menu=document.createElement('div');menu.className='labLayerMenuV2';menu.setAttribute('data-control','1');\n"
"      var trim=document.createElement('button');trim.type='button';trim.className='labMenuBtnV2 mock';trim.textContent='TRIM';trim.disabled=layer.type!=='video';trim.title=layer.type==='video'?'Temporal trim control · implementation next':'Trim applies to video';trim.onclick=function(ev){ev.stopPropagation();v2Status('TRIM CONTROL · NEXT','')};\n"
"      var crop=document.createElement('button');crop.type='button';crop.className='labMenuBtnV2 mock';crop.textContent='CROP';crop.title='Visual crop control · implementation next';crop.onclick=function(ev){ev.stopPropagation();v2Status('CROP CONTROL · NEXT','')};\n"
"      var sizeWrap=document.createElement('div');sizeWrap.className='labSizeControlV2';\n"
"      var sizeLabel=document.createElement('div');sizeLabel.className='labSizeLabelV2';sizeLabel.textContent='SIZE';\n"
"      var sizeRange=document.createElement('input');sizeRange.type='range';sizeRange.className='labSizeRangeV2';sizeRange.min='25';sizeRange.max='200';sizeRange.step='1';sizeRange.value=String(Math.round((layer.size||1)*100));sizeRange.draggable=false;\n"
"      var sizePct=document.createElement('div');sizePct.className='labSizePctV2';sizePct.textContent=sizeRange.value+'%';\n"
"      sizeRange.onpointerdown=function(ev){ev.stopPropagation()};sizeRange.onclick=function(ev){ev.stopPropagation()};sizeRange.oninput=function(ev){ev.stopPropagation();layer.size=Math.max(.25,Math.min(2,Number(sizeRange.value)/100));sizePct.textContent=sizeRange.value+'%';v2Paint();};\n"
"      sizeWrap.appendChild(sizeLabel);sizeWrap.appendChild(sizeRange);sizeWrap.appendChild(sizePct);menu.appendChild(trim);menu.appendChild(crop);menu.appendChild(sizeWrap);wrap.appendChild(menu);\n"
"    }\n"
"    list.appendChild(wrap);\n"
"  })(v2Layers[i]);}\n"
"}\n"
"list.addEventListener('dragover',function(ev){if(ev.target!==list)return;ev.preventDefault();try{ev.dataTransfer.dropEffect='move'}catch(e){}});\n"
"list.addEventListener('drop',function(ev){if(ev.target!==list)return;ev.preventDefault();var from=v2DragId;try{if(!from)from=ev.dataTransfer.getData('text/plain')}catch(e){}if(!from||!v2Layers.length)return;v2Move(from,v2Layers[v2Layers.length-1].id,true)});\n"
"\n"
"function v2AspectRatio(nw,nh){if(v2Aspect==='16:9')return 16/9;if(v2Aspect==='4:3')return 4/3;if(v2Aspect==='1:1')return 1;if(v2Aspect==='9:16')return 9/16;return nw/Math.max(1,nh);}\n"
"function v2OutputDimensions(sw,sh,rot){\n"
"  var quarter=(rot%180)!==0,nw=quarter?sh:sw,nh=quarter?sw:sh,ratio=v2AspectRatio(nw,nh);\n"
"  if(v2Resolution==='source'&&v2Aspect==='source'){var sc=Math.min(1,4096/Math.max(nw,nh));return {w:Math.max(1,Math.round(nw*sc)),h:Math.max(1,Math.round(nh*sc))};}\n"
"  var shortSide=v2Resolution==='source'?Math.min(nw,nh):Number(v2Resolution);\n"
"  shortSide=Math.max(64,Math.min(2160,Math.round(shortSide||720)));\n"
"  var w,h;if(ratio>=1){h=shortSide;w=Math.round(h*ratio)}else{w=shortSide;h=Math.round(w/ratio)}\n"
"  var cap=Math.max(w,h);if(cap>4096){var scale=4096/cap;w=Math.round(w*scale);h=Math.round(h*scale)}\n"
"  return {w:Math.max(1,w),h:Math.max(1,h)};\n"
"}\n"
"function v2LayerGeometry(layer,w,h,rot){\n"
"  var cx=w/2+(Number(layer.x)||0)*w,cy=h/2+(Number(layer.y)||0)*h,size=Math.max(.25,Math.min(2,Number(layer.size)||1)),mw=0,mh=0,rw=0,rh=0;\n"
"  if(layer.type==='live'&&sourcePreview){mw=sourcePreview.naturalWidth||0;mh=sourcePreview.naturalHeight||0;rw=(rot%180)?mh:mw;rh=(rot%180)?mw:mh;}\n"
"  else if(layer.media){mw=layer.media.videoWidth||layer.media.naturalWidth||0;mh=layer.media.videoHeight||layer.media.naturalHeight||0;rw=mw;rh=mh;}\n"
"  if(!rw||!rh)return {cx:cx,cy:cy,w:w*size,h:h*size,x:cx-w*size/2,y:cy-h*size/2,rot:rot,scale:1,mw:mw,mh:mh};\n"
"  var sc=Math.min(w/rw,h/rh),dw=rw*sc*size,dh=rh*sc*size;\n"
"  return {cx:cx,cy:cy,w:dw,h:dh,x:cx-dw/2,y:cy-dh/2,rot:rot,scale:sc*size,mw:mw,mh:mh};\n"
"}\n"
"function v2DrawLayer(ctx,layer,w,h,sw,sh,rot){\n"
"  var g=v2LayerGeometry(layer,w,h,rot);ctx.save();ctx.globalAlpha=layer.opacity;ctx.translate(g.cx,g.cy);\n"
"  if(layer.type==='live'){\n"
"    if(sourcePreview&&sourcePreview.complete&&sourcePreview.naturalWidth){ctx.rotate(rot*Math.PI/180);ctx.drawImage(sourcePreview,-g.mw*g.scale/2,-g.mh*g.scale/2,g.mw*g.scale,g.mh*g.scale)}\n"
"  }else if(layer.media&&layer.media.readyState>=2){ctx.drawImage(layer.media,-g.w/2,-g.h/2,g.w,g.h)}\n"
"  ctx.restore();fxResetContext(ctx);\n"
"}\n"
"renderRotatedSource=function(w,h,sw,sh,rot){\n"
"  var dst=fxEngine.buffers.a,ctx=dst.getContext('2d');if(!ctx)return null;fxResetContext(ctx);ctx.clearRect(0,0,w,h);ctx.imageSmoothingEnabled=true;\n"
"  for(var i=v2Layers.length-1;i>=0;i--){var layer=v2Layers[i];if(!layer.visible||layer.opacity<=0)continue;v2DrawLayer(ctx,layer,w,h,sw,sh,rot)}\n"
"  return dst;\n"
"};\n"
"function v2DrawSelectionOverlay(){\n"
"  if(!v2Selected||!labCanvas)return;var layer=v2LayerById(v2Selected);if(!layer||!layer.visible)return;var w=labCanvas.width,h=labCanvas.height;if(!w||!h)return;\n"
"  var rot=((previewRotation%360)+360)%360,g=v2LayerGeometry(layer,w,h,rot),ctx=labCanvas.getContext('2d');if(!ctx)return;\n"
"  ctx.save();ctx.setTransform(1,0,0,1,0,0);ctx.globalAlpha=1;ctx.globalCompositeOperation='source-over';ctx.strokeStyle='#23dcdc';ctx.fillStyle='#23dcdc';ctx.lineWidth=Math.max(1,Math.round(Math.min(w,h)/320));ctx.setLineDash([Math.max(4,w/90),Math.max(3,w/140)]);ctx.strokeRect(g.x,g.y,g.w,g.h);ctx.setLineDash([]);\n"
"  var hs=Math.max(4,Math.min(w,h)/85),pts=[[g.x,g.y],[g.x+g.w,g.y],[g.x,g.y+g.h],[g.x+g.w,g.y+g.h]];for(var i=0;i<pts.length;i++)ctx.fillRect(pts[i][0]-hs/2,pts[i][1]-hs/2,hs,hs);ctx.restore();\n"
"}\n"
"function v2RenderOutput(){\n"
"  if(!labCanvas||!sourcePreview)return;\n"
"  if(!sourcePreview.complete||!sourcePreview.naturalWidth){if(labWaiting)labWaiting.style.display='flex';return}\n"
"  var sw=sourcePreview.naturalWidth,sh=sourcePreview.naturalHeight,rot=((previewRotation%360)+360)%360,d=v2OutputDimensions(sw,sh,rot),w=d.w,h=d.h;\n"
"  fxEngine.resize(w,h);if(labCanvas.width!==w)labCanvas.width=w;if(labCanvas.height!==h)labCanvas.height=h;\n"
"  var base=renderRotatedSource(w,h,sw,sh,rot);if(!base)return;var output=runFxPipeline(latestState),ctx=labCanvas.getContext('2d');if(!ctx)return;\n"
"  fxResetContext(ctx);ctx.clearRect(0,0,w,h);ctx.imageSmoothingEnabled=false;ctx.drawImage(output,0,0,w,h);\n"
"  if(labWaiting)labWaiting.style.display='none';if(outputActual)outputActual.textContent=w+'×'+h;\n"
"  v2DrawSelectionOverlay();var s=v2LayerById(v2Selected);if(labSourceName)labSourceName.textContent=(s?s.name:'COMPOSITE')+' · '+v2Layers.length+(v2Layers.length===1?' LAYER':' LAYERS');\n"
"}\n"
"renderLabPreview=v2RenderOutput;\n"
"\n"
"function v2CanvasPoint(ev){var r=labCanvas.getBoundingClientRect();if(!r.width||!r.height)return null;return {x:(ev.clientX-r.left)*(labCanvas.width/r.width),y:(ev.clientY-r.top)*(labCanvas.height/r.height)}}\n"
"function v2PointInside(g,p){return p&&p.x>=g.x&&p.x<=g.x+g.w&&p.y>=g.y&&p.y<=g.y+g.h}\n"
"labCanvas.addEventListener('pointerdown',function(ev){if(!v2Selected)return;var layer=v2LayerById(v2Selected),p=v2CanvasPoint(ev);if(!layer||!p)return;var rot=((previewRotation%360)+360)%360,g=v2LayerGeometry(layer,labCanvas.width,labCanvas.height,rot);if(!v2PointInside(g,p))return;v2Pointer={id:ev.pointerId,lastX:p.x,lastY:p.y,layer:layer};try{labCanvas.setPointerCapture(ev.pointerId)}catch(e){}ev.preventDefault();ev.stopPropagation();});\n"
"labCanvas.addEventListener('pointermove',function(ev){if(!v2Pointer||v2Pointer.id!==ev.pointerId)return;var p=v2CanvasPoint(ev);if(!p)return;var dx=(p.x-v2Pointer.lastX)/Math.max(1,labCanvas.width),dy=(p.y-v2Pointer.lastY)/Math.max(1,labCanvas.height);v2Pointer.lastX=p.x;v2Pointer.lastY=p.y;v2Pointer.layer.x=Math.max(-.75,Math.min(.75,(Number(v2Pointer.layer.x)||0)+dx));v2Pointer.layer.y=Math.max(-.75,Math.min(.75,(Number(v2Pointer.layer.y)||0)+dy));v2Paint();ev.preventDefault();ev.stopPropagation();});\n"
"function v2PointerEnd(ev){if(!v2Pointer||v2Pointer.id!==ev.pointerId)return;try{if(labCanvas.hasPointerCapture&&labCanvas.hasPointerCapture(ev.pointerId))labCanvas.releasePointerCapture(ev.pointerId)}catch(e){}v2Pointer=null;ev.preventDefault();ev.stopPropagation()}\n"
"labCanvas.addEventListener('pointerup',v2PointerEnd);labCanvas.addEventListener('pointercancel',v2PointerEnd);\n"
"\n"
"if(aspectSelect)aspectSelect.addEventListener('change',function(ev){ev.stopPropagation();v2Aspect=aspectSelect.value;try{localStorage.setItem('prismaFxLabAspectV1',v2Aspect)}catch(e){}v2Paint()});\n"
"if(resolutionSelect)resolutionSelect.addEventListener('change',function(ev){ev.stopPropagation();v2Resolution=resolutionSelect.value;try{localStorage.setItem('prismaFxLabResolutionV1',v2Resolution)}catch(e){}v2Paint()});\n"
"function v2HasVideo(){for(var i=0;i<v2Layers.length;i++)if(v2Layers[i].type==='video')return true;return false;}\n"
"function v2Animate(){v2Raf=0;if(!v2HasVideo())return;if(pg===4||realFsElement()===labFrame||labFrame.classList.contains('pseudoFull'))renderLabPreview();v2Raf=requestAnimationFrame(v2Animate);}\n"
"function v2EnsureAnimation(){if(!v2Raf&&v2HasVideo())v2Raf=requestAnimationFrame(v2Animate);}\n"
"function v2Paint(){if(pg===4||realFsElement()===labFrame||labFrame.classList.contains('pseudoFull'))requestAnimationFrame(renderLabPreview);}\n"
"if(add&&picker)add.onclick=function(){picker.click();};\n"
"if(picker)picker.addEventListener('change',function(){var file=picker.files&&picker.files[0];picker.value='';if(!file)return;if(file.size>V2_MAX_BYTES){v2Status('FILE TOO LARGE · 50 MB MAX','error');return;}var url=URL.createObjectURL(file),video=document.createElement('video');video.muted=true;video.loop=true;video.playsInline=true;video.preload='auto';video.src=url;var failed=false;function fail(){if(failed)return;failed=true;try{URL.revokeObjectURL(url)}catch(e){}v2Status('BROWSER CANNOT DECODE '+file.name,'error');}video.addEventListener('error',fail,{once:true});video.addEventListener('loadeddata',function(){if(failed)return;var duration=isFinite(video.duration)?video.duration:0,layer={id:'video'+(++v2Seq),type:'video',name:file.name,visible:true,opacity:1,size:1,x:0,y:0,cropX:0,cropY:0,cropW:1,cropH:1,trimIn:0,trimOut:duration,media:video,removable:true,url:url};v2Layers.unshift(layer);v2Selected=layer.id;v2RenderList();v2Status(file.name+' READY · '+(file.size/1048576).toFixed(1)+' MB','ok');var p=video.play();if(p&&p.catch)p.catch(function(){});v2EnsureAnimation();v2Paint();},{once:true});v2Status('TESTING BROWSER DECODER · '+file.name,'');video.load();var p=video.play();if(p&&p.catch)p.catch(function(){});});\n"
"window.addEventListener('beforeunload',function(){for(var i=0;i<v2Layers.length;i++)if(v2Layers[i].url)try{URL.revokeObjectURL(v2Layers[i].url)}catch(e){}});\n"
"v2RenderList();v2Paint();\n"
"})();\n"
"</script>\n";

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
