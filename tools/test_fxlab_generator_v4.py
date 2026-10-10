#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
v4 = (root / 'main' / 'wifi_portal_layout_v4.c').read_text(encoding='utf-8')
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')

assert '"wifi_portal_layout_v4.c"' in cmake
assert '#include "wifi_portal_layout_v2.c"' in v4
assert 'buf == fxlab_layout_v2_injection' in v4

# Add-source menu offers file video and procedural generator.
assert "id=\\\"labAddVideoV4\\\">VIDEO FILE" in v4
assert "id=\\\"labAddGeneratorV4\\\">GENERATOR" in v4
assert 'function v4AddGenerator()' in v4
assert "type:'generator'" in v4
assert "name:v4GeneratorName()" in v4

# Generator types and always-present AMOUNT control.
assert '<option value=\\\"lines\\\">LINES</option>' in v4
assert '<option value=\\\"shapes\\\">SHAPES</option>' in v4
assert '<option value=\\\"simple\\\">SIMPLE</option>' in v4
assert "v4RangeRow(box,'AMOUNT',1,100" in v4

# Type-specific controls.
assert "v4RangeRow(extras,'THICK'" in v4
assert "v4RangeRow(extras,'VAR'" in v4
assert "v4RangeRow(extras,'SPREAD'" in v4
assert "v4RangeRow(extras,'SPEED'" in v4

# Procedural source participates in normal layer transform and global FX pipeline.
assert "else if(layer.type==='generator'){v4DrawGenerator(ctx,layer,g);}" in v4
assert 'performance.now()' in v4
assert "if(v2Layers[i].type==='generator')return true" in v4
assert 'rotation:0' in v4
assert "layer.rotation=(((Number(layer.rotation)||0)+90)%360+360)%360" in v4

# V3 fullscreen behavior is carried forward unchanged in intent.
assert 'var v4DesktopToggleLabFs=toggleLabFs' in v4
assert 'if(!v4UseMobilePseudoFull())return v4DesktopToggleLabFs()' in v4
assert '100dvw' in v4 and '100dvh' in v4
assert "window.addEventListener('orientationchange'" in v4

# Syntax-check the injected JavaScript.
start = v4.index('static const char fxlab_v4_inner_patch[] =')
end = v4.index('\nstatic const char fxlab_v4_style_patch[] =', start)
chunk = v4[start:end]
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

print('FX Lab V4 protected: generator sources, lines/shapes/simple, dynamic controls, animation, rotation and mobile fullscreen')
