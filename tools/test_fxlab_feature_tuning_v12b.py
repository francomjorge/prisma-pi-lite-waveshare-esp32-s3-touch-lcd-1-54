#!/usr/bin/env python3
from pathlib import Path
import ast
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
src=(root/'main'/'wifi_portal_record_v10.c').read_text(encoding='utf-8')
cmake=(root/'main'/'CMakeLists.txt').read_text(encoding='utf-8')

# This iteration must stay on the V11 architecture: no new wrapper/source layer.
assert 'wifi_portal_record_v11.c' in cmake
assert 'wifi_portal_record_v10.c' in cmake
assert 'wifi_portal_record_v12' not in cmake
assert 'fxlab_v12' not in cmake
assert 'V12B feature tuning' in src

# Existing generator families stay available in code; only weak GRID/TUNNEL are hidden from the selector.
for name in ('lines','shapes','simple','ascii','grid','wave','particles','tunnel','starfield'):
    assert name in src
selector=src.split("var opts=",1)[1].split(';',1)[0]
for marker in ("['lines','FLOW LINES']","['shapes','SHAPES']","['simple','SIMPLE']","['wave','WAVE']","['particles','PARTICLES']","['starfield','STARFIELD']","['ascii','ASCII']"):
    assert marker in selector, marker
for marker in ("['grid','GRID']","['tunnel','TUNNEL']"):
    assert marker not in selector, marker

# New continuous generators are feature-level additions inside the existing V10 generator path.
for marker in (
    "['polygon','POLYGON']",
    "['fire','FIRE / PLASMA']",
    "['orbit3d','ORBIT SPHERES']",
    "['flow','PARTICLE FLOW']",
    "['ribbon','RIBBON']",
    'function v10DrawPolygon',
    'function v10DrawFire',
    'function v10DrawOrbit3D',
    'function v10DrawFlow',
    'function v10DrawRibbon',
    "v5RangeRow(extras,'SIDES',3,12",
):
    assert marker in src, marker

# ASCII is curated but no existing embedded assets are deleted from V5.
assert "[['face','FACE'],['eye','EYE'],['flower','FLOWER']]" in src
assert 'function v10DrawFlowerAscii' in src
assert 'fps=.65+speed*3.35' in src

# Black is the stage/background only; output dimensions/aspect architecture is untouched.
assert ".labFrame,#labCanvas{background:#000!important}" in src
assert 'function v10ForceBlackStage' in src
assert "document.body.style.background='#000'" in src
assert "getElementById('prismaOutputCanvas')" in src

# Existing reorder is tuned, not replaced structurally: hysteresis + edge autoscroll.
assert 'function v10TuneLayerDrag' in src
assert 'ratio>.56' in src and 'ratio<.44' in src
assert 'list.scrollTop-=8' in src and 'list.scrollTop+=8' in src

# Existing canvas drag is tuned in-place: small threshold, pointer capture, absolute origin, RAF batching.
assert 'function v10BindCanvasDrag' in src
assert 'Math.hypot(dx,dy)<2.5' in src
assert 'setPointerCapture' in src
assert 'baseX:Number(layer.x)||0' in src
assert 'requestAnimationFrame' in src

# V10/V11 protections remain present: chroma, visibility/recording fast paths, deferred export.
for marker in ('v10ApplyChroma','chromaEnabled','v10NeedsAnimation','v10HasActiveFx','v10RecordWarmup','v10IdleExport','requestIdleCallback'):
    assert marker in src, marker
assert 'nvs_set' not in src
assert 'esp_partition_write' not in src

# Validate the exact embedded browser JavaScript.
start=src.index('static const char fxlab_record_v10_patch[] =')
end=src.index('\nesp_err_t __wrap_httpd_resp_send_chunk',start)
parts=[]
for line in src[start:end].splitlines()[1:]:
    s=line.strip()
    if s.startswith('"'):
        if s.endswith(';'): s=s[:-1]
        parts.append(ast.literal_eval(s))
js=''.join(parts)
with tempfile.NamedTemporaryFile('w',suffix='.js',delete=False,encoding='utf-8') as f:
    f.write(js); path=f.name
subprocess.run(['node','--check',path],check=True)
Path(path).unlink(missing_ok=True)

print('FX Lab V12B protected: V11 structure unchanged; generators, drag and black stage tuned only')
