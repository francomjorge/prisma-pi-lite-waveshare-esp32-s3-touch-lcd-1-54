#define page_get prisma_legacy_page_get
#define state_get prisma_legacy_state_get
#define start_portal prisma_legacy_start_portal
#define prisma_wifi_start prisma_legacy_wifi_start
#include "wifi_portal.c"
#undef prisma_wifi_start
#undef start_portal
#undef state_get
#undef page_get

bool prisma_appliance_get_motion_effect(int effect);
int prisma_appliance_get_motion_value(int effect);

static const char checkpoint_web_injection[] =
"<style>"
".track{width:600%!important}.page{width:16.6666667%!important;flex:0 0 16.6666667%!important}"
".labFrame{background:#04070c!important;border-color:#04070c!important;box-shadow:none!important}"
".labWaiting{background:#04070c!important}"
".labFrame .labFsBtn{opacity:0!important;transition:opacity .15s ease,background .15s ease}"
".labFrame:hover .labFsBtn,.labFrame:focus-within .labFsBtn,.labFrame .labFsBtn:focus,.labFrame .labFsBtn:active{opacity:1!important}"
".labFixedRow .labBlock[data-fx=\"5\"]{border-color:#af37ff}"
"</style>"
"<script>(function(){"
"var track=document.getElementById('t');if(track&&!document.getElementById('motionEffects')){"
"var sec=document.createElement('section');sec.className='page motionPage';"
"sec.innerHTML='<div class=\"labHead\"><div><div class=\"labTitle\">MOTION FX</div><div class=\"labSub\">APPLIANCE CONTROL / BROWSER STATE</div></div></div><div id=\"motionEffects\" class=\"effects\"></div><div class=\"hint effectsHint\">MOTION FX / READ ONLY · NO MIX · PROCESSING COMES NEXT</div>';"
"var names=['PULSE','DRIFT','ROTATE','WAVE','SHAKE','FEEDBACK'],box=sec.querySelector('#motionEffects');"
"for(var i=0;i<6;i++){var r=document.createElement('div');r.id='mx'+i;r.className='fxrow off';r.innerHTML='<div class=\"fxline\"><div class=\"fxname\">'+names[i]+'</div><div id=\"mxv'+i+'\" class=\"fxvalue\">50</div></div><div class=\"fxbar\"><div id=\"mxfill'+i+'\" class=\"fxfill\"></div></div>';box.appendChild(r);}track.appendChild(sec);"
"var dots=document.querySelector('.dots');if(dots&&!document.getElementById('d5')){var d=document.createElement('i');d.id='d5';d.className='dot';dots.appendChild(d);}}"
"page=function(n){pg=Math.max(0,Math.min(5,n));t.style.transform='translateX(-'+(pg*(100/6))+'%)';for(var i=0;i<6;i++){var d=document.getElementById('d'+i);if(d)d.className='dot'+(i===pg?' on':'');}if(pg===4)renderLabPreview();};"
"labFxNames=['PIXEL','GLITCH','POSTER','HUE','MIRROR','MIX','PULSE','DRIFT','ROTATE','WAVE','SHAKE','FEEDBACK'];"
"normalizeLabOrder=function(order){var allowed=[0,1,2,3,4,6,7,8,9,10,11],seen={},out=[];if(Array.isArray(order)){for(var i=0;i<order.length;i++){var fx=Number(order[i]);if(allowed.indexOf(fx)>=0&&!seen[fx]){seen[fx]=true;out.push(fx);}}}for(var j=0;j<allowed.length;j++){var id=allowed[j];if(!seen[id])out.push(id);}out.push(5);return out;};"
"try{var savedUnified=JSON.parse(localStorage.getItem('prismaFxLabOrderV1')||'null');labFxOrder=normalizeLabOrder(savedUnified||labFxOrder);}catch(e){labFxOrder=normalizeLabOrder(labFxOrder);}"
"saveLabOrder=function(){labFxOrder=normalizeLabOrder(labFxOrder);try{localStorage.setItem('prismaFxLabOrderV1',JSON.stringify(labFxOrder));}catch(e){}};"
"labEffectEnabled=function(s,i){if(i===5)return true;if(i>=6&&i<=11){var me=s&&Array.isArray(s.motionEffects)?s.motionEffects:[];return !!me[i-6];}var ef=s&&Array.isArray(s.effects)?s.effects:[];return !!ef[i];};"
"labEffectValue=function(s,i){if(i>=6&&i<=11){var mv=s&&Array.isArray(s.motionValues)?s.motionValues:[];return clamp100(mv[i-6]);}var vals=s&&Array.isArray(s.values)?s.values:[];return clamp100(vals[i]);};"
"moveLabFx=function(fx,dir){if(fx===5)return;var active=[];for(var i=0;i<labFxOrder.length;i++){var id=labFxOrder[i];if(id!==5&&labEffectEnabled(latestState,id))active.push(id);}var p=active.indexOf(fx),target=p+dir;if(p<0||target<0||target>=active.length)return;var other=active[target],a=labFxOrder.indexOf(fx),b=labFxOrder.indexOf(other);if(a<0||b<0)return;labFxOrder[a]=other;labFxOrder[b]=fx;saveLabOrder();renderLabPipeline(latestState);renderLabPreview();};"
"reverseLabFx=function(){var positions=[],active=[];for(var i=0;i<labFxOrder.length;i++){var id=labFxOrder[i];if(id!==5&&labEffectEnabled(latestState,id)){positions.push(i);active.push(id);}}active.reverse();for(var j=0;j<positions.length;j++)labFxOrder[positions[j]]=active[j];saveLabOrder();renderLabPipeline(latestState);renderLabPreview();};"
"var oldReverseButton=document.getElementById('labReverse');if(oldReverseButton){var newReverseButton=oldReverseButton.cloneNode(true);oldReverseButton.parentNode.replaceChild(newReverseButton,oldReverseButton);newReverseButton.addEventListener('click',function(ev){ev.preventDefault();ev.stopPropagation();reverseLabFx();});}"
"appendLabMixBlock=function(s,row){var block=document.createElement('div');block.className='labBlock';block.draggable=false;block.dataset.fx='5';block.style.cursor='default';var handle=document.createElement('span');handle.className='labHandle';handle.textContent='■';var name=document.createElement('span');name.className='labBlockName';name.textContent='MIX';var val=document.createElement('span');val.className='labBlockValue';val.textContent=String(labEffectValue(s,5));block.appendChild(handle);block.appendChild(name);block.appendChild(val);row.appendChild(block);};"
"renderLabPipeline=function(s){if(!labPipeline)return;labFxOrder=normalizeLabOrder(labFxOrder);labPipeline.innerHTML='';var orderRow=document.createElement('div');orderRow.className='labOrderRow';var active=[];for(var oi=0;oi<labFxOrder.length;oi++){var id=labFxOrder[oi];if(id!==5&&labEffectEnabled(s,id))active.push(id);}if(active.length===0){var empty=document.createElement('div');empty.className='labEmpty';empty.textContent='NO FX ACTIVE';orderRow.appendChild(empty);}for(var p=0;p<active.length;p++){(function(fx){var block=document.createElement('div');block.className='labBlock';block.draggable=true;block.dataset.fx=String(fx);var handle=document.createElement('span');handle.className='labHandle';handle.textContent='≡';var name=document.createElement('span');name.className='labBlockName';name.textContent=labFxNames[fx];var val=document.createElement('span');val.className='labBlockValue';val.textContent=fx===4?'ON':String(labEffectValue(s,fx));if(fx===4)val.style.color='#23c86e';var left=document.createElement('button');left.type='button';left.className='labMove';left.textContent='◀';left.setAttribute('data-control','1');left.onclick=function(ev){ev.stopPropagation();moveLabFx(fx,-1);};var right=document.createElement('button');right.type='button';right.className='labMove';right.textContent='▶';right.setAttribute('data-control','1');right.onclick=function(ev){ev.stopPropagation();moveLabFx(fx,1);};block.appendChild(handle);block.appendChild(name);block.appendChild(val);block.appendChild(left);block.appendChild(right);block.addEventListener('dragstart',function(ev){labDragFx=fx;block.classList.add('dragging');try{ev.dataTransfer.effectAllowed='move';ev.dataTransfer.setData('text/plain',String(fx));}catch(e){}});block.addEventListener('dragend',function(){labDragFx=-1;block.classList.remove('dragging');});block.addEventListener('dragover',function(ev){ev.preventDefault();try{ev.dataTransfer.dropEffect='move';}catch(e){}});block.addEventListener('drop',function(ev){ev.preventDefault();var from=labDragFx;if(from<0){try{from=Number(ev.dataTransfer.getData('text/plain'));}catch(e){from=-1;}}if(from===5||fx===5)return;var fromAt=labFxOrder.indexOf(from),toAt=labFxOrder.indexOf(fx);if(fromAt<0||toAt<0||fromAt===toAt)return;var swap=labFxOrder[fromAt];labFxOrder[fromAt]=labFxOrder[toAt];labFxOrder[toAt]=swap;saveLabOrder();renderLabPipeline(latestState);renderLabPreview();});orderRow.appendChild(block);})(active[p]);}labPipeline.appendChild(orderRow);var fixedRow=document.createElement('div');fixedRow.className='labFixedRow';appendLabMixBlock(s,fixedRow);labPipeline.appendChild(fixedRow);};"
"var labHint=document.querySelector('.labHint');if(labHint)labHint.textContent='DRAG BLOCKS OR USE ◀ ▶ · MIX FIXED · F FULLSCREEN';"
"window.drawMotion=function(s){var ef=s&&Array.isArray(s.motionEffects)?s.motionEffects:[],vals=s&&Array.isArray(s.motionValues)?s.motionValues:[];for(var i=0;i<6;i++){var enabled=!!ef[i],row=document.getElementById('mx'+i),vv=clamp100(vals[i]);if(row)row.className='fxrow'+(enabled?'':' off');var value=document.getElementById('mxv'+i),fill=document.getElementById('mxfill'+i);if(value)value.textContent=vv;if(fill)fill.style.width=vv+'%';}};"
"var oldDrawAll=drawAll;drawAll=function(s){oldDrawAll(s);drawMotion(s);};if(latestState){renderLabPipeline(latestState);drawMotion(latestState);}"
"})();</script>";

