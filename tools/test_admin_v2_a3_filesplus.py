#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
cmake=(root/'main/CMakeLists.txt').read_text()
api=(root/'main/files_plus_api.c').read_text()
html=(root/'main/web/admin.html').read_text()
css=(root/'main/web/admin.css').read_text()

assert 'files_plus_api.c' in cmake
assert '/api/filesplus' in api
assert '/media/*' in api
assert '/api/files/sample' in api
assert 'parse_wav' in api and 'sample_rate' in api and 'duration_ms' in api
assert 'parse_png' in api and 'parse_jpeg' in api
assert 'CFG_SAMPLE' in api and 'nvs_set_str' in api
assert 'FILES+' in html
assert 'mediaGrid' in html and 'imagePreview' in html and 'audioPreview' in html
assert 'SELECT SAMPLE' in html and 'REPLACE' in html and 'DOWNLOAD' in html and 'DELETE' in html
assert '/api/filesplus' in html and '/media/' in html and '/api/files/sample' in html
assert 'A3 FILES+' in html
assert '.mediaGrid' in css and '.mediaCard' in css and '.sampleBadge' in css
print('Admin V2 A3 Files+ OK')
