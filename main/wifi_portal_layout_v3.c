#include "esp_http_server.h"
#include <string.h>

static esp_err_t layout_v3_send_chunk(httpd_req_t *r, const char *buf, ssize_t len);

/*
 * Reuse the validated V2 portal byte-for-byte, but intercept chunk delivery so
 * a small V3 patch can be inserted inside the FX Lab IIFE. This keeps the
 * desktop implementation untouched while adding mobile fullscreen fallback
 * and selected-layer rotation.
 */
#define httpd_resp_send_chunk(...) layout_v3_send_chunk(__VA_ARGS__)
#include "wifi_portal_layout_v2.c"
#undef httpd_resp_send_chunk

static const char fxlab_v3_inner_patch[] =
"\n"
"/* FX LAB V3: selected-layer quarter-turn rotation + iOS pseudo fullscreen. */\n"
"function v3EffectiveLayerRotation(layer,globalRot){var own=((Number(layer&&layer.rotation)||0)%360+360)%360;var base=(layer&&layer.type==='live')?(((Number(globalRot)||0)%360+360)%360):0;return (base+own)%360;}\n"
"v2LayerGeometry=function(layer,w,h,rot){var cx=w/2+(Number(layer.x)||0)*w,cy=h/2+(Number(layer.y)||0)*h,size=Math.max(.25,Math.min(2,Number(layer.size)||1)),mw=0,mh=0;if(layer.type==='live'&&sourcePreview){mw=sourcePreview.naturalWidth||0;mh=sourcePreview.naturalHeight||0;}else if(layer.media){mw=layer.media.videoWidth||layer.media.naturalWidth||0;mh=layer.media.videoHeight||layer.media.naturalHeight||0;}var er=v3EffectiveLayerRotation(layer,rot),rw=(er%180)?mh:mw,rh=(er%180)?mw:mh;if(!rw||!rh)return {cx:cx,cy:cy,w:w*size,h:h*size,x:cx-w*size/2,y:cy-h*size/2,rot:er,scale:1,mw:mw,mh:mh};var sc=Math.min(w/rw,h/rh),dw=rw*sc*size,dh=rh*sc*size;return {cx:cx,cy:cy,w:dw,h:dh,x:cx-dw/2,y:cy-dh/2,rot:er,scale:sc*size,mw:mw,mh:mh};};\n"
"v2DrawLayer=function(ctx,layer,w,h,sw,sh,rot){var g=v2LayerGeometry(layer,w,h,rot);ctx.save();ctx.globalAlpha=layer.opacity;ctx.translate(g.cx,g.cy);ctx.rotate(g.rot*Math.PI/180);if(layer.type==='live'){if(sourcePreview&&sourcePreview.complete&&sourcePreview.naturalWidth)ctx.drawImage(sourcePreview,-g.mw*g.scale/2,-g.mh*g.scale/2,g.mw*g.scale,g.mh*g.scale);}else if(layer.media&&layer.media.readyState>=2){ctx.drawImage(layer.media,-g.mw*g.scale/2,-g.mh*g.scale/2,g.mw*g.scale,g.mh*g.scale);}ctx.restore();fxResetContext(ctx);};\n"
"document.addEventListener('keydown',function(ev){if(pg!==4||!v2Selected||ev.repeat||ev.altKey||ev.ctrlKey||ev.metaKey)return;var target=ev.target,tag=(target&&target.tagName?target.tagName:'').toLowerCase();if(tag==='input'||tag==='textarea'||tag==='select'||(target&&target.isContentEditable))return;if(ev.key!=='r'&&ev.key!=='R')return;var layer=v2LayerById(v2Selected);if(!layer)return;layer.rotation=(((Number(layer.rotation)||0)+90)%360+360)%360;ev.preventDefault();ev.stopPropagation();v2Status('ROT '+layer.rotation+'°','');v2Paint();},true);\n"
"var v3DesktopToggleLabFs=toggleLabFs;\n"
"function v3UseMobilePseudoFull(){var ua=navigator.userAgent||'';return /iPhone|iPad|iPod/i.test(ua)||(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1);}\n"
"function v3SetMobilePseudoFull(on){if(!labFrame)return;if(on){labFrame.classList.add('pseudoFull');document.documentElement.classList.add('fxlabMobileFull');document.body.classList.add('fxlabMobileFull');try{window.scrollTo(0,0)}catch(e){}}else{labFrame.classList.remove('pseudoFull');document.documentElement.classList.remove('fxlabMobileFull');document.body.classList.remove('fxlabMobileFull');}labFsLabel();requestAnimationFrame(renderLabPreview);}\n"
"function v3ToggleLabFs(){if(!v3UseMobilePseudoFull())return v3DesktopToggleLabFs();if(realFsElement()===labFrame){exitRealFs();return}v3SetMobilePseudoFull(!labFrame.classList.contains('pseudoFull'));}\n"
"if(labFsButton){labFsButton.removeEventListener('click',v3DesktopToggleLabFs);toggleLabFs=v3ToggleLabFs;labFsButton.addEventListener('click',toggleLabFs);}\n"
"function v3RefreshMobileFull(){if(labFrame&&labFrame.classList.contains('pseudoFull')){requestAnimationFrame(function(){renderLabPreview();labFsLabel();});}}\n"
"window.addEventListener('orientationchange',v3RefreshMobileFull,{passive:true});if(window.visualViewport)window.visualViewport.addEventListener('resize',v3RefreshMobileFull,{passive:true});\n";

static const char fxlab_v3_style_patch[] =
"\n<style>\n"
"html.fxlabMobileFull,body.fxlabMobileFull{overflow:hidden!important;overscroll-behavior:none!important;width:100%!important;height:100%!important;}\n"
".labFrame.pseudoFull{width:100dvw!important;height:100dvh!important;max-width:100dvw!important;max-height:100dvh!important;}\n"
"@supports not (height:100dvh){.labFrame.pseudoFull{width:100vw!important;height:100vh!important;max-width:100vw!important;max-height:100vh!important;}}\n"
"</style>\n";

static esp_err_t layout_v3_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)
{
    if (buf == fxlab_layout_v2_injection) {
        const char marker[] = "v2RenderList();v2Paint();\n})();";
        const char *at = strstr(buf, marker);
        if (at) {
            esp_err_t err = httpd_resp_send_chunk(r, buf, (ssize_t)(at - buf));
            if (err != ESP_OK) return err;
            err = httpd_resp_send_chunk(r, fxlab_v3_inner_patch, HTTPD_RESP_USE_STRLEN);
            if (err != ESP_OK) return err;
            err = httpd_resp_send_chunk(r, at, HTTPD_RESP_USE_STRLEN);
            if (err != ESP_OK) return err;
            return httpd_resp_send_chunk(r, fxlab_v3_style_patch, HTTPD_RESP_USE_STRLEN);
        }
    }
    return httpd_resp_send_chunk(r, buf, len);
}