static esp_err_t checkpoint_page_get(httpd_req_t*r)
{
    const char needle[]="</body>";const uint8_t *s=web_index_html_start,*e=web_index_html_end,*cut=NULL;size_t len=(size_t)(e-s),nlen=sizeof(needle)-1;
    for(size_t i=0;i+nlen<=len;i++){if(memcmp(s+i,needle,nlen)==0){cut=s+i;break;}}
    httpd_resp_set_type(r,"text/html");no_cache(r);
    if(!cut)return httpd_resp_send(r,(const char*)s,(ssize_t)len);
    esp_err_t err=httpd_resp_send_chunk(r,(const char*)s,(ssize_t)(cut-s));if(err!=ESP_OK)return err;
    err=httpd_resp_send_chunk(r,checkpoint_web_injection,HTTPD_RESP_USE_STRLEN);if(err!=ESP_OK)return err;
    err=httpd_resp_send_chunk(r,(const char*)cut,(ssize_t)(e-cut));if(err!=ESP_OK)return err;
    return httpd_resp_send_chunk(r,NULL,0);
}

static esp_err_t checkpoint_state_get(httpd_req_t*r)
{
    static bool have_last=false,last_on=false;static int last_mode=-1,last_intensity=-1,last_values[6]={-1,-1,-1,-1,-1,-1},last_motion_values[6]={-1,-1,-1,-1,-1,-1},generation=0;static char last_filename[32]="";static bool last_effects[6]={false,false,false,false,false,false},last_motion[6]={false,false,false,false,false,false};
    bool on=prisma_appliance_get_on();int mode=prisma_appliance_get_mode()?1:0;int intensity=prisma_appliance_get_intensity();const char *filename=prisma_appliance_get_filename();bool effects[6],motion[6];int values[6],motion_values[6];if(!filename||!*filename)filename="F1_MOON.JPG";
    for(int i=0;i<6;i++){effects[i]=prisma_appliance_get_effect(i);values[i]=prisma_appliance_get_effect_value(i);motion[i]=prisma_appliance_get_motion_effect(i);motion_values[i]=prisma_appliance_get_motion_value(i);}effects[5]=true;
    bool changed=!have_last||last_on!=on||last_mode!=mode||last_intensity!=intensity||strcmp(last_filename,filename)!=0;for(int i=0;i<6;i++)if(last_effects[i]!=effects[i]||last_values[i]!=values[i]||last_motion[i]!=motion[i]||last_motion_values[i]!=motion_values[i])changed=true;
    if(changed){generation++;if(generation<=0)generation=1;last_on=on;last_mode=mode;last_intensity=intensity;strlcpy(last_filename,filename,sizeof(last_filename));for(int i=0;i<6;i++){last_effects[i]=effects[i];last_values[i]=values[i];last_motion[i]=motion[i];last_motion_values[i]=motion_values[i];}have_last=true;ESP_LOGI(TAG,"WEB STATE gen=%d source=%s motion-sync",generation,filename);}
    char b[1024];int n=snprintf(b,sizeof(b),"{\"on\":%s,\"mode\":%d,\"intensity\":%d,\"generation\":%d,\"filename\":\"%s\",\"effects\":[%s,%s,%s,%s,%s,true],\"values\":[%d,%d,%d,%d,%d,%d],\"motionEffects\":[%s,%s,%s,%s,%s,%s],\"motionValues\":[%d,%d,%d,%d,%d,%d]}",on?"true":"false",mode,intensity,generation,filename,effects[0]?"true":"false",effects[1]?"true":"false",effects[2]?"true":"false",effects[3]?"true":"false",effects[4]?"true":"false",values[0],values[1],values[2],values[3],values[4],values[5],motion[0]?"true":"false",motion[1]?"true":"false",motion[2]?"true":"false",motion[3]?"true":"false",motion[4]?"true":"false",motion[5]?"true":"false",motion_values[0],motion_values[1],motion_values[2],motion_values[3],motion_values[4],motion_values[5]);
    if(n<0||n>=(int)sizeof(b)){httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"state overflow");return ESP_FAIL;}httpd_resp_set_type(r,"application/json");no_cache(r);return httpd_resp_send(r,b,n);
}

