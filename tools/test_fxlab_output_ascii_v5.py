#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
v5 = (root / 'main' / 'wifi_portal_layout_v5.c').read_text(encoding='utf-8')
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')

assert '"wifi_portal_layout_v5.c"' in cmake
assert '#include "wifi_portal_layout_v2.c"' in v5
assert 'buf == fxlab_layout_v2_injection' in v5

# OUTPUT is no longer over the preview: it is moved below the pipeline and defines canvas dimensions.
assert "outputBar.parentNode.removeChild(outputBar)" in v5
assert "insertBefore(outputRow,labPipeline.nextSibling)" in v5
assert "outputTitle.textContent='OUTPUT'" in v5
for token in ('240P','360P','480P','540P','720P','900P','1080P','1440P','2160P'):
    assert token in v5
for token in ('16:9','4:3','3:2','1:1','9:16','21:9'):
    assert token in v5
assert "v2AspectRatio=function" in v5

# Sources panel uses the full desktop preview height.
assert '.labSourcesPanelV2{position:relative;height:300px!important}' in v5

# MIRROR + MIX share the active row; MIX remains last because the fixed row is appended in order then removed.
assert 'var v5BaseRenderLabPipeline=renderLabPipeline' in v5
assert "while(fixed.firstChild)order.appendChild(fixed.firstChild)" in v5
assert 'fixed.remove()' in v5
assert '.labFixedRow{display:none!important}' in v5

# Detached output mirrors the already-processed final canvas and offers fullscreen in the second window.
assert "window.open('','PRISMA_FX_OUTPUT'" in v5
assert "dctx.drawImage(labCanvas,0,0)" in v5
assert "id=\\\"prismaOutputCanvas\\\"" in v5
assert "id=\\\"prismaOutputFs\\\"" in v5
assert "v5BaseRenderLabPreview=renderLabPreview" in v5

# Generator V5 adds ASCII while retaining procedural types and AMOUNT.
assert "['lines','LINES'],['shapes','SHAPES'],['simple','SIMPLE'],['ascii','ASCII']" in v5
assert "v5RangeRow(box,'AMOUNT',1,100" in v5
assert "v5SelectRow(extras,'ANIM'" in v5
assert "['face','FACE']" in v5
assert "['circle','CIRCLE']" in v5
assert "['shapes','SHAPES']" in v5
assert "['eye','EYE']" in v5
assert "['countdown','COUNTDOWN']" in v5
assert "v5RangeRow(extras,'SPEED'" in v5
assert "v5RangeRow(extras,'SCALE',50,200" in v5
assert 'function v5UnpackAscii' in v5
assert 'function v5DrawAscii' in v5

# Per-layer transforms and mobile fullscreen behavior remain carried forward.
assert "layer.rotation=(((Number(layer.rotation)||0)+90)%360+360)%360" in v5
assert 'var v5DesktopToggleLabFs=toggleLabFs' in v5
assert 'if(!v5UseMobilePseudoFull())return v5DesktopToggleLabFs()' in v5
assert '100dvw' in v5 and '100dvh' in v5

# No persistent storage writes were added.
assert 'nvs_set' not in v5
assert 'esp_partition_write' not in v5

# Syntax-check the injected JavaScript.
start = v5.index('static const char fxlab_v5_inner_patch[] =')
end = v5.index('\nstatic const char fxlab_v5_style_patch[] =', start)
chunk = v5[start:end]
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

print('FX Lab V5 protected: single-row MIX, output controls, detach window, taller sources and ASCII generator')
