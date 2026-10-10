#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
web = (root / 'main' / 'wifi_portal_layout_v2.c').read_text(encoding='utf-8')
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')
app = (root / 'main' / 'app_checkpoint_a.c').read_text(encoding='utf-8')

# Build must use the isolated web wrapper; appliance logic remains the same source.
assert '"wifi_portal_layout_v2.c"' in cmake
assert '"app_checkpoint_a.c"' in cmake
assert 'wifi_portal_layout_v2.c' not in app

# Three-column workbench: larger preview, shorter Sources panel.
assert 'grid-template-columns:repeat(3,minmax(0,1fr))' in web
assert '.labWorkbenchV2 .labFrame{grid-column:1/3;height:290px!important;min-height:290px}' in web
assert '.labSourcesPanelV2{grid-column:3;height:225px;align-self:start' in web
assert "work.appendChild(labFrame)" in web
assert "oldPanel.style.display='none'" in web

# Compact technical console: only the name/grip zone can start layer reorder.
assert '.labLayerV2{height:27px;display:grid;grid-template-columns:18px minmax(0,1fr) 64px 28px 18px 18px' in web
assert '.labEyeV2,.labDeleteV2,.labMoreV2{width:18px;height:18px' in web
assert 'row.draggable=false' in web
assert 'dragZone.draggable=true' in web
assert "dragZone.title='Drag layer / click to select'" in web
assert 'range.draggable=false' in web
assert 'range.onpointerdown=function(ev){ev.stopPropagation()}' in web
assert 'labAddSourceV2' in web
assert 'title=\\"Add source\\">+</button>' in web
assert '+ ADD SOURCE' not in web

# Clean web titles and reduced explanatory clutter.
assert "setPageTitle(pages[1],'PRISMA PiCore')" in web
assert "setPageTitle(pages[2],'IMAGEN')" in web
assert "setPageTitle(pages[3],'EFFECTS')" in web
assert "fxTitle.textContent='FX LAB'" in web
assert "motionTitle.textContent='MOTION'" in web
assert 'if(fxSub)fxSub.remove()' in web
assert 'if(pipeSub)pipeSub.remove()' in web
assert 'if(motionSub)motionSub.remove()' in web

# LIVE source display name is APPLIANCE IMAGEN; internal id remains stable.
assert "id:'live'" in web
assert "name:'APPLIANCE IMAGEN'" in web
assert 'removable:false' in web

# Source selection toggles, shows an overlay, and can be moved by pointer drag.
assert "function v2ToggleSelect(id)" in web
assert "v2Selected=(v2Selected===id?'':id)" in web
assert 'function v2DrawSelectionOverlay()' in web
assert "ctx.strokeStyle='#23dcdc'" in web
assert "labCanvas.addEventListener('pointerdown'" in web
assert 'labCanvas.setPointerCapture(ev.pointerId)' in web
assert "labCanvas.addEventListener('pointermove'" in web
assert 'v2Pointer.layer.x=' in web and 'v2Pointer.layer.y=' in web

# Three-dot inspector is present. SIZE works now; TRIM/CROP are deliberate mockups.
assert "more.textContent='⋯'" in web
assert "trim.textContent='TRIM'" in web
assert "crop.textContent='CROP'" in web
assert "sizeLabel.textContent='SIZE'" in web
assert "sizeRange.min='25'" in web and "sizeRange.max='200'" in web
assert 'layer.size=Math.max(.25,Math.min(2,Number(sizeRange.value)/100))' in web
assert "v2Status('TRIM CONTROL · NEXT','')" in web
assert "v2Status('CROP CONTROL · NEXT','')" in web

# Transform state is non-destructive and composed before the existing global FX chain.
for token in ('size:1','x:0','y:0','cropX:0','cropY:0','cropW:1','cropH:1','trimIn:0','trimOut:0'):
    assert token in web
assert 'function v2LayerGeometry(layer,w,h,rot)' in web
assert 'function v2DrawLayer(ctx,layer,w,h,sw,sh,rot)' in web
assert 'for(var i=v2Layers.length-1;i>=0;i--)' in web
assert 'v2DrawLayer(ctx,layer,w,h,sw,sh,rot)' in web