static void checkpoint_start_portal(void)
{
    uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP);char ap[32];snprintf(ap,sizeof(ap),"PRISMA-PI-%02X%02X",mac[4],mac[5]);wifi_config_t cfg={0};strlcpy((char*)cfg.ap.ssid,ap,sizeof(cfg.ap.ssid));cfg.ap.ssid_len=strlen(ap);cfg.ap.max_connection=4;cfg.ap.authmode=WIFI_AUTH_OPEN;ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&cfg));ESP_ERROR_CHECK(esp_wifi_start());
    httpd_config_t hc=HTTPD_DEFAULT_CONFIG();hc.uri_match_fn=httpd_uri_match_wildcard;hc.lru_purge_enable=true;hc.max_uri_handlers=16;httpd_handle_t h=NULL;if(httpd_start(&h,&hc)==ESP_OK){httpd_uri_t a={.uri="/",.method=HTTP_GET,.handler=checkpoint_page_get};httpd_uri_t b={.uri="/api/state",.method=HTTP_GET,.handler=checkpoint_state_get};httpd_uri_t c={.uri="/style.css",.method=HTTP_GET,.handler=css_get};httpd_uri_t d={.uri="/source/*",.method=HTTP_GET,.handler=source_get};httpd_register_uri_handler(h,&a);httpd_register_uri_handler(h,&b);httpd_register_uri_handler(h,&c);httpd_register_uri_handler(h,&d);prisma_admin_register(h);}ESP_LOGI(TAG,"PRISMA LITE CHECKPOINT A: %s - http://192.168.4.1",ap);
}

void prisma_wifi_start(void)
{
    esp_err_t r=nvs_flash_init();if(r==ESP_ERR_NVS_NO_FREE_PAGES||r==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());ESP_ERROR_CHECK(nvs_flash_init());}ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_ap();wifi_init_config_t i=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&i));checkpoint_start_portal();
}
