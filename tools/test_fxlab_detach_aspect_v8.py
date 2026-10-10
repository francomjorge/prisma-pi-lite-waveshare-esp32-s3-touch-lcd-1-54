#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
rec = (root / 'main' / 'wifi_portal_record_v6.c').read_text(encoding='utf-8')

assert 'FX LAB V8' in rec
assert 'function v8FitCanvas(canvas,aw,ah)' in rec
assert 'function v8FitLocalCanvas()' in rec
assert 'function v8FitDetachedCanvas()' in rec
assert 'function v8FitOutputSurfaces()' in rec
assert "canvas.style.width=dw+'px'" in rec
assert "canvas.style.height=dh+'px'" in rec
assert "canvas.style.aspectRatio=cw+' / '+ch" in rec
assert "aspectSelect.addEventListener('change'" in rec
assert "resolutionSelect.addEventListener('change'" in rec
assert "window.addEventListener('resize'" in rec
assert "new ResizeObserver" in rec

# DETACH owns the visible monitor while open; local canvas stays black/hidden.
assert '.labFrame.v7Detached #labCanvas{visibility:hidden!important}' in rec
assert "body:hover .hud{opacity:.62;pointer-events:auto}" in rec
assert "fs.textContent='⛶'" in rec
assert "meta.title='Output resolution'" in rec
assert "v5DetachedWindow.innerWidth" in rec
assert "v5DetachedWindow.innerHeight" in rec
assert "fullscreenchange" in rec

# Recording still follows the exact output surface and imports hidden source.
assert 'function v7TargetCanvas(){return v7DetachedCanvas()||labCanvas;}' in rec
assert 'v7RecMirror.captureStream(30)' in rec
assert 'requestFrame' in rec
assert 'visible:false' in rec
assert 'recordedBlob:blob' in rec
assert 'v2Layers.unshift(layer)' in rec

# Preserve currently working automatic download without adding another UI control.
assert 'function v8AutoDownload(url,name)' in rec
assert 'v8AutoDownload(url,name)' in rec

# Syntax-check injected JS.
start = rec.index('static const char fxlab_record_v6_patch[] =')
end = rec.index('\nesp_err_t __wrap_httpd_resp_send_chunk', start)
chunk = rec[start:end]
parts = []
for line in chunk.splitlines()[1:]:
    s = line.strip()
    if s.startswith('"'):
        if s.endswith(';'):
            s = s[:-1]
        parts.append(ast.literal_eval(s))
js = ''.join(parts)
with tempfile.NamedTemporaryFile('w', suffix='.js', delete=False, encoding='utf-8') as f:
    f.write(js)
    js_path = f.name
subprocess.run(['node', '--check', js_path], check=True)

print('FX Lab V8 protected: aspect-correct local/detached canvas, detached monitor ownership, exact recording + auto-download')
