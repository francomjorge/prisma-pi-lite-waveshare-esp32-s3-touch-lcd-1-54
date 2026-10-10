#!/usr/bin/env python3
from pathlib import Path
import ast
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
web = (root / 'main' / 'wifi_portal_intensity.c').read_text(encoding='utf-8')
app = (root / 'main' / 'app_checkpoint_a.c').read_text(encoding='utf-8')

# Web-only injection must exist and remain separate from appliance logic.
assert 'fxlab_sources_web_injection' in web
assert 'FX Lab Sources V1 is intentionally browser-only' in web
assert 'app_checkpoint_a.c and the appliance UI remain' in web

# Scroll behavior: native page scroll + keyboard arrows + mouse wheel.
assert 'fxlabScrollable' in web
assert "e.key==='ArrowDown'" in web
assert "e.key==='ArrowUp'" in web
assert "labPage.addEventListener('wheel'" in web
assert "touch-action:pan-y" in web

# Keyboard zoom in FX Lab only.
assert "if(k==='i')" in web
assert "if(k==='o')" in web
assert "changeLabZoom(.1)" in web
assert "changeLabZoom(-.1)" in web
assert "ZOOM 100%" in web

# Sources/layers console.
assert 'SOURCES / LAYERS' in web
assert "name:'LIVE VIEW'" in web
assert "removable:false" in web
assert "layer.visible=!layer.visible" in web
assert "layer.opacity=" in web
assert '+ ADD SOURCE' in web

# Local file source stays browser-local and is constrained for first tests.
assert 'MAX_LOCAL_VIDEO_BYTES=20*1024*1024' in web
assert 'accept=\\\"video/mp4,.mp4\\\"' in web
assert 'URL.createObjectURL(file)' in web
assert 'URL.revokeObjectURL(layer.url)' in web
assert 'VIDEO COULD NOT BE DECODED' in web
assert 'LOCAL BROWSER MEMORY' in web

# Composition happens before the existing FX pipeline by overriding only the
# browser render source. No storage/NVS write path is introduced here.
assert 'renderRotatedSource=function' in web
assert 'drawContain(ctx,layer.media,w,h)' in web
assert 'SPIFFS' in web  # comment explicitly states files are not written there
assert 'nvs_set' not in web

# Reconstruct only the V1 source injection, not string literals in later handlers.
match = re.search(
    r'static const char fxlab_sources_web_injection\[\]\s*=\s*(.*?);\n\nstatic esp_err_t (?:checkpoint_page_get|PRISMA_INTENSITY_PAGE_GET)',
    web,
    re.S,
)
assert match
parts = []
for line in match.group(1).splitlines():
    line = line.strip()
    if line.startswith('"'):
        if line.endswith(';'):
            line = line[:-1]
        if line.endswith('"'):
            try:
                parts.append(ast.literal_eval(line))
            except Exception:
                pass
injection = ''.join(parts)
assert '<style>' in injection and '<script>' in injection
scripts = re.findall(r'<script>(.*?)</script>', injection, re.S)
assert len(scripts) == 1
if shutil.which('node'):
    with tempfile.NamedTemporaryFile('w', suffix='.js', encoding='utf-8', delete=False) as f:
        f.write(scripts[0])
        js_path = f.name
    try:
        subprocess.run(['node', '--check', js_path], check=True)
    finally:
        Path(js_path).unlink(missing_ok=True)

# Stable appliance preset/control code must still contain the known V3 values.
for token in (
    '/* 1G */ {{false,false,false,true,false}, { 0, 6,15,12,50,45}',
    '/* 2Y */ {{false,true,true,true,false},   { 0,33,42,52,50,62}',
    '/* 2R */ {{true,true,true,true,true},     { 8,43,50,64,50,68}',
    '/* 3G */ {{false,true,true,true,true},    { 0,54,58,74,50,75}',
    '/* 3Y */ {{false,true,true,true,true},    { 0,68,68,85,50,84}',
    '/* 3R */ {{false,true,true,true,true},    { 0,86,80,96,50,95}',
):
    assert token in app

print('FX Lab Sources V1 protected: scroll, I/O zoom, fixed Live View, local MP4 layers, opacity/visibility, JS syntax')
