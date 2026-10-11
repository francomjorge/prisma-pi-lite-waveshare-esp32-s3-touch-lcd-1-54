#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / 'main' / 'wifi_portal_record_v11.c').read_text(encoding='utf-8')
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')

assert 'wifi_portal_record_v11.c' in cmake
assert 'wifi_portal_record_v10.c' in cmake
assert 'v10_wrapped_httpd_resp_send_chunk' in cmake
assert 'wifi_portal_record_v10_nested.c' in cmake
assert 'FX LAB V11' in src
assert 'extern esp_err_t __real_httpd_resp_send_chunk' in src

# Chroma uses a readback-optimized canvas and only scans the active layer ROI.
assert 'willReadFrequently:true' in src
assert 'function v11ClampRegion' in src
assert 'v11ChromaRegion' in src
assert 'ctx.getImageData(r.x,r.y,r.w,r.h)' in src
assert 'v2LayerGeometry(layer,w,h,rot)' in src

# Hidden browser tabs stop local-only rendering/video work, while DETACH remains authoritative.
assert 'document.hidden' in src
assert 'function v11DetachedActive' in src
assert 'v11PauseBackgroundVideos' in src
assert "document.addEventListener('visibilitychange'" in src
assert 'v10SyncAllPlayback();v2EnsureAnimation();v2Paint();' in src

# Buffers are lazy and released after use instead of permanently pinning full-frame memory.
assert 'v10ChromaCanvas.width=1' in src
assert 'v10ChromaCanvas.height=1' in src
assert 'v7RecMirror.width=1' in src
assert 'v7RecMirror.height=1' in src
assert 'v11ReleaseMirrorLater' in src

# Export stays asynchronous/deferred.
assert 'requestIdleCallback' in src
assert 'timeout:1200' in src
assert 'setTimeout(once,180)' in src

# V11 remains browser-side only.
assert 'nvs_set' not in src
assert 'esp_partition_write' not in src

# Syntax-check injected JS.
start = src.index('static const char fxlab_record_v11_patch[] =')
end = src.index('\nesp_err_t __wrap_httpd_resp_send_chunk', start)
chunk = src[start:end]
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

print('FX Lab V11 protected: chroma ROI, visibility-aware scheduler, lazy buffer release, deferred export')
