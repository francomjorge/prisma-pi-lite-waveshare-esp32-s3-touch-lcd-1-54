#include "esp_http_server.h"

static esp_err_t checkpoint_page_get(httpd_req_t *r);

#define checkpoint_page_get(...) intensity_base_page_get(__VA_ARGS__)
#include "wifi_portal_checkpoint_a.c"
#undef checkpoint_page_get

static const char intensity_web_injection[] =
"\n"
"<style>\n"
".intensityPresetState{font-size:10px;font-weight:900;letter-spacing:1px;color:#789;text-align:right}\n"
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
"  if(card){card.innerHTML='<div class=\"row compact\"><div class=\"label stronglabel\">INTENSITY</div><div id=\"intensityPresetState\" class=\"intensityPresetState\">FREE</div></div><div id=\"intensityPresetRow\" class=\"intensityPresetRow\" data-control=\"1\"><div id=\"intensityPreset0\" class=\"intensityPresetBtn on free\">0</div><div id=\"intensityPreset1\" class=\"intensityPresetBtn\">1</div><div id=\"intensityPreset2\" class=\"intensityPresetBtn\">2</div><div id=\"intensityPreset3\" class=\"intensityPresetBtn\">3</div></div>';}
"
"}\n"
"function decodeIntensityPreset(s){\n"
"  var state=Math.round(Number(s&&s.intensity));if(!isFinite(state)||state<0)state=0;if(state>9)state=9;\n"
"  if(state===0)return {state:0,button:0,level:0};\n"
"  var z=state-1;return {state:state,button:Math.floor(z/3)+1,level:z%3};\n"
"}\n"
"function drawIntensityPreset(s){\n"
"  var d=decodeIntensityPreset(s),names=['GREEN','YELLOW','RED'],classes=['green','yellow','red'];\n"
"  for(var i=0;i<4;i++){var b=document.getElementById('intensityPreset'+i);if(!b)continue;b.className='intensityPresetBtn';if(i===d.button)b.className+=' on '+(i===0?'free':classes[d.level]);}\n"
"  var label=document.getElementById('intensityPresetState');if(label)label.textContent=d.button===0?'FREE':(d.button+' / '+names[d.level]);\n"
"}\n"
"var intensityOldDrawAll=drawAll;drawAll=function(s){intensityOldDrawAll(s);drawIntensityPreset(s);};\n"
"if(latestState)drawIntensityPreset(latestState);\n"
"})();</script>\n";

static esp_err_t checkpoint_page_get(httpd_req_t *r)
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
    err=httpd_resp_send_chunk(r,(const char*)cut,(ssize_t)(e-cut));
    if(err!=ESP_OK)return err;
    return httpd_resp_send_chunk(r,NULL,0);
}
