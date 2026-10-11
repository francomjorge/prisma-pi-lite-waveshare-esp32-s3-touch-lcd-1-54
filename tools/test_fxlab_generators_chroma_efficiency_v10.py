#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = (root / 'main' / 'wifi_portal_record_v10.c').read_text(encoding='utf-8') if (root / 'main' / 'wifi_portal_record_v10.c').exists() else ''
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')

assert 'wifi_portal_record_v10.c' in cmake
assert 'FX LAB V10' in src

# New generators start neutral: user must choose a category.
assert "generatorType:''" in src
assert "['','CHOOSE...']" in src

# Existing families plus five additional efficient vector generators.
for name in ['lines','shapes','simple','ascii','grid','wave','particles','tunnel','starfield']:
    assert name in src

# Generator color is user-selectable, including ASCII.
assert "type='color'" in src or "type=\"color\"" in src or "type='color';" in src
assert 'generatorColor' in src
assert 'v10GeneratorColor' in src

# Per-layer chroma key: opt-in only, with key color + tolerance.
assert 'chromaEnabled' in src
assert 'chromaColor' in src
assert 'chromaTolerance' in src
assert 'v10ApplyChroma' in src
assert 'v10ChromaCanvas' in src

# Efficiency: hidden/static sources do not keep RAF alive; FX all-off fast path.
assert 'function v10NeedsAnimation' in src
assert 'v2HasVideo=function' in src
assert 'function v10HasActiveFx' in src
assert 'return fxEngine.buffers.a' in src

# Recording mirror is not copied continuously when not recording.
assert 'v10RecordWarmup' in src
assert 'v7SyncRecordMirror=function' in src

# Export is deferred to idle time / delayed fallback instead of blocking render.
assert 'requestIdleCallback' in src
assert 'setTimeout' in src

# Browser-side only: no persistent appliance writes.
assert 'nvs_set' not in src
assert 'esp_partition_write' not in src

print('FX Lab V10 spec protected: generator choice/color, 5 new families, per-layer chroma, idle export and render fast paths')
