#include <string.h>
#include <stdio.h>
#include "esp_http_server.h"

bool prisma_appliance_get_local_processing_active(void);
int prisma_appliance_get_local_processing_value(int effect);

static esp_err_t local_httpd_resp_send_proxy(httpd_req_t *r,const char *buf,ssize_t len);
static esp_err_t local_httpd_resp_send_chunk_proxy(httpd_req_t *r,const char *buf,ssize_t len);

#define httpd_resp_send local_httpd_resp_send_proxy
#define httpd_resp_send_chunk local_httpd_resp_send_chunk_proxy
#include "wifi_portal_intensity.c"
#undef httpd_resp_send_chunk
#undef httpd_resp_send

static const char local_processing_web_injection[] =
"\n"
"<style>\n"
".track{width:700%!important}.page{width:14.2857143%!important;flex:0 0 14.2857143%!important}\n"
".localPage{padding-top:2px}\n"
".localStatus{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:12px 0 16px}\n"
".localStatusBox{height:40px;border:1px solid #2d3c4b;border-radius:10px;background:#172536;color:#788897;display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:900;letter-spacing:1px}\n"
".localStatusBox.on.active{background:#23c86e;border-color:#23c86e;color:#f0f5fa}\n"
".localStatusBox.on.inactive{background:#e63741;border-color:#e63741;color:#f0f5fa}\n"
".localRows{display:flex;flex-direction:column;gap:13px}\n"
".localRow{display:grid;grid-template-columns:92px 1fr;gap:8px;align-items:center}\n"
".localName{font-size:11px;font-weight:900;letter-spacing:1px;color:#f0f5fa}\n"
".localSteps{display:grid;grid-template-columns:repeat(4,1fr);gap:6px}\n"
".localStep{height:34px;border:1px solid #2d3c4b;border-radius:9px;background:#172536;color:#788897;display:flex;align-items:center;justify-content:center;font-size:14px;font-weight:900}\n"
".localStep.on{background:#23c86e;border-color:#23c86e;color:#f0f5fa}\n"
".labLocalRow{border:1px dashed #6f5c18;border-radius:11px;padding:6px 8px;background:#171306}\n"
".labLocalRow .labBlock{border-color:#f5be23!important;cursor:default!important}\n"
".labLocalRow .labHandle,.labLocalRow .labBlockValue{color:#f5be23!important}\n"
"</style>\n"
"<script>(function(){\n"
"var track=document.getElementById('t');\n"
"if(track&&!document.getElementById('localProcessingPage')){\n"
"  var sec=document.createElement('section');sec.id='localProcessingPage';sec.className='page localPage';\n"
"  sec.innerHTML='<div class=\"labHead\"><div><div class=\"labTitle\">LOCAL PROCESSING</div><div class=\"labSub\">APPLIANCE PREPROCESSING / READ ONLY</div></div></div><div class=\"localStatus\"><div id=\"lpActive\" class=\"localStatusBox\">ACTIVE</div><div id=\"lpInactive\" class=\"localStatusBox on inactive\">INACTIVE</div></div><div id=\"localRows\" class=\"localRows\"></div><div class=\"hint effectsHint\">LOCAL PROCESSING / STATE ONLY · PROCESSING COMES IN STEP B</div>';\n"
"  var names=['MONO','CONTRAST','NEGATIVE'],box=sec.querySelector('#localRows');\n"
"  for(var i=0;i<3;i++){var row=document.createElement('div');row.className='localRow';var steps='';for(var v=0;v<4;v++)steps+='<div id=\"lp'+i+'v'+v+'\" class=\"localStep'+(v===0?' on':'')+'\">'+v+'</div>';row.innerHTML='<div class=\"localName\">'+names[i]+'</div><div class=\"localSteps\">'+steps+'</div>';box.appendChild(row);}\n"
"  track.appendChild(sec);\n"
"  var dots=document.querySelector('.dots');if(dots&&!document.getElementById('d6')){var d=document.createElement('i');d.id='d6';d.className='dot';dots.appendChild(d);}\n"
"}\n"
"page=function(n){pg=Math.max(0,Math.min(6,n));t.style.transform='translateX(-'+(pg*(100/7))+'%)';for(var i=0;i<7;i++){var d=document.getElementById('d'+i);if(d)d.className='dot'+(i===pg?' on':'');}if(pg===4)renderLabPreview();};\n"
"function localState(s){var p=s&&s.localProcessing?s.localProcessing:{};var vals=Array.isArray(p.values)?p.values:[0,0,0];return {active:!!p.active,values:[Math.max(0,Math.min(3,Number(vals[0])||0)),Math.max(0,Math.min(3,Number(vals[1])||0)),Math.max(0,Math.min(3,Number(vals[2])||0))]};}\n"
"function drawLocalProcessing(s){var p=localState(s),a=document.getElementById('lpActive'),z=document.getElementById('lpInactive');if(a)a.className='localStatusBox'+(p.active?' on active':'');if(z)z.className='localStatusBox'+(!p.active?' on inactive':'');for(var i=0;i<3;i++)for(var v=0;v<4;v++){var b=document.getElementById('lp'+i+'v'+v);if(b)b.className='localStep'+(p.values[i]===v?' on':'');}}\n"
"var localBaseRenderLabPipeline=renderLabPipeline;\n"
"renderLabPipeline=function(s){\n"
"  localBaseRenderLabPipeline(s);if(!labPipeline)return;var p=localState(s);if(!p.active)return;\n"
"  var row=document.createElement('div');row.className='labOrderRow labLocalRow';var names=['MONO','CONTRAST','NEGATIVE'],count=0;\n"
"  for(var i=0;i<3;i++){if(p.values[i]<=0)continue;count++;var block=document.createElement('div');block.className='labBlock labLocalBlock';block.draggable=false;block.dataset.local=String(i);var handle=document.createElement('span');handle.className='labHandle';handle.textContent='●';var name=document.createElement('span');name.className='labBlockName';name.textContent=names[i];var val=document.createElement('span');val.className='labBlockValue';val.textContent=String(p.values[i]);block.appendChild(handle);block.appendChild(name);block.appendChild(val);row.appendChild(block);}\n"
"  if(count===0){var block=document.createElement('div');block.className='labBlock labLocalBlock';block.draggable=false;var handle=document.createElement('span');handle.className='labHandle';handle.textContent='●';var name=document.createElement('span');name.className='labBlockName';name.textContent='LOCAL ACTIVE';block.appendChild(handle);block.appendChild(name);row.appendChild(block);}\n"
"  labPipeline.insertBefore(row,labPipeline.firstChild);\n"
"};\n"
"var localOldDrawAll=drawAll;drawAll=function(s){localOldDrawAll(s);drawLocalProcessing(s);renderLabPipeline(s);};\n"
"var localHint=document.querySelector('.labHint');if(localHint)localHint.textContent='YELLOW = LOCAL FIXED · PURPLE = MIXED FX · ORANGE = MOTION · MIX FIXED';\n"
"if(latestState){drawLocalProcessing(latestState);renderLabPipeline(latestState);}\n"
"})();</script>\n";

