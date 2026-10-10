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

# Three-column workbench: preview occupies two columns, Sources the third.
assert 'grid-template-columns:repeat(3,minmax(0,1fr))' in web
assert '.labWorkbenchV2 .labFrame{grid-column:1/3' in web
assert '.labSourcesPanelV2{grid-column:3' in web
assert "work.appendChild(labFrame)" in web
assert "oldPanel.style.display='none'" in web

# Compact technical console: one row per layer, small controls, add button in header.
assert '.labLayerV2{height:27px;display:grid' in web
assert 'grid-template-columns:14px 18px minmax(0,1fr) 64px 28px 18px' in web
assert '.labEyeV2,.labDeleteV2{width:18px;height:18px' in web
assert 'labAddSourceV2' in web
assert 'title=\\"Add source\\">+</button>' in web
assert '+ ADD SOURCE' not in web

# Layer ordering is real composition ordering. Drop upper/lower half means before/after.
assert 'row.draggable=true' in web
assert 'function v2Move(fromId,toId,after)' in web
assert 'if(after)b++;' in web
assert 'ev.clientY>rect.top+rect.height/2' in web
assert "after?'dragAfter':'dragBefore'" in web
assert "ev.dataTransfer.setData('text/plain',layer.id)" in web
assert 'for(var i=v2Layers.length-1;i>=0;i--)' in web
assert "v2Layers.unshift(layer)" in web
assert "v2Move(from,v2Layers[v2Layers.length-1].id,true)" in web

# LIVE VIEW stays permanent but remains reorderable.
assert "id:'live'" in web
assert "name:'LIVE VIEW'" in web
assert 'removable:false' in web
assert "row.draggable=true" in web
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

# Syntax-check only the V2 injected JavaScript using Node when available.
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

print('FX Lab Console Layout V2 protected: compact one-line layers, header add, bidirectional drag reorder')
