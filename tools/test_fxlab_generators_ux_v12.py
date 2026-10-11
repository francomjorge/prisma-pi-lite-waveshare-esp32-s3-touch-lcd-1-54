#!/usr/bin/env python3
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
js = (root / 'main' / 'web' / 'fxlab_v12.js').read_text(encoding='utf-8')
wrap = (root / 'main' / 'wifi_portal_record_v12.c').read_text(encoding='utf-8')
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')

assert 'wifi_portal_record_v12.c' in cmake
assert 'wifi_portal_record_v11.c' in cmake
assert 'v11_wrapped_httpd_resp_send_chunk' in cmake
assert 'wifi_portal_record_v11_nested.c' in cmake
assert 'web/fxlab_v12.js' in cmake
assert '_binary_fxlab_v12_js_start' in wrap
assert '_binary_fxlab_v12_js_end' in wrap
assert '_binary_web_fxlab_v12_js_start' not in wrap
assert 'v11_wrapped_httpd_resp_send_chunk' in wrap

# Curated continuous generator set; weak GRID/TUNNEL controls are removed.
for marker in [
    "['polygon','POLYGON']",
    "['fire','FIRE / PLASMA']",
    "['orbit3d','ORBIT SPHERES']",
    "['flow','PARTICLE FLOW']",
    "['ribbon','RIBBON']",
    'function v12Noise1',
    'function v12Noise2',
    'function v12DrawPolygon',
    'function v12DrawFire',
    'function v12DrawOrbit3D',
    'function v12DrawFlow',
    'function v12DrawRibbon',
    "v12Range(extras,'SIDES',3,12",
]:
    assert marker in js, marker
assert "['grid','GRID']" not in js
assert "['tunnel','TUNNEL']" not in js
assert "generatorType:''" in js

# ASCII is intentionally curated and includes a procedural flower.
assert "[['face','FACE'],['eye','EYE'],['flower','FLOWER']]" in js
assert 'function v12DrawFlowerAscii' in js
assert "['countdown','COUNTDOWN']" not in js

# Black stage is enforced for local, fullscreen and detached output.
assert "background='#000'" in js
assert "#labCanvas{background:#000!important" in js
assert 'function v12BlackDetached' in js

# Precision layer reorder: threshold, hysteresis, pointer capture and autoscroll.
assert 'Math.hypot(dx,dy)<4.5' in js
assert 'ratio<.44' in js and 'ratio>.56' in js
assert 'setPointerCapture' in js
assert 'requestAnimationFrame(v12AutoScrollStep)' in js
assert 'touch-action:none' in js

# Canvas dragging is absolute, batched to RAF, and uses a small hitbox pad.
assert 'function v12CanvasPointHit' in js
assert 'padx=9*' in js
assert 'Math.hypot(dx,dy)<2' in js
assert 'requestAnimationFrame(v12CanvasPaint)' in js
assert 'stopImmediatePropagation' in js

# Remains browser-side; no storage/NVS mutation.
assert 'nvs_set' not in js
assert 'esp_partition_write' not in js

subprocess.run(['node', '--check', str(root / 'main' / 'web' / 'fxlab_v12.js')], check=True)
print('FX Lab V12 protected: continuous generators, curated ASCII, black stage, precision drag, embedded script symbols')
