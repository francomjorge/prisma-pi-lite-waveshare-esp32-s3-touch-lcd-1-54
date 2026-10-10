#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
v6 = (root / 'main' / 'wifi_portal_record_v6.c').read_text(encoding='utf-8')
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')

assert '"wifi_portal_layout_v5.c"' in cmake
assert '"wifi_portal_record_v6.c"' in cmake
assert '--wrap=httpd_resp_send_chunk' in cmake
assert '__wrap_httpd_resp_send_chunk' in v6
assert '__real_httpd_resp_send_chunk' in v6
assert 'v2RenderList();v2Paint();\\n})();' in v6

# Record exactly the final processed FX Lab canvas.
assert 'labCanvas.captureStream(30)' in v6
assert 'new MediaRecorder' in v6
assert 'V6_REC_LIMIT_MS=5000' in v6
assert 'setTimeout(v6StopRecording,V6_REC_LIMIT_MS)' in v6
assert "v6RecBtn.textContent='REC ●'" in v6
assert "v6RecBtn.textContent='STOP ■'" in v6

# Finalization must both download and add a browser-local video layer.
assert 'v6DownloadBlob(blob,name,url)' in v6
assert 'v6AddRecordedSource(blob,name,url)' in v6
assert "type:'video'" in v6
assert 'visible:false' in v6
assert 'opacity:1' in v6
assert "v2Layers.unshift(layer)" in v6
assert "SAVED + ADDED · EYE OFF" in v6

# Keep final-output animation alive while recording, even with only static sources.
assert 'return v6Recording||v6BaseHasVideo()' in v6
assert 'v2EnsureAnimation()' in v6

# Browser format negotiation; no appliance/storage mutation and no R-key takeover.
assert "video/webm;codecs=vp9" in v6
assert "video/webm;codecs=vp8" in v6
assert "video/mp4" in v6
assert 'nvs_set' not in v6
assert 'esp_partition_write' not in v6
assert "addEventListener('keydown'" not in v6

# Syntax-check injected JS.
start = v6.index('static const char fxlab_record_v6_patch[] =')
end = v6.index('\nesp_err_t __wrap_httpd_resp_send_chunk', start)
chunk = v6[start:end]
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
js = ''.join(parts)
with tempfile.NamedTemporaryFile('w', suffix='.js', delete=False, encoding='utf-8') as f:
    f.write(js)
    js_path = f.name
subprocess.run(['node', '--check', js_path], check=True)

print('FX Lab V6 protected: final-canvas REC, 5 s max, manual stop, download and hidden source')
