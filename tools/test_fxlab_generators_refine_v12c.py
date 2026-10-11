#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
v11=(root/'main'/'wifi_portal_record_v11.c').read_text(encoding='utf-8')
v10=(root/'main'/'wifi_portal_record_v10.c').read_text(encoding='utf-8')
cmake=(root/'main'/'CMakeLists.txt').read_text(encoding='utf-8')

# V12C stays inside the existing V11/V10 architecture: no new runtime source layer.
assert 'wifi_portal_record_v11.c' in cmake
assert 'wifi_portal_record_v10.c' in cmake
assert 'wifi_portal_record_v12' not in cmake
assert 'fxlab_v12' not in cmake
assert 'V12C generator refinement' in v11

# Exact visible generator catalog requested for the stable creative base.
selector=v11.split("var opts=",1)[1].split(';',1)[0]
expected=(
    "['straight','STRAIGHT LINE']",
    "['lines','FLOW LINE']",
    "['shapes','SHAPES']",
    "['simple','SIMPLE']",
    "['complex','COMPLEX']",
    "['polygon','POLYGON']",
    "['ascii','ASCII']",
    "['fire','FIRE / PLASMA']",
)
for marker in expected:
    assert marker in selector, marker
for hidden in (
    "['grid','GRID']","['tunnel','TUNNEL']","['orbit3d','ORBIT SPHERES']",
    "['flow','PARTICLE FLOW']","['ribbon','RIBBON']","['wave','WAVE']",
    "['particles','PARTICLES']","['starfield','STARFIELD']",
):
    assert hidden not in selector, hidden

# Straight line and complex pseudo-3D are additive generators; existing V10 engines remain available internally.
assert 'function v12cDrawStraight' in v11
assert 'function v12cDrawComplex' in v11
assert "if(ty==='straight')" in v11
assert "if(ty==='complex')" in v11
assert 'v12cBaseDrawGenerator=v10DrawGenerator' in v11
for engine in ('grid','tunnel','orbit3d','flow','ribbon','wave','particles','starfield'):
    assert engine in v10

# COMPLEX exposes motion/depth/size/twist and uses perspective projection plus cross-links.
for marker in ("'DEPTH'","'SIZE'","'TWIST'",'focal=2.6','all=[]','ctx.closePath()'):
    assert marker in v11, marker

# Polygon still has explicit side count and ASCII remains curated.
assert "v5RangeRow(extras,'SIDES',3,12" in v11
assert "[['face','FACE'],['eye','EYE'],['flower','FLOWER']]" in v11

# FX Lab R becomes exact 30-degree steps without touching the older V5 implementation or Live View R.
assert 'function v12cBindRotate30' in v11
assert 'v12cRotateStart' in v11
assert 'start+30' in v11
assert 'pg!==4' in v11

# V11 performance protections remain present.
for marker in ('willReadFrequently:true','v11ChromaRegion','v11DetachedActive','v11ReleaseMirrorLater','requestIdleCallback'):
    assert marker in v11, marker
assert 'nvs_set' not in v11
assert 'esp_partition_write' not in v11

# Syntax-check the exact embedded browser JavaScript.
start=v11.index('static const char fxlab_record_v11_patch[] =')
end=v11.index('\nesp_err_t __wrap_httpd_resp_send_chunk',start)
parts=[]
for line in v11[start:end].splitlines()[1:]:
    s=line.strip()
    if s.startswith('"'):
        if s.endswith(';'): s=s[:-1]
        parts.append(ast.literal_eval(s))
js=''.join(parts)
with tempfile.NamedTemporaryFile('w',suffix='.js',delete=False,encoding='utf-8') as f:
    f.write(js); path=f.name
subprocess.run(['node','--check',path],check=True)
Path(path).unlink(missing_ok=True)

print('FX Lab V12C protected: refined 8-generator catalog, COMPLEX pseudo-3D, exact 30-degree FX Lab R rotation')
