#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
rec = (root / 'main' / 'wifi_portal_record_v6.c').read_text(encoding='utf-8')
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')

assert 'wifi_portal_record_v6.c' in cmake
assert 'target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=httpd_resp_send_chunk")' in cmake
assert 'FX LAB V7 PART 1' in rec
assert 'V7_REC_LIMIT_MS=5000' in rec
assert "document.createElement('canvas')" in rec
assert 'v7RecMirror' in rec
assert 'function v7SyncRecordMirror()' in rec
assert 'function v7DetachedCanvas()' in rec
assert 'function v7TargetCanvas()' in rec
assert "v7RecMirror.captureStream(30)" in rec
assert 'videoBitsPerSecond' in rec
assert 'requestFrame' in rec
assert "visible:false" in rec
assert "recordedBlob:blob" in rec
assert "v2Layers.unshift(layer)" in rec
assert 'v6DownloadBlob' not in rec
assert '.labFrame.v7Detached #labCanvas{visibility:hidden!important}' in rec
assert '.labFrame.v7Detached .labFsBtn{display:none!important}' in rec
assert "body:hover .hud{opacity:.62;pointer-events:auto}" in rec
assert "fs.textContent='⛶'" in rec
assert "v5DetachedWindow.closed" in rec
assert "labFrame.classList.remove('v7Detached')" in rec

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

print('FX Lab V7 part 1 protected: detached output mirror, black local frame, exact canvas recording, hidden imported source')