static esp_err_t local_httpd_resp_send_proxy(httpd_req_t *r,const char *buf,ssize_t len)
{
    if(r&&strcmp(r->uri,"/api/state")==0&&buf){
        size_t n=(len==HTTPD_RESP_USE_STRLEN)?strlen(buf):(len>0?(size_t)len:0);
        size_t cut=n;while(cut>0&&(buf[cut-1]==' '||buf[cut-1]=='\n'||buf[cut-1]=='\r'||buf[cut-1]=='\t'))cut--;
        if(cut>0&&buf[cut-1]=='}'&&cut<1200){
            char out[1400];int v0=prisma_appliance_get_local_processing_value(0),v1=prisma_appliance_get_local_processing_value(1),v2=prisma_appliance_get_local_processing_value(2);
            int written=snprintf(out,sizeof(out),"%.*s,\"localProcessing\":{\"active\":%s,\"values\":[%d,%d,%d]}}",(int)(cut-1),buf,prisma_appliance_get_local_processing_active()?"true":"false",v0,v1,v2);
            if(written>0&&written<(int)sizeof(out))return httpd_resp_send(r,out,written);
        }
    }
    return httpd_resp_send(r,buf,len);
}

static esp_err_t local_httpd_resp_send_chunk_proxy(httpd_req_t *r,const char *buf,ssize_t len)
{
    if(r&&strcmp(r->uri,"/")==0&&buf){
        size_t n=(len==HTTPD_RESP_USE_STRLEN)?strlen(buf):(len>0?(size_t)len:0);
        if(n>=7&&memcmp(buf,"</body>",7)==0){esp_err_t e=httpd_resp_send_chunk(r,local_processing_web_injection,HTTPD_RESP_USE_STRLEN);if(e!=ESP_OK)return e;}
    }
    return httpd_resp_send_chunk(r,buf,len);
}
