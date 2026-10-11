#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
v9 = (root / 'main' / 'wifi_portal_record_v9.c').read_text(encoding='utf-8')
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')

assert '"wifi_portal_record_v9.c"' in cmake
assert '"wifi_portal_record_v6.c"' in cmake  # compatibility marker + included V8 source
assert '#include "wifi_portal_record_v6.c"' in v9
assert '#define __wrap_httpd_resp_send_chunk v8_wrapped_httpd_resp_send_chunk' in v9
assert 'FX LAB V9' in v9

# Reset sits beside DETACH and performs browser-local cleanup without appliance writes.
assert "v9ResetBtn.textContent='RESET'" in v9
assert "detachBtn.parentNode.insertBefore(v9ResetBtn,detachBtn.nextSibling)" in v9
assert 'function v9CleanupReset()' in v9
assert 'function v9ReleaseLayer(layer)' in v9
assert 'URL.revokeObjectURL(layer.url)' in v9
assert 'v2Layers.length=0' in v9
assert "v2Layers.push(live)" in v9
assert "v2Aspect='source';v2Resolution='source'" in v9
assert "localStorage.removeItem('prismaFxLabAspectV1')" in v9
assert "localStorage.removeItem('prismaFxLabResolutionV1')" in v9
assert 'v9StopRecorderForReset()' in v9
assert 'v5DetachedWindow.close()' in v9
assert 'nvs_set' not in v9
assert 'esp_partition_write' not in v9

# Detached window gets the same I/O zoom semantics (50%..300%), baked into its canvas.
assert 'function v9BindDetachedZoom()' in v9
assert 'function v9ChangeDetachedZoom(delta)' in v9
assert "if(k==='i')" in v9
assert "else if(k==='o')" in v9
assert 'Math.max(.5,Math.min(3,v9DetachZoom))' in v9
assert 'ctx.scale(v9DetachZoom,v9DetachZoom)' in v9
assert 'v7SyncRecordMirror()' in v9

# Recording runtime is extended to 15 seconds while preserving the V8 recorder.
assert 'V7_REC_LIMIT_MS=15000' in v9
assert "v7RecClock.textContent='00.0 / 15.0'" in v9
assert "+' / 15.0'" in v9
assert "MAX 15.0 S" in v9

# Syntax-check the V9 injected JavaScript.
start = v9.index('static const char fxlab_record_v9_patch[] =')
end = v9.index('\nesp_err_t __wrap_httpd_resp_send_chunk', start)
chunk = v9[start:end]
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

print('FX Lab V9 protected: RESET cleanup, detached I/O zoom, exact recorded zoom surface, 15 s recording')