# Active FX pipeline remains compact.
assert '.labPipeline{gap:4px!important;margin-top:5px!important;min-height:28px!important}' in web
assert '.labBlock{gap:4px!important;border-radius:8px!important;padding:4px 5px!important;min-height:27px!important}' in web
assert '.labMove{width:18px!important;height:18px!important' in web
assert '.labBlockName{font-size:9px!important' in web
assert '.labReverse{padding:5px 7px!important;font-size:8px!important' in web

# Page and Sources scrollbars stay hidden until hover.
assert '.labPage.fxlabScrollable{scrollbar-width:none!important}' in web
assert '.labPage.fxlabScrollable:hover{scrollbar-width:thin!important}' in web
assert '.labPage.fxlabScrollable:hover::-webkit-scrollbar{width:5px!important}' in web
assert '.labSourcesListV2{flex:1;min-height:0;overflow-y:auto;padding-right:2px;scrollbar-width:none}' in web
assert '.labSourcesListV2:hover::-webkit-scrollbar{width:5px}' in web

# Layer ordering remains real composition ordering. Drop upper/lower half means before/after.
assert 'function v2Move(fromId,toId,after)' in web
assert 'if(after)b++;' in web
assert 'ev.clientY>rect.top+rect.height/2' in web
assert "after?'dragAfter':'dragBefore'" in web
assert "ev.dataTransfer.setData('text/plain',layer.id)" in web
assert "v2Layers.unshift(layer)" in web
assert "v2Move(from,v2Layers[v2Layers.length-1].id,true)" in web
assert "if(!layer||!layer.removable)return" in web

# Browser decides codec support. Common CapCut / WhatsApp containers are offered.
assert 'accept=\\\"video/*,.mp4,.mov,.m4v,.webm,.3gp\\\"' in web
assert 'BROWSER CANNOT DECODE' in web
assert 'loadeddata' in web
assert 'V2_MAX_BYTES=50*1024*1024' in web

# Root handler interception must not rename/redefine the stable intensity handler.
assert 'layout_v2_register_uri' in web
assert 'copy.handler = layout_v2_page_get' in web
assert '#include "wifi_portal_intensity.c"' in web

# No persistent storage/control path added by this web-only feature.
assert 'nvs_set' not in web
assert 'esp_partition_write' not in web

# Existing low-destruction intensity presets remain untouched.
for token in (
    '/* 1G */ {{false,false,false,true,false}, { 0, 6,15,12,50,45}',
    '/* 2Y */ {{false,true,true,true,false},   { 0,33,42,52,50,62}',
    '/* 2R */ {{true,true,true,true,true},     { 8,43,50,64,50,68}',
    '/* 3G */ {{false,true,true,true,true},    { 0,54,58,74,50,75}',
    '/* 3Y */ {{false,true,true,true,true},    { 0,68,68,85,50,84}',
    '/* 3R */ {{false,true,true,true,true},    { 0,86,80,96,50,95}',
):
    assert token in app

# Syntax-check only the V2 injected JavaScript using Node.
start = web.index('static const char fxlab_layout_v2_injection[] =')
end = web.index('\nstatic esp_err_t layout_v2_page_get', start)
chunk = web[start:end]
parts = []
for line in chunk.splitlines()[1:]:
    s = line.strip()
    if s.startswith('"'):
        if s.endswith(';'):
            s = s[:-1]
        try:
            parts.append(ast.literal_eval(s))
        except Exception:
            pass
html = ''.join(parts)
js_start = html.find('<script>')
js_end = html.rfind('</script>')
assert js_start >= 0 and js_end > js_start
js = html[js_start + len('<script>'):js_end]
with tempfile.NamedTemporaryFile('w', suffix='.js', delete=False, encoding='utf-8') as f:
    f.write(js)
    js_path = f.name
subprocess.run(['node', '--check', js_path], check=True)

print('FX Lab editor protected: drag-handle-only reorder, toggle selection, canvas positioning, size inspector, trim/crop mockups')
